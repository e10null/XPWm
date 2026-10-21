#!/usr/bin/env python3
"""Decode original XP notification-area resources at their native 16px size."""
import argparse
import io
import json
import struct
import tempfile
from pathlib import Path
from PIL import Image
from xp_assets import archive_members, run, sha
from prepare_luna import rgba_dib, png


def native_icon(icon):
    """Select the richest original 16px frame, never the first legacy frame."""
    depth = max(e.bpp for e in icon.ico.entry if e.dim == (16, 16))
    return icon.ico.getimage((16, 16), depth), depth


def prepare(source, iso, output):
    output.mkdir(parents=True, exist_ok=True)
    records = json.loads((source / 'index.json').read_text())['resources']
    ledger = []

    def save(name, image, provenance):
        target = output / (name + '.png')
        image.convert('RGBA').save(target)
        ledger.append(dict(role=name, file=target.name, width=16, height=16,
                           png_sha256=sha(target.read_bytes()), **provenance))

    with tempfile.TemporaryDirectory(prefix='xpwm-tray-') as temporary:
        work = Path(temporary)
        member = 'I386/STOBJECT.DL_'
        compressed = run(['7z', 'x', '-so', str(iso), member])
        cab = work / 'STOBJECT.DL_'
        cab.write_bytes(compressed)
        library_name = next(n for n in archive_members(cab) if n.lower() == 'stobject.dll')
        library = work / library_name
        library.write_bytes(run(['7z', 'x', '-so', str(cab), library_name]))
        run(['wrestool', '-x', '-t', '14', '-o', str(work), str(library)])
        roles = {'volume': 230, 'volume-muted': 231}
        for role, ident in roles.items():
            ico = work / f'{library_name}_14_{ident}.ico'
            # Explicit depth: Pillow otherwise selects the first 16px entry,
            # which can be the legacy yellow 16-color speaker.
            icon = Image.open(ico)
            image, depth = native_icon(icon)
            save(role, image, dict(source=member, library=library_name, resource_id=ident,
                                  type='GROUP_ICON', bit_depth=depth, ico_sha256=sha(ico.read_bytes()),
                                  library_sha256=sha(library.read_bytes()), container_sha256=sha(compressed)))

    # STObject 316..325 are mouse icons, not batteries. The actual native
    # battery artwork lives in BATMETER bitmap 301 (ten 16px frames).
    with tempfile.TemporaryDirectory(prefix='xpwm-battery-') as temporary:
        work = Path(temporary)
        member = 'I386/BATMETER.DL_'
        compressed = run(['7z', 'x', '-so', str(iso), member])
        cab = work / 'BATMETER.DL_'
        cab.write_bytes(compressed)
        name = next(n for n in archive_members(cab) if n.lower() == 'batmeter.dll')
        library = work / name
        library.write_bytes(run(['7z', 'x', '-so', str(cab), name]))
        raw = run(['wrestool', '-x', '--raw', '-t', '2', '-n', '301', str(library)])
        bitmap = run(['wrestool', '-x', '-t', '2', '-n', '301', str(library)])
        strip = Image.open(io.BytesIO(bitmap)).convert('RGBA')
        if strip.size != (161, 16):
            raise ValueError('Unexpected BATMETER 16px strip dimensions')
        roles = {'power-battery': 0, 'power-medium': 1, 'power-low': 2,
                 'power-unknown': 4, 'power-ac': 6, 'power-charging': 8}
        for role, frame in roles.items():
            image = strip.crop((frame*16, 0, frame*16+16, 16))
            image.putdata([(r,g,b,0 if (r,g,b)==(255,0,255) else 255)
                           for r,g,b,a in (image.get_flattened_data() if hasattr(image, 'get_flattened_data') else image.getdata())])
            save(role, image, dict(source=member, library=name, resource_id=301,
                                  type='BITMAP', frame=frame, color_key='#ff00ff',
                                  raw_sha256=sha(raw), library_sha256=sha(library.read_bytes()),
                                  container_sha256=sha(compressed)))

    # Keep the actual Win32 popup template as a geometry reference, rather
    # than deriving its dimensions from a screenshot or a generic GTK dialog.
    with tempfile.TemporaryDirectory(prefix='xpwm-volume-') as temporary:
        work = Path(temporary)
        member = 'I386/SNDVOL32.EX_'
        compressed = run(['7z', 'x', '-so', str(iso), member])
        cab = work / 'SNDVOL32.EX_'
        cab.write_bytes(compressed)
        name = next(n for n in archive_members(cab) if n.lower() == 'sndvol32.exe')
        library = work / name
        library.write_bytes(run(['7z', 'x', '-so', str(cab), name]))
        raw = run(['wrestool', '-x', '--raw', '-t', '5', '-n', '106', str(library)])
        style, extended_style, count, x, y, width, height = struct.unpack_from('<IIHhhhh', raw)
        layout = dict(source=member, resource_type='DIALOG', resource_id=106,
                      raw_sha256=sha(raw), library_sha256=sha(library.read_bytes()),
                      container_sha256=sha(compressed), width_dlu=width, height_dlu=height,
                      control_count=count, xp_base_units=[6, 13], client_pixels=[75, 130])
        (output / 'volume-popup.json').write_text(json.dumps(layout, indent=2)+'\n')

    roles = {'network-connected': 190, 'network-transmit': 191,
             'network-receive': 192, 'network-idle': 193,
             'network-disconnected': 195, 'network-limited': 196, 'display-brightness': 100}
    for role, ident in roles.items():
        group = next(r for r in records if r['type'] == 'GROUP_ICON' and
                     Path(r['source']).name.lower() == ('desk.cpl' if role == 'display-brightness' else 'netshell.dll') and r['id'] == ident)
        component = max((c for c in group['components'] if c['width'] == 16 and
                         c['height'] == 16 and c.get('file')), key=lambda c: c['bit_depth'])
        record = next(r for r in records if r['file'] == component['file'])
        raw = (source / record['file']).read_bytes()
        if sha(raw) != record['sha256']:
            raise ValueError('Network resource hash mismatch')
        ico = struct.pack('<HHHBBBBHHII', 0, 1, 1, 16, 16, 0, 0, 1,
                          component['bit_depth'], len(raw), 22) + raw
        image = Image.open(io.BytesIO(ico)).convert('RGBA')
        save(role, image, dict(source=group['source'], resource_id=ident, type='GROUP_ICON',
                              component=record['id'], raw_file=record['file'], raw_sha256=sha(raw)))
    theme = next(t for t in json.loads((source / 'theme-references.json').read_text())
                 if t['resource_id'] == 'NORMALBLUE_INI')
    prop = next(p for p in theme['properties'] if p['section'] == 'TrackBar.ThumbVert'
                and p['property'].lower() == 'imagefile')
    record = next(r for r in records if r['file'] == prop['bitmap_file'])
    raw = (source / record['file']).read_bytes()
    if sha(raw) != record['sha256']:
        raise ValueError('Volume slider resource hash mismatch')
    width, height, rows = rgba_dib(raw, True)
    target = output / 'volume-thumb.png'
    target.write_bytes(png(width, height, rows))
    ledger.append(dict(role='volume-thumb', file=target.name, width=width, height=height,
                       frames=5, frame_height=height//5, source=record['source'],
                       resource_id=record['id'], raw_file=record['file'],
                       raw_sha256=sha(raw), png_sha256=sha(target.read_bytes())))
    (output / 'index.json').write_text(json.dumps(ledger, indent=2) + '\n')
    print(f'Prepared {len(ledger)} original XP tray icons')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('assets/local/xp-sp3'))
    parser.add_argument('--iso', type=Path, default=Path('en_windows_xp_professional_with_service_pack_3_x86_cd_x14-80428.iso'))
    parser.add_argument('--output', type=Path, default=Path('assets/local/xp-tray'))
    args = parser.parse_args()
    prepare(args.source, args.iso, args.output)
