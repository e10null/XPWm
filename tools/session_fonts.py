#!/usr/bin/env python3
"""Session-local XP font defaults; never modifies host font configuration."""
import configparser
import os
from pathlib import Path
import sys
from xml.sax.saxutils import escape


def font_config(directory):
    aliases = [('sans-serif', 'Tahoma')]
    return ('<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
            '<fontconfig><include ignore_missing="yes">/etc/fonts/fonts.conf</include>'
            f'<dir>{escape(str(directory))}</dir>' + ''.join(
                f'<match target="pattern"><test name="family" qual="any"><string>{generic}</string></test>'
                f'<edit name="family" mode="prepend" binding="strong"><string>{face}</string></edit></match>'
                for generic, face in aliases) + '</fontconfig>\n')


def write_config(fonts, destination):
    destination = Path(destination)
    contents = font_config(Path(fonts).resolve())
    destination.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    if not destination.exists() or destination.read_text() != contents:
        temporary = destination.with_name(f'{destination.name}.{os.getpid()}.tmp')
        try:
            temporary.write_text(contents)
            temporary.replace(destination)
        finally:
            temporary.unlink(missing_ok=True)


def font_dpi():
    settings = configparser.ConfigParser(interpolation=None)
    path = Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home() / '.config'))) / 'xpwm/appearance.ini'
    try:
        settings.read(path)
        dpi = settings.getint('Appearance', 'FontDPI', fallback=96)
    except (ValueError, configparser.Error):
        return 96
    return dpi if dpi in (96, 120, 144) else 96


if __name__ == '__main__':
    if sys.argv[1:] == ['--dpi']:
        print(font_dpi())
    else:
        write_config(*sys.argv[1:])
