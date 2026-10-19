#!/usr/bin/env python3
"""Measure an isolated, software-rendered XPWm session using Linux /proc."""
import argparse
import json
import os
from pathlib import Path
import signal
import statistics
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
HZ = os.sysconf("SC_CLK_TCK")

def processes(root_pid):
    records = {}
    for path in Path('/proc').iterdir():
        if not path.name.isdigit():
            continue
        try:
            name, tail = (path / 'stat').read_text().rsplit(') ', 1)
            fields = tail.split()
            records[int(path.name)] = {
                'name': name.split('(', 1)[1], 'ppid': int(fields[1]),
                'cpu_ticks': sum(int(fields[i]) for i in (11, 12, 13, 14)),
                'start': int(fields[19]), 'path': path,
            }
        except (OSError, ValueError, IndexError):
            pass
    selected = {root_pid}
    while True:
        found = selected | {pid for pid, r in records.items() if r['ppid'] in selected}
        if selected == found:
            break
        selected = found
    return {pid: records[pid] for pid in selected if pid in records}

def sample(root_pid):
    rows = []
    for pid, record in processes(root_pid).items():
        try:
            fields = {}
            for line in (record['path'] / 'smaps_rollup').read_text().splitlines():
                key, _, value = line.partition(':')
                if key in ('Pss', 'Rss', 'Private_Clean', 'Private_Dirty', 'SwapPss'):
                    fields[key] = int(value.split()[0])
            rows.append({'pid': pid, 'name': record['name'], 'cpu_ticks': record['cpu_ticks'],
                         'pss_kib': fields['Pss'], 'rss_kib': fields['Rss'],
                         'private_kib': fields.get('Private_Clean', 0) + fields.get('Private_Dirty', 0),
                         'swap_pss_kib': fields.get('SwapPss', 0)})
        except FileNotFoundError:
            pass
    return rows

def run(bare=False, seconds=30, files=0):
    with tempfile.TemporaryDirectory(prefix='xpwm-profile-') as directory:
        temp = Path(directory)
        (temp / 'config').mkdir()
        desktop = temp / 'Desktop'
        desktop.mkdir()
        for i in range(files):
            (desktop / f'Document {i:03d}.txt').write_text('profiling fixture\n')
        (temp / 'config/user-dirs.dirs').write_text(f'XDG_DESKTOP_DIR="{desktop}"\n')
        env = os.environ.copy()
        for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'DBUS_SESSION_BUS_ADDRESS'):
            env.pop(key, None)
        env.update(XDG_RUNTIME_DIR=directory, XDG_CONFIG_HOME=str(temp/'config'),
                   XDG_DATA_HOME=str(temp/'data'), XDG_CACHE_HOME=str(temp/'cache'),
                   XPWM_PROJECT_ROOT=str(ROOT), XPWM_PROFILE_READY=str(temp/'ready.json'),
                   WLR_BACKENDS='headless', WLR_HEADLESS_OUTPUTS='1', WLR_RENDERER='pixman',
                   XPWM_SKIP_LOGON='1', XPWM_SOUND_MUTE='1', XPWM_DEMO='0')
        if bare:
            env['XPWM_PROFILE_BARE'] = '1'
        else:
            env.pop('XPWM_PROFILE_BARE', None)
        with (temp/'session.log').open('w') as log:
            start = time.monotonic()
            process = subprocess.Popen(['dbus-run-session', '--', 'sh', '-c',
                                        '. tools/session-theme.sh; exec ./compositor/session-profile'],
                                       cwd=ROOT, env=env, stdout=log, stderr=log, start_new_session=True)
            known = {}
            try:
                deadline = start+20
                while not (temp/'ready.json').exists():
                    if process.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError((temp/'session.log').read_text()[-6000:])
                    time.sleep(.01)
                ready = json.loads((temp/'ready.json').read_text())
                ready['launcher_to_ready_ms'] = (time.monotonic()-start)*1000
                time.sleep(5)  # Exclude first-paint/font/service initialization from idle.
                first = processes(process.pid)
                known.update({pid: r['start'] for pid, r in first.items()})
                cpu_start = sum(r['cpu_ticks'] for r in first.values())
                cpu_names_start = {}
                for r in first.values():
                    cpu_names_start[r['name']] = cpu_names_start.get(r['name'], 0)+r['cpu_ticks']
                measurements = []
                idle_start = time.monotonic()
                for _ in range(seconds):
                    measurements.append(sample(process.pid))
                    time.sleep(1)
                elapsed = time.monotonic()-idle_start
                last = processes(process.pid)
                known.update({pid: r['start'] for pid, r in last.items()})
                cpu_end = sum(r['cpu_ticks'] for r in last.values())
                names = {}
                for rows in measurements:
                    for row in rows:
                        names.setdefault(row['name'], []).append(row)
                per_process = []
                for name, rows in names.items():
                    ticks = sum(r['cpu_ticks'] for r in last.values() if r['name'] == name)-cpu_names_start.get(name, 0)
                    per_process.append({'name': name,
                                        'median_pss_mib': statistics.median(r['pss_kib'] for r in rows)/1024,
                                        'median_rss_mib': statistics.median(r['rss_kib'] for r in rows)/1024,
                                        'idle_cpu_percent_one_core_including_reaped_children': max(0, ticks)/HZ/elapsed*100})
                totals = [sum(r['pss_kib'] for r in rows)/1024 for rows in measurements]
                return dict(mode='bare' if bare else 'shell', desktop_files=files, **ready,
                            idle_seconds=elapsed, idle_cpu_percent_one_core=(cpu_end-cpu_start)/HZ/elapsed*100,
                            median_pss_mib=statistics.median(totals), min_pss_mib=min(totals), max_pss_mib=max(totals),
                            median_rss_mib=statistics.median(sum(r['rss_kib'] for r in rows)/1024 for rows in measurements),
                            processes=sorted(per_process, key=lambda r: -r['median_pss_mib']))
            finally:
                known.update({pid: r['start'] for pid, r in processes(process.pid).items()})
                # Only stop this fixture's processes, checking PID identity first.
                for pid in reversed(list(known)):
                    try:
                        stat = Path(f'/proc/{pid}/stat').read_text().rsplit(') ', 1)[1].split()
                        if int(stat[19]) == known[pid]:
                            os.kill(pid, signal.SIGTERM)
                    except (OSError, IndexError):
                        pass
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=int, default=30)
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--files', type=int, default=0)
    parser.add_argument('--bare', action='store_true')
    args = parser.parse_args()
    if args.seconds < 1 or args.runs < 1 or args.files < 0:
        parser.error('seconds/runs must be positive and files nonnegative')
    results = []
    for index in range(args.runs):
        result = run(args.bare, args.seconds if index == 0 else 1, args.files)
        results.append(result)
        print(json.dumps(result), flush=True)
    args.output.write_text(json.dumps({'method': '1920x1080 headless/pixman; private XDG and D-Bus; mapped shell buffers; 5s settle; PSS proportional memory; CPU includes reaped helpers; no user applications or interactive logon', 'runs': results}, indent=2)+'\n')
