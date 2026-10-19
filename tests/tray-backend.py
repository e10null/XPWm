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
if Path(sys.argv[0]).name == 'wpctl':
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
