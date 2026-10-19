#!/usr/bin/env python3
"""Build local rendering assets from the verified NORMALBLUE_INI resource."""
import argparse
import json
import struct
import zlib
from pathlib import Path
from xp_assets import archive_members, dib_info, run, sha, write

ROLES = {
    'start-header': ('StartPanel.UserPane', 'ImageFile'),
    'start-programs': ('StartPanel.ProgList', 'ImageFile'),
    'start-more': ('StartPanel.MorePrograms', 'ImageFile'),
    'start-places': ('StartPanel.PlacesList', 'ImageFile'),
    'start-footer': ('StartPanel.Logoff', 'ImageFile'),
    'caption': ('Window.Caption', 'ImageFile'),
    'frame-left': ('Window.FrameLeft', 'ImageFile'),
    'frame-right': ('Window.FrameRight', 'ImageFile'),
    'frame-bottom': ('Window.FrameBottom', 'ImageFile'),
    'push-button': ('button.pushbutton', 'ImageFile'),
    'caption-button': ('Window.MaxButton', 'ImageFile'),
    'close-button': ('Window.CloseButton', 'ImageFile'),
    # ImageSelectType=size: 13px is the largest glyph fitting the 17px content box.
    'min-glyph': ('Window.MinButton', 'ImageFile3'),
    'max-glyph': ('Window.MaxButton', 'ImageFile3'),
    'restore-glyph': ('Window.RestoreButton', 'ImageFile3'),
    'close-glyph': ('Window.CloseButton', 'ImageFile3'),
    'start': ('Start::Button', 'ImageFile'),
    'taskbar': ('TaskBar.BackgroundBottom', 'ImageFile'),
    'task-button': ('TaskBand::Toolbar.Button', 'ImageFile'),
    'tray': ('TrayNotifyHoriz::TrayNotify.Background', 'ImageFile'),
}
FONTS = ['TREBUC', 'TREBUCBD', 'TAHOMA', 'TAHOMABD', 'FRAMD', 'FRAMDIT']


def decode_rle8(data, width, height):
    """Decode Windows BI_RLE8 indices, including absolute runs and deltas."""
    stride = (width + 3) & ~3
    pixels = bytearray(stride * height)
    x = y = pos = 0
    while pos + 2 <= len(data):
        count, value = data[pos:pos + 2]
        pos += 2
        if count:
            if y >= height or x + count > width:
                raise ValueError('RLE8 run outside bitmap')
            start = y * stride + x
            pixels[start:start + count] = bytes([value]) * count
            x += count
        elif value == 0:
            x = 0
            y += 1
            if y > height:
                raise ValueError('RLE8 row outside bitmap')
        elif value == 1:
            return bytes(pixels)
        elif value == 2:
            if pos + 2 > len(data):
                raise ValueError('Truncated RLE8 delta')
            dx, dy = data[pos:pos + 2]
            pos += 2
            x += dx
            y += dy
            if x > width or y >= height:
                raise ValueError('RLE8 delta outside bitmap')
        else:
            padded = value + (value & 1)
            if pos + padded > len(data):
                raise ValueError('Truncated RLE8 absolute run')
            if y >= height or x + value > width:
                raise ValueError('RLE8 absolute run outside bitmap')
            start = y * stride + x
            pixels[start:start + value] = data[pos:pos + value]
            x += value
            pos += padded
    raise ValueError('Missing RLE8 end marker')


def rgba_dib(data, transparent=False, color_key=(255, 0, 255)):
    info = dib_info(data)
    if not info or info['bit_depth'] not in (8, 24, 32) or not (
            info['compression'] == 0 or (info['compression'] == 1 and info['bit_depth'] == 8)):
        raise ValueError('Only uncompressed 8/24/32-bit and RLE8 DIBs are supported')
    w, h, depth, offset = (info[k] for k in ('width', 'height', 'bit_depth', 'pixel_offset'))
    stride = ((w * depth + 31) // 32) * 4
    if info['compression'] == 1:
        data = data[:offset] + decode_rle8(data[offset:], w, h)
    if offset + h * stride > len(data):
        raise ValueError('Truncated bitmap pixels')
    palette_at = struct.unpack_from('<I', data)[0]
    rows = []
    for y in range(h):
        sy = h - 1 - y if info['stored_height'] > 0 else y
        row = bytearray()
        for x in range(w):
            pos = offset + sy * stride + x * (depth // 8)
            if depth == 8:
                entry = palette_at + data[pos] * 4
                if entry + 4 > offset:
                    raise ValueError('Bitmap palette index out of bounds')
                b, g, r, unused = struct.unpack_from('<4B', data, entry)
                a = 255
            else:
                b, g, r = data[pos:pos + 3]
                a = data[pos + 3] if depth == 32 else 255
            if depth != 32 and transparent and (r, g, b) == color_key:
                a = 0
            row.extend((r, g, b, a))
        rows.append(bytes(row))
    # These Luna 32-bit strips contain straight-alpha values (RGB can exceed A).
    # PNG also uses straight alpha; Cairo performs premultiplication when loading.
    return w, h, rows


def png(w, h, rows):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(b''.join(b'\0' + row for row in rows))) + chunk(b'IEND', b''))


def prepare(source, output, iso=None):
    if output.exists():
        raise ValueError('Choose a new output directory; local theme data is not overwritten')
    index = json.loads((source / 'index.json').read_text())
    themes = json.loads((source / 'theme-references.json').read_text())
    theme = next(t for t in themes if t['resource_id'] == 'NORMALBLUE_INI')
    records = {r['file']: r for r in index['resources']}
    properties = {}
    for prop in theme['properties']:
        properties.setdefault(prop['section'], {})[prop['property'].lower()] = prop
    output.mkdir(parents=True)
    manifest, provenance = [], []
    for role, (section, key) in ROLES.items():
        props = properties[section]
        prop = props[key.lower()]
        record = records[prop['bitmap_file']]
        raw = (source / record['file']).read_bytes()
        if sha(raw) != record['sha256']:
            raise ValueError(f'Original hash mismatch: {record["file"]}')
        glyph = role.endswith('-glyph')
        transparent = glyph or props.get('transparent', {}).get('value', 'false').lower() == 'true'
        color_key = tuple(int(c) for c in props.get('transparentcolor', {}).get('value', '255 0 255').split())
        w, h, rows = rgba_dib(raw, transparent, color_key)
        count = int(props.get('imagecount', {}).get('value', '1'))
        if h % count:
            raise ValueError(f'Incomplete state strip: {role}')
        name = f"images/{record['id']}.png"
        if not (output / name).exists():
            write(output, name, png(w, h, rows))
        margins = '0;0;0;0' if glyph else props.get('sizingmargins', {}).get('value', '0,0,0,0').replace(',', ';').replace(' ', '')
        tile = not glyph and props.get('sizingtype', {}).get('value', '').lower() == 'tile'
        manifest.append(f'[{role}]\nfile={name}\ncount={count}\nmargins={margins};\ntile={str(tile).lower()}\n')
        provenance.append(dict(role=role, section=section, property=key, source=record['source'],
                               resource_id=record['id'], language=record['language'],
                               raw_file=record['file'], sha256=record['sha256'], file=name,
                               alpha='straight BGRA bytes' if record['bit_depth'] == 32 else
                               f'color key {color_key}' if transparent else 'opaque',
                               theme_text=theme['raw_file'], line=prop['line']))
    if iso:
        for font in FONTS:
            member = f'I386/{font}.TT_'
            compressed = run(['7z', 'x', '-so', str(iso), member])
            saved = write(output, f'sources/{member}', compressed)
            for name in archive_members(output / saved):
                data = run(['7z', 'x', '-so', str(output / saved), name])
                path = write(output, f'fonts/{name}', data)
                provenance.append(dict(role='font', source=member, member=name, file=path,
                                       sha256=sha(data), container_sha256=sha(compressed)))
    write(output, 'theme.ini', ('\n'.join(manifest)).encode())
    write(output, 'provenance.json', (json.dumps(provenance, indent=2) + '\n').encode())
    print(f'Prepared {len(ROLES)} Luna image roles in {output}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('extracted', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--iso', type=Path, help='Optionally extract the six required font faces locally')
    args = parser.parse_args()
    prepare(args.extracted, args.output, args.iso)
