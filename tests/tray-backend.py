#!/usr/bin/python3
"""Private tray device fixtures, selected by executable basename."""
import os
import sys
from pathlib import Path

root = Path(os.environ['XDG_RUNTIME_DIR'])
args = sys.argv[1:]
if Path(sys.argv[0]).name == 'pavucontrol':
    (root / 'advanced-opened').write_text('1')
    sys.exit(0)
if Path(sys.argv[0]).name == 'pactl':
    assert args == ['subscribe']
    descriptor = os.open(root / 'audio-events', os.O_RDWR)
    (root / 'audio-listening').write_text('1')
    with os.fdopen(descriptor) as events:
        for event in events:
            print(event.rstrip(), flush=True)
    sys.exit(0)
if Path(sys.argv[0]).name in ('brightnessctl', 'gdbus'):
    if (root / 'brightness-fail').exists():
        sys.exit(1)
    if Path(sys.argv[0]).name == 'gdbus':
        assert args == ['call', '--system', '--dest', 'org.freedesktop.login1',
                        '--object-path', '/org/freedesktop/login1/session/self',
                        '--method', 'org.freedesktop.login1.Session.SetBrightness',
                        'backlight', 'mock', args[-1]]
        value = int(args[-1]) // 10
    else:
        assert args[:4] == ['--class=backlight', '--device', 'mock', 'set']
        value = int(args[4].rstrip('%'))
    assert 1 <= value <= 100
    (root / 'brightness').write_text(str(value))
    (root / 'backlight/mock/brightness').write_text(str(value * 10))
elif Path(sys.argv[0]).name == 'wpctl':
    if (root / 'audio-fail').exists():
        sys.exit(1)
    if args[0] == 'get-volume':
        muted = ' [MUTED]' if (root / 'mute').read_text() == '1' else ''
        print(f"Volume: {int((root / 'volume').read_text()) / 100:.2f}{muted}")
    elif args[0] == 'set-volume':
        (root / 'volume').write_text(args[2].rstrip('%'))
    elif args[0] == 'set-mute':
        (root / 'mute').write_text(args[2])
    else:
        sys.exit(2)
else:
    if (root / 'network-fail').exists():
        sys.exit(1)
    if 'radio' in args:
        (root / 'wifi').write_text('enabled' if args[-1] == 'on' else 'disabled')
    elif args[-2:] == ['general', 'status']:
        print(f"connected:{(root / 'connectivity').read_text()}:{(root / 'wifi').read_text()}")
    else:
        sys.exit(2)
