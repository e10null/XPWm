#!/usr/bin/env python3
"""Prepare XPWm's private runtime assets from a user-supplied XP SP3 x86 ISO."""
import argparse
import importlib.util
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUTPUTS = ('xp-sp3', 'luna-blue-complete', 'xp-icons', 'xp-bitmaps',
           'xp-tray', 'sounds-xp', 'share', 'import-report.json')


def preflight(iso, output):
    iso = iso.resolve(strict=True)
    if not iso.is_file():
        raise ValueError('Input must be a Windows XP SP3 x86 ISO file.')
    local = output.resolve() / 'assets/local'
    if local.is_symlink() or local.parent.is_symlink():
        raise ValueError('Asset directories must not be symbolic links.')
    conflicts = [str(local / name) for name in OUTPUTS
                 if (local / name).exists() or (local / name).is_symlink()]
    if conflicts:
        raise ValueError('Existing assets are preserved; choose a new --output directory.\n'
                         + '\n'.join(conflicts))
    missing = [name for name in ('7z', 'wrestool') if not shutil.which(name)]
    if importlib.util.find_spec('PIL') is None:
        missing.append('Python Pillow')
    if missing:
        raise ValueError('Missing dependencies: ' + ', '.join(missing)
                         + '. On Arch: sudo pacman -S --needed 7zip icoutils python-pillow')
    return iso, local


def import_iso(iso, output):
    iso, local = preflight(iso, output)
    local.mkdir(parents=True, exist_ok=True)
    report_path = local / 'import-report.json'
    report = dict(schema_version=1, source=str(iso), state='incomplete', completed=[])
    # Exclusive creation prevents concurrent imports into the same asset root.
    with report_path.open('x') as stream:
        json.dump(report, stream, indent=2)

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')

    def step(name, action):
        print(f'\n== {name} ==', flush=True)
        action()
        report['completed'].append(name)
        save()

    def script(name, *args):
        subprocess.run([sys.executable, str(ROOT / 'tools' / name),
                        *(str(arg) for arg in args)], check=True)

    source = local / 'xp-sp3'
    try:
        step('Extract source resources', lambda: script('xp_assets.py', iso,
             '--output', source, '--profile', 'all'))
        step('Verify source resources', lambda: script('verify_assets.py', source))
        from prepare_luna import prepare as luna
        from prepare_icons import prepare as icons
        from prepare_shell_bitmaps import prepare as bitmaps
        from prepare_tray import prepare as tray
        from prepare_app_theme import prepare as app_theme
        from PIL import Image
        step('Luna artwork and fonts', lambda: luna(source, local / 'luna-blue-complete', iso))
        step('Shell icons', lambda: icons(source, local / 'xp-icons'))
        step('Shell and logon bitmaps', lambda: bitmaps(source, local / 'xp-bitmaps'))
        step('Notification area artwork', lambda: tray(source, iso, local / 'xp-tray'))
        step('Event sounds', lambda: script('prepare_sounds.py', iso, '--output', local / 'sounds-xp'))

        def wallpaper():
            index = json.loads((source / 'index.json').read_text())
            record = next((r for r in index['files']
                           if Path(r['source']).name.lower() == 'bliss.jpg'), None)
            if record is None:
                raise ValueError('The ISO does not contain the expected Bliss wallpaper.')
            original = source / record['file']
            target = original.with_name('bliss.png')
            with Image.open(original) as image:
                image.convert('RGB').save(target)
            from xp_assets import sha
            report['wallpaper'] = dict(source=record['file'], sha256=record['sha256'],
                                      derivative=str(target.relative_to(local)),
                                      derivative_sha256=sha(target.read_bytes()))

        step('Bliss wallpaper', wallpaper)
        step('GTK application theme and icon aliases', lambda: app_theme(
             source, local / 'share', icon_source=local / 'xp-icons',
             font_dir=local / 'luna-blue-complete/fonts'))
        report['source_sha256'] = json.loads((source / 'index.json').read_text())['origin']['sha256']
        report['state'] = 'complete'
        save()
    except (Exception, KeyboardInterrupt) as error:
        report['state'] = 'failed'
        report['error'] = str(error) or type(error).__name__
        save()
        raise
    print(f'\nReady: {local}\nSource ISO unchanged. No system files installed.')
    print('Launch from the XPWm project directory with:')
    print(f'XPWM_ASSET_ROOT={shlex.quote(str(local.parent.parent))} sh tools/start-session.sh tty')
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso', type=Path, help='Local Windows XP SP3 x86 ISO; never downloaded or executed')
    parser.add_argument('--output', type=Path, default=ROOT,
                        help='Asset root containing assets/local (default: XPWm project root); existing assets refused')
    args = parser.parse_args(argv)
    try:
        import_iso(args.iso, args.output)
    except (OSError, ValueError, RuntimeError, StopIteration, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Import failed: {str(error) or type(error).__name__}\nPartial output is kept for diagnosis; retry with a new --output directory.\n')
    except KeyboardInterrupt:
        parser.exit(130, 'Import interrupted; partial output is preserved.\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
