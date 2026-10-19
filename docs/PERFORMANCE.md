# Local session measurements — 2026-10-18

These measurements use the current XPWm compositor and normal `shell/session.sh`,
with an isolated D-Bus session and private XDG configuration, cache, data and
Desktop directories. The observer maps a 1920×1080 headless output with wlroots'
software renderer. No user applications are launched; interactive Welcome/logon
and startup audio are skipped.

Machine: Intel Core i5-10400F, 12 logical CPUs, approximately 32 GiB installed RAM.
GTK 3.24.52; wlroots 0.18.3. This is a local test, not a real GPU/TTY performance
claim or a comparison against other desktop environments.

## Results

| Session | Median proportional RAM | Summed RSS | Idle CPU, one core | Launch to mapped surfaces |
| --- | ---: | ---: | ---: | ---: |
| Bare compositor and private D-Bus | 12.3 MiB | 26.4 MiB | No CPU ticks observed | 0.041 s, output ready |
| Desktop/taskbar, empty Desktop folder | 49.7 MiB | 120.6 MiB | 0.75% | 0.141 s |
| Desktop/taskbar, 40 desktop files | 58.2 MiB | 138.7 MiB | 0.78% | 0.141 s |

RAM is PSS from Linux `/proc/PID/smaps_rollup`, summed across the session's
process tree, including helper processes. PSS divides shared pages among their
users. The RSS column deliberately shows the larger, double-counted total that
can result from adding ordinary per-process memory columns. GPU/device memory,
applications and the host OS are outside these totals.

Each idle sample runs for about 30 seconds after a five-second settling period.
CPU includes user/system ticks and reaped child-process ticks, so the cost of
short-lived tray commands is retained. 100% means one logical CPU fully busy.
The bare sample's zero means no ticks were detected at this resolution, not a
promise of literally zero work.

Empty-desktop PSS stayed between 49.65 and 49.73 MiB during its 30-second sample.
Approximate component contributions were desktop 22.9 MiB, compositor 15.6 MiB,
taskbar 9.7 MiB, and launcher/session/D-Bus processes 1.5 MiB. Most idle CPU was
in the taskbar and its helpers (about 0.68% of one core); the desktop recorded no
CPU ticks. The taskbar redraws every second and polls audio/network status every
five seconds. Those are concrete remaining opportunities to reduce idle work.

With forty files, desktop PSS was about 26.3 MiB. A sandboxed toolkit image-decoder
helper (`glycin-image-rs` and its wrapper) added approximately 5 MiB. This accounts
for much of the difference beyond the extra icon tiles and file model.

Startup is measured until both desktop and taskbar have mapped their initial
buffers. It includes the private D-Bus/shell launch, but not boot, authentication,
a physical scanout or the completion of every asynchronous folder/service query.
Three repeated isolated empty-desktop starts measured about 141.4–141.5 ms from
the profiling launcher. Filesystem caches were not flushed; each fixture had a
fresh private user cache. One-second samples after the extra startup runs are
recorded by the tool, but are not used for the idle CPU results above.

The development checkout is about 891 MiB on disk, dominated by approximately
589 MiB of XP ISO media and 292 MiB of local extracted/generated resources. Those
files are not all loaded into RAM. The core compositor, desktop and taskbar
binaries together are about 1.1 MiB, with debug information included; system
libraries and assets are separate.

## Repeat

From the project root, after building XPWm:

```sh
make -C compositor session-profile
python3 tools/profile-session.py --output /tmp/xpwm-shell.json
python3 tools/profile-session.py --bare --runs 1 --output /tmp/xpwm-bare.json
python3 tools/profile-session.py --files 40 --runs 1 --output /tmp/xpwm-files.json
```

The tool reports per-process figures and cleans up its private fixture processes
and directories. Run scenarios sequentially: concurrent copies would share library
pages and change the proportional memory accounting. Recheck on a real GPU session
before treating these numbers as hardware performance guarantees.
