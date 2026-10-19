#!/usr/bin/env python3
"""Generate a session-local GTK Luna theme from verified private XP resources."""
import argparse
import json
import shutil
from pathlib import Path
from xml.sax.saxutils import escape
from prepare_luna import rgba_dib, png
from xp_assets import sha

ROOT = Path(__file__).resolve().parents[1]
ROLES = {
    'button': ('button.pushbutton', 'imagefile'),
    'check': ('button.checkbox', 'imagefile1'),
    'radio': ('button.radiobutton', 'imagefile1'),
    'combo': ('Combobox.DropDownButton', 'imagefile'),
    'tab': ('Tab.TabItem', 'imagefile'),
    'scroll-arrows': ('ScrollBar.ArrowBtn', 'imagefile'),
    'slider-v': ('TrackBar.ThumbVert', 'imagefile'),
    'slider-track-v': ('TrackBar.TrackVert', 'imagefile'),
    'progress-track-v': ('Progress.BarVert', 'imagefile'),
    'progress-v': ('Progress.ChunkVert', 'imagefile'),
    'scroll-v': ('Scrollbar.ThumbBtnVert', 'imagefile'),
    'scroll-h': ('Scrollbar.ThumbBtnHorz', 'imagefile'),
    'track-v': ('Scrollbar.LowerTrackVert', 'imagefile'),
    'track-h': ('Scrollbar.LowerTrackHorz', 'imagefile'),
    'slider': ('TrackBar.Thumb', 'imagefile'),
    'slider-track': ('TrackBar.Track', 'imagefile'),
    'progress-track': ('Progress.Bar', 'imagefile'),
    'progress': ('Progress.Chunk', 'imagefile'),
}


def prepare_icons(source, share, icon_source=None):
    icon_source = icon_source if icon_source is not None else ROOT / 'assets/local/xp-icons'
    if not (icon_source / 'index.json').exists():
        return
    records = {r['file']: r for r in json.loads((icon_source / 'index.json').read_text())}
    roles = {
        'places': {'folder': 4, 'folder-open': 5, 'user-home': 235, 'folder-documents': 235,
                   'folder-pictures': 236, 'folder-music': 237, 'folder-videos': 238,
                   'user-desktop': 35, 'user-trash': 32, 'user-trash-full': 33,
                   'network-workgroup': 18},
        'devices': {'computer': 16, 'drive-harddisk': 9, 'drive-optical': 12,
                    'media-optical': 12, 'printer': 17},
        'mimetypes': {'application-x-executable': 3, 'text-x-generic': 1,
                      'application-octet-stream': 1, 'image-x-generic': 2},
        'actions': {'system-shutdown': 28, 'system-log-out': 35, 'system-search': 23,
                    'help-browser': 24, 'document-open': 4},
    }
    output = share / 'icons/XPWm-Luna'
    directories, ledger = [], []
    for size in (16, 32, 48):
        for context, names in roles.items():
            directory = f'{size}x{size}/{context}'
            directories.append((directory, size, context))
            (output / directory).mkdir(parents=True, exist_ok=True)
            for name, resource in names.items():
                path = f'shell32.dll/GROUP_ICON/{resource}/1033-{size}.png'
                if path not in records or not (icon_source / path).exists():
                    continue
                record = records[path]
                if sha((source / record['raw_file']).read_bytes()) != record['sha256']:
                    raise ValueError(f'Icon source hash mismatch: {path}')
                target = f'{directory}/{name}.png'
                shutil.copyfile(icon_source / path, output / target)
                ledger.append(dict(record, derivative=target))
    index = '[Icon Theme]\nName=XPWm Luna\nInherits=Adwaita,hicolor\nDirectories='
    index += ','.join(d[0] for d in directories)+'\n'
    for directory, size, context in directories:
        index += f'\n[{directory}]\nSize={size}\nType=Fixed\nContext={context.title()}\n'
    (output / 'index.theme').write_text(index)
    (output / 'provenance.json').write_text(json.dumps(ledger, indent=2)+'\n')


def prepare(source, share, *, icon_source=None, font_dir=None):
    records = {r['file']: r for r in json.loads((source / 'index.json').read_text())['resources']}
    theme = next(t for t in json.loads((source / 'theme-references.json').read_text())
                 if t['resource_id'] == 'NORMALBLUE_INI')
    props = {}
    for prop in theme['properties']:
        props.setdefault(prop['section'].lower(), {})[prop['property'].lower()] = prop
    output = share / 'themes/XPWm-Luna'
    # Validate and convert all source strips before writing the generated theme.
    generated, ledger, slices = {}, [], {}
    for role, (section, key) in ROLES.items():
        properties = props[section.lower()]
        prop = properties[key]
        record = records[prop['bitmap_file']]
        raw = (source / record['file']).read_bytes()
        if sha(raw) != record['sha256']:
            raise ValueError(f'Source hash mismatch: {record["file"]}')
        transparent = properties.get('transparent', {}).get('value', 'false').lower() == 'true'
        w, h, rows = rgba_dib(raw, transparent)
        count = int(properties.get('imagecount', {}).get('value', '1'))
        if h % count:
            raise ValueError(f'Incomplete strip: {role}')
        frame_height = h // count
        margins = [int(n.strip()) for n in properties.get('sizingmargins', {}).get('value', '0,0,0,0').split(',')]
        left, right, top, bottom = margins
        slices[role] = f'{top} {right} {bottom} {left}'
        for state in range(count):
            path = f'assets/{role}-{state}.png'
            generated[path] = png(w, frame_height, rows[state*frame_height:(state+1)*frame_height])
            ledger.append(dict(role=role, state=state, file=path, width=w, height=frame_height,
                               raw_file=record['file'], sha256=record['sha256'],
                               source=record['source'], resource_id=record['id'], language=record['language'],
                               theme_text=theme['raw_file'], line=prop['line']))
    css = []
    def border(selector, role, state):
        css.append(f'{selector} {{ border-image: url("assets/{role}-{state}.png") {slices[role]} fill / {" ".join(n+"px" for n in slices[role].split())} stretch; }}')
    # Default is a normal-state image, not an override for hover/press/disabled.
    for selector, state in [('button', 0), ('button.default', 4),
                            ('button:hover', 1), ('button:active', 2),
                            ('button:checked', 2), ('button:disabled', 3)]:
        border(selector, 'button', state)
    css.append('button { background-image:none; background-color:transparent; }')
    css.append('button.flat, toolbar button, headerbar button { border-image:none; }')
    for role, node, states in [('check', 'checkbutton check', 12), ('radio', 'radiobutton radio', 8)]:
        for i in range(states):
            value = ':checked' if i//4 == 1 else ':indeterminate' if i//4 == 2 else ':not(:checked):not(:indeterminate)'
            variant = ['', ':hover', ':active', ':disabled'][i%4]
            css.append(f'{node}{value}{variant} {{ -gtk-icon-source:none; border:0; background-image:url("assets/{role}-{i}.png"); background-repeat:no-repeat; background-position:center; background-size:13px 13px; }}')
    for state, suffix in enumerate(('', ':hover', ':active', ':disabled')):
        border('combobox button'+suffix+' arrow', 'combo', state)
        for direction, offset in [('vertical button.up', 0), ('vertical button.down', 4),
                                  ('horizontal button.left', 8), ('horizontal button.right', 12)]:
            border('scrollbar.'+direction+suffix, 'scroll-arrows', offset+state)
        border('scrollbar.vertical slider'+suffix, 'scroll-v', state)
        border('scrollbar.horizontal slider'+suffix, 'scroll-h', state)
    for state, suffix in [(0, ''), (1, ':hover'), (2, ':checked'), (3, ':disabled')]:
        border('notebook > header tab'+suffix, 'tab', state)
    border('scrollbar.vertical trough', 'track-v', 0)
    border('scrollbar.horizontal trough', 'track-h', 0)
    # Trackbar frames: normal, hot, pressed, focused, disabled.
    for suffix, state in [('', 0), (':focus', 3), (':hover', 1), (':active', 2), (':disabled', 4)]:
        border('scale.horizontal'+suffix+' slider', 'slider', state)
        border('scale.vertical'+suffix+' slider', 'slider-v', state)
    border('scale.horizontal trough', 'slider-track', 0)
    border('scale.vertical trough', 'slider-track-v', 0)
    border('progressbar trough', 'progress-track', 0)
    border('progressbar.vertical trough', 'progress-track-v', 0)
    css.append('progressbar progress { background-color:transparent; background-image:url("assets/progress-0.png"); background-repeat:repeat; }')
    css.append('progressbar.vertical progress { background-image:url("assets/progress-v-0.png"); min-width:14px; min-height:0; }')
    css.append('progressbar.vertical trough { min-width:16px; min-height:0; }')
    # XP stores button backgrounds separately from their centered arrow glyphs.
    css.append('combobox arrow, scrollbar button { background-image:none; background-color:transparent; }')
    css.append('scrollbar slider, scale slider { background-image:none; background-color:transparent; }')
    css.append('notebook > header tab { background-image:none; background-color:transparent; }')

    for version in ('gtk-3.0', 'gtk-2.0'):
        (output / version / 'assets').mkdir(parents=True, exist_ok=True)
        for name, data in generated.items():
            (output / version / name).write_bytes(data)
    for asset in (ROOT / 'themes/XPWm-Luna/gtk-3.0/assets').glob('*.svg'):
        shutil.copyfile(asset, output / 'gtk-3.0/assets' / asset.name)
    shutil.copyfile(ROOT / 'themes/XPWm-Luna/index.theme', output / 'index.theme')
    base = (ROOT / 'themes/XPWm-Luna/gtk-3.0/gtk.css').read_text()
    (output / 'gtk-3.0/gtk.css').write_text(base+'\n/* Original, source-indexed Luna control states. */\n'+'\n'.join(css)+'\n')
    gtk2 = (ROOT / 'themes/XPWm-Luna/gtk-2.0/gtkrc').read_text()
    gtk2 += '\ngtk-icon-theme-name = \"XPWm-Luna\"\n'
    gtk2 += '\nstyle "xp-buttons" = "xp-default" {\n engine "pixmap" {\n'
    for i, state in enumerate(('NORMAL', 'PRELIGHT', 'ACTIVE', 'INSENSITIVE')):
        gtk2 += f'  image {{ function = BOX detail = "button" state = {state} file = "assets/button-{i}.png" border = {{8,8,9,9}} stretch = TRUE }}\n'
    gtk2 += ' }\n}\nclass "GtkButton" style "xp-buttons"\n'
    (output / 'gtk-2.0/gtkrc').write_text(gtk2)
    (output / 'provenance.json').write_text(json.dumps(ledger, indent=2)+'\n')
    applications = share / 'applications'
    applications.mkdir(parents=True, exist_ok=True)
    executable = str(ROOT / 'shell/xpwm-theme-preview').replace('\\', '\\\\').replace('"', '\\"')
    (applications / 'xpwm-theme-preview.desktop').write_text(
        '[Desktop Entry]\nType=Application\nName=Luna Theme Preview\n'
        f'Exec="{executable}"\nIcon=preferences-desktop-theme\nCategories=Settings;\nTerminal=false\n')
    # Fonts are process/session-local, not installed into the host's font directories.
    font_dir = font_dir if font_dir is not None else ROOT / 'assets/local/luna-blue-complete/fonts'
    if font_dir.is_dir():
        config = share / 'fontconfig/fonts.conf'
        config.parent.mkdir(parents=True, exist_ok=True)
        config.write_text('<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">\n'
                          '<fontconfig><include ignore_missing="yes">/etc/fonts/fonts.conf</include>'
                          f'<dir>{escape(str(font_dir))}</dir></fontconfig>\n')
    prepare_icons(source, share, icon_source)
    print(f'Prepared GTK Luna theme ({len(generated)} native control frames) in {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, nargs='?', default=ROOT/'assets/local/xp-sp3')
    parser.add_argument('--share', type=Path, default=ROOT/'assets/local/share')
    args = parser.parse_args()
    prepare(args.source, args.share)
