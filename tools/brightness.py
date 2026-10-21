#!/usr/bin/env python3
"""Adjust the display backlight without requiring brightnessctl or root."""
import argparse
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

BACKLIGHT_ROOT = Path('/sys/class/backlight')


def adjust(direction):
    if shutil.which('brightnessctl'):
        subprocess.run(['brightnessctl', '--class=backlight', 'set',
                        '+10%' if direction == 'up' else '10%-'], check=True)
        return
    devices = sorted(BACKLIGHT_ROOT.iterdir())
    if not devices:
        raise ValueError('No display backlight was found.')
    device = devices[0]
    current = int((device / 'brightness').read_text())
    maximum = int((device / 'max_brightness').read_text())
    if maximum <= 0:
        raise ValueError('Invalid maximum backlight brightness.')
    step = max(1, math.ceil(maximum / 10))
    target = min(maximum, max(max(1, math.ceil(maximum / 100)),
                             current + (step if direction == 'up' else -step)))
    path = '/org/freedesktop/login1/session/self'
    # Compositor launchers call setsid(); keep the original login session ID.
    if os.environ.get('XDG_SESSION_ID'):
        result = subprocess.run(['gdbus', 'call', '--system', '--dest',
                                 'org.freedesktop.login1', '--object-path',
                                 '/org/freedesktop/login1', '--method',
                                 'org.freedesktop.login1.Manager.GetSession',
                                 os.environ['XDG_SESSION_ID']],
                                check=True, capture_output=True, text=True)
        match = re.fullmatch(r"\(objectpath '([^']+)',\)", result.stdout.strip())
        if not match:
            raise ValueError('Unable to resolve the login session for backlight control.')
        path = match[1]
    subprocess.run(['gdbus', 'call', '--system', '--dest', 'org.freedesktop.login1',
                    '--object-path', path, '--method',
                    'org.freedesktop.login1.Session.SetBrightness',
                    'backlight', device.name, str(target)], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('direction', choices=('up', 'down'))
    args = parser.parse_args()
    try:
        adjust(args.direction)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'XPWm brightness: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
