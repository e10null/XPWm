#!/usr/bin/env python3
"""Extract data resources only. Never execute or decompile Windows binaries.

PE directory layout: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
Outputs are local, immutable originals plus explicitly linked previews.
"""
import argparse
import hashlib
import html
import json
import shutil
import struct
import subprocess
import sys
from pathlib import Path, PurePosixPath
from urllib.parse import quote

TYPES = {1: 'CURSOR', 2: 'BITMAP', 3: 'ICON', 4: 'MENU', 5: 'DIALOG',
         6: 'STRING', 7: 'FONTDIR', 8: 'FONT', 9: 'ACCELERATOR', 10: 'RCDATA',
         11: 'MESSAGETABLE', 12: 'GROUP_CURSOR', 14: 'GROUP_ICON', 16: 'VERSION',
         17: 'DLGINCLUDE', 19: 'PLUGPLAY', 20: 'VXD', 21: 'ANICURSOR',
         22: 'ANIICON', 23: 'HTML', 24: 'MANIFEST'}
CORE = {'luna.ms_', 'luna.msstyles', 'luna.mst', 'blue_ss.dl_', 'home_ss.dl_',
        'metal_ss.dl_', 'class_ss.dl_', 'shellstyle.dll', 'explorer.exe',
        'shell32.dll', 'user32.dll', 'comctl32.dll', 'msgina.dll', 'logonui.exe',
        'netshell.dll', 'shdocvw.dll'}
CORE |= {n[:-1] + '_' for n in tuple(CORE) if n.endswith(('.exe', '.dll'))}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(value):
    p = PurePosixPath(value)
    if not value or '\\' in value or p.is_absolute() or '..' in p.parts or ':' in value:
        raise ValueError(f'Unsafe source path: {value!r}')
    return p


def ident(value):
    # Named numeric IDs must never collide with integer IDs or escape a directory.
    return ('id-' if isinstance(value, int) else 'name-') + quote(str(value), safe='')


def run(args):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if result.returncode:
        raise ValueError(f'{args[0]} failed ({result.returncode}): {result.stderr.decode(errors="replace")}')
    return result.stdout


def archive_members(path):
    listing = run(['7z', 'l', '-slt', str(path)]).decode('utf-8', errors='strict')
    if '----------\n' not in listing:
        raise ValueError(f'Cannot read archive member listing: {path}')
    members = []
    for block in listing.split('----------\n', 1)[1].strip().split('\n\n'):
        fields = dict(line.split(' = ', 1) for line in block.splitlines() if ' = ' in line)
        name = fields.get('Path')
        if not name or fields.get('Folder') == '+' or fields.get('Attributes', '').startswith('D'):
            continue
        safe_path(name)
        if int(fields.get('Size', 0)) > 64 * 1024 * 1024:
            continue
        members.append(name)
    return members


def selected(name, profile):
    p = PurePosixPath(name.lower())
    if profile == 'luna':
        return p.name in {'luna.ms_', 'luna.msstyles', 'luna.mst', 'blue_ss.dl_',
                          'home_ss.dl_', 'metal_ss.dl_', 'class_ss.dl_', 'shellstyle.dll'}
    return (p.name in CORE or p.suffix in {'.cpl', '.cp_', '.cur', '.cu_', '.ani', '.an_',
                                         '.wav', '.wa_', '.jpg', '.jp_', '.bmp', '.bm_'})


class PE:
    """Bounds-checked traversal of PE32/PE32+ resource directories."""
    def __init__(self, data):
        self.data = data
        if data[:2] != b'MZ':
            raise ValueError('Not a PE file')
        pe = self.unpack('<I', 0x3c)[0]
        if self.slice(pe, 4) != b'PE\0\0':
            raise ValueError('Bad PE signature')
        count = self.unpack('<H', pe + 6)[0]
        optsize = self.unpack('<H', pe + 20)[0]
        opt = pe + 24
        magic = self.unpack('<H', opt)[0]
        directory = {0x10b: 96, 0x20b: 112}.get(magic)
        if directory is None or optsize < directory + 24:
            raise ValueError('Missing PE resource directory')
        rva, self.resource_size = self.unpack('<II', opt + directory + 16)
        self.sections = [self.unpack('<IIII', opt + optsize + n * 40 + 8) for n in range(count)]
        self.base = self.offset(rva, self.resource_size) if rva else None

    def slice(self, offset, size):
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise ValueError('PE range outside file')
        return self.data[offset:offset + size]

    def unpack(self, fmt, offset):
        return struct.unpack(fmt, self.slice(offset, struct.calcsize(fmt)))

    def offset(self, rva, size):
        for virtual_size, address, raw_size, raw_offset in self.sections:
            if address <= rva and rva - address + size <= raw_size:
                offset = raw_offset + rva - address
                self.slice(offset, size)
                return offset
        raise ValueError('Resource RVA outside section data')

    def relative(self, offset, size):
        if offset < 0 or offset + size > self.resource_size:
            raise ValueError('Resource directory outside resource section')
        return self.slice(self.base + offset, size)

    def resources(self):
        if self.base is None:
            return []
        result = []

        def walk(at, path, ancestry):
            if at in ancestry or len(path) > 3:
                raise ValueError('Cyclic or over-deep resource directory')
            header = self.relative(at, 16)
            named, numeric = struct.unpack_from('<HH', header, 12)
            table = self.relative(at + 16, 8 * (named + numeric))
            for index in range(named + numeric):
                name, target = struct.unpack_from('<II', table, index * 8)
                if name & 0x80000000:
                    pos = name & 0x7fffffff
                    length = struct.unpack('<H', self.relative(pos, 2))[0]
                    name = self.relative(pos + 2, length * 2).decode('utf-16le')
                branch = path + [name]
                if target & 0x80000000:
                    walk(target & 0x7fffffff, branch, ancestry | {at})
                else:
                    if len(branch) != 3:
                        raise ValueError('Expected type/name/language resource hierarchy')
                    rva, size, codepage, reserved = struct.unpack('<IIII', self.relative(target, 16))
                    offset = self.offset(rva, size)
                    result.append(dict(type_id=branch[0], id=branch[1], language=branch[2],
                                       directory_entry=target, offset=offset, rva=rva,
                                       codepage=codepage, size=size))
        walk(0, [], set())
        return result


def dib_info(data, icon=False):
    if len(data) < 12:
        return None
    header = struct.unpack_from('<I', data)[0]
    if header == 12:
        width, height, planes, depth = struct.unpack_from('<HHHH', data, 4)
        colors, compression, palette_bytes = 0, 0, 3
    elif header in (40, 52, 56, 108, 124) and len(data) >= header:
        width, height, planes, depth, compression = struct.unpack_from('<iiHHI', data, 4)
        colors = struct.unpack_from('<I', data, 32)[0]
        palette_bytes = 4
    else:
        return None
    if width <= 0 or height == 0 or planes != 1 or depth not in (1, 4, 8, 16, 24, 32):
        return None
    palette_count = colors or ((1 << depth) if depth <= 8 else 0)
    masks = (16 if compression == 6 else 12) if header == 40 and compression in (3, 6) else 0
    pixels = header + masks + palette_count * palette_bytes
    actual_height = abs(height) // 2 if icon else abs(height)
    return dict(format='DIB', width=width, height=actual_height, stored_height=height,
                bit_depth=depth, compression=compression, pixel_offset=pixels,
                alpha_channel=(None if depth == 32 and compression == 0 else
                               bool(depth == 32 and header >= 56 and struct.unpack_from('<I', data, 52)[0])),
                transparency={'and_mask': icon, 'color_key': None, 'alpha_interpretation':
                              'unknown' if depth == 32 else 'no alpha channel'})


def metadata(data, kind):
    if kind is None and data[:2] == b'BM':
        info = dib_info(data[14:])
        if info:
            return dict(info, format='BMP')
    if kind is None and data[:4] in (b'\0\0\1\0', b'\0\0\2\0'):
        cursor = data[2] == 2
        count = struct.unpack_from('<H', data, 4)[0]
        variants = []
        for i in range(count):
            width, height, colors, reserved, a, b, size, offset = struct.unpack_from('<BBBBHHII', data, 6 + i * 16)
            if offset + size > len(data):
                raise ValueError('Icon/cursor image outside file')
            info = metadata(data[offset:offset + size], 3)
            info.update(width=width or 256, height=height or 256, size=size, offset=offset)
            if cursor:
                info['hotspot'] = [a, b]
            variants.append(info)
        return dict(format='CUR' if cursor else 'ICO', width=None, height=None,
                    bit_depth=None, alpha_channel=None, transparency=None, variants=variants)
    if kind is None and data[:3] == b'\xff\xd8\xff':
        pos = 2
        while pos + 4 <= len(data):
            if data[pos] != 255:
                break
            marker = data[pos + 1]
            if marker == 255:
                pos += 1
                continue
            length = struct.unpack_from('>H', data, pos + 2)[0]
            if length < 2 or pos + 2 + length > len(data):
                break
            if marker in (0xc0, 0xc1, 0xc2) and length >= 8:
                depth, height, width, channels = struct.unpack_from('>BHHB', data, pos + 4)
                return dict(format='JPEG', width=width, height=height, bit_depth=depth * channels,
                            alpha_channel=False, transparency=None)
            if marker == 0xda:
                break
            pos += 2 + length
        return dict(format='JPEG', width=None, height=None, bit_depth=None,
                    alpha_channel=False, transparency=None)
    if data[:4] == b'RIFF' and len(data) >= 12:
        form = data[8:12]
        info = dict(format={b'WAVE': 'WAV', b'ACON': 'ANI', b'AVI ': 'AVI'}.get(form, 'RIFF'),
                    width=None, height=None, bit_depth=None, alpha_channel=None, transparency=None)
        pos = 12
        while pos + 8 <= len(data):
            tag, size = struct.unpack_from('<4sI', data, pos)
            chunk = data[pos + 8:pos + 8 + size]
            if len(chunk) != size:
                break
            if form == b'WAVE' and tag == b'fmt ' and size >= 16:
                codec, channels, rate, byte_rate, align, depth = struct.unpack_from('<HHIIHH', chunk)
                info.update(codec=codec, channels=channels, sample_rate=rate, sample_bit_depth=depth)
            elif form == b'ACON' and tag == b'anih' and size >= 36:
                header, frames, steps, width, height, depth, planes, rate, flags = struct.unpack_from('<9I', chunk)
                info.update(frame_count=frames, steps=steps, width=width or None, height=height or None,
                            bit_depth=depth or None, default_rate_jiffies=rate)
            pos += 8 + size + size % 2
        return info
    if kind in (1, 2, 3):
        info = dib_info(data[4:] if kind == 1 else data, icon=kind in (1, 3))
        if info:
            if kind == 1:
                info['hotspot'] = list(struct.unpack_from('<HH', data))
            return info
    if data[:8] == b'\x89PNG\r\n\x1a\n' and len(data) >= 29:
        width, height, depth, color = struct.unpack_from('>IIBB', data, 16)
        channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}.get(color, 0)
        return dict(format='PNG', width=width, height=height, bit_depth=depth * channels,
                    alpha_channel=color in (4, 6), transparency={'color_key': None})
    return dict(format='PE resource bytes' if kind is not None else 'unknown binary', width=None, height=None, bit_depth=None,
                alpha_channel=None, transparency=None)


def group_info(data, kind):
    if len(data) < 6:
        raise ValueError('Truncated icon/cursor group')
    reserved, group_type, count = struct.unpack_from('<HHH', data)
    if reserved or group_type != (1 if kind == 14 else 2) or len(data) < 6 + count * 14:
        raise ValueError('Invalid icon/cursor group')
    entries = []
    for i in range(count):
        pos = 6 + i * 14
        if kind == 14:
            width, height, colors, unused, planes, depth, size, resource_id = struct.unpack_from('<BBBBHHIH', data, pos)
            width, height = width or 256, height or 256
        else:
            width, height, planes, depth, size, resource_id = struct.unpack_from('<HHHHIH', data, pos)
        entries.append(dict(id=resource_id, width=width, height=height, bit_depth=depth, size=size))
    return entries


def write(root, relative, data):
    target = root.joinpath(*safe_path(relative).parts)
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open('xb') as stream:
        stream.write(data)
    return relative


def extract_pe(root, source, data, previews):
    pe = PE(data)
    entries = []
    source_digest = sha(data)
    for record in pe.resources():
        raw = pe.slice(record['offset'], record['size'])
        kind = record['type_id']
        record.update(source=source, source_sha256=source_digest, type=TYPES.get(kind, str(kind)),
                      sha256=sha(raw), description='unknown', **metadata(raw, kind))
        extension = 'dib' if record['format'] == 'DIB' else 'bin'
        stem = f"{source}/{ident(kind)}/{ident(record['id'])}/{ident(record['language'])}-entry-{record['directory_entry']:x}"
        record['file'] = write(root, f'raw/{stem}.{extension}', raw)
        record['derivatives'] = []
        if kind in (12, 14):
            record['components'] = group_info(raw, kind)
        if kind == 2 and record['format'] == 'DIB':
            bmp = struct.pack('<2sIHHI', b'BM', len(raw) + 14, 0, 0, record['pixel_offset'] + 14) + raw
            bmp_path = write(root, f'previews/{stem}.bmp', bmp)
            record['derivatives'].append(dict(file=bmp_path, conversion='BMP file header added; DIB bytes unchanged'))
            if previews:
                png_path = f'previews/{stem}.png'
                run(['magick', str(root / bmp_path), str(root / png_path)])
                record['derivatives'].append(dict(file=png_path, conversion='ImageMagick BMP to PNG; no transparency key inferred'))
        if kind == 'TEXTFILE':
            text = raw.decode('utf-16le').lstrip('\ufeff').rstrip('\0')
            path = write(root, f'previews/{stem}.txt', text.encode('utf-8'))
            record['derivatives'].append(dict(file=path, conversion='UTF-16LE decoded to UTF-8'))
        entries.append(record)
    by_id = {(r['type_id'], r['id'], r['language']): r for r in entries}
    for record in entries:
        for component in record.get('components', []):
            child = by_id.get((3 if record['type_id'] == 14 else 1, component['id'], record['language']))
            component['file'] = child['file'] if child else None
            component['language_match'] = child is not None
    return entries


def theme_references(root, entries):
    """Record explicit theme properties with line provenance; do not infer defaults."""
    themes = []
    bitmaps = {(r['source'], r['id'], r['language']): r for r in entries if r['type_id'] == 2}
    for record in entries:
        if record['type_id'] != 'TEXTFILE':
            continue
        text_file = next((d['file'] for d in record['derivatives'] if d['file'].endswith('.txt')), None)
        if not text_file:
            continue
        properties, section = [], None
        for number, raw_line in enumerate((root / text_file).read_text().splitlines(), 1):
            line = raw_line.split(';', 1)[0].strip()
            if line.startswith('[') and line.endswith(']'):
                section = line[1:-1]
            elif section and '=' in line:
                key, value = (s.strip() for s in line.split('=', 1))
                prop = dict(section=section, property=key, value=value, line=number)
                if key.lower().startswith('imagefile'):
                    candidate = value.replace('\\', '_').replace('/', '_').replace('.', '_').upper()
                    bitmap = bitmaps.get((record['source'], candidate, record['language']))
                    prop['bitmap_id_candidate'] = candidate
                    prop['bitmap_file'] = bitmap['file'] if bitmap else None
                    prop['mapping_method'] = 'theme image filename normalized to PE bitmap name; verify before rendering'
                properties.append(prop)
        themes.append(dict(source=record['source'], resource_id=record['id'],
                           language=record['language'], raw_file=record['file'],
                           text_file=text_file, properties=properties))
    write(root, 'theme-references.json', (json.dumps(themes, indent=2) + '\n').encode())


def gallery(root, entries):
    cards = []
    for r in entries:
        png = next((d['file'] for d in r['derivatives'] if d['file'].endswith('.png')), None)
        if not png:
            continue
        label = f"{r['source']} | {r['type']} {r['id']} | lang {r['language']} | {r['width']}×{r['height']} | {r['bit_depth']}bpp"
        cards.append(f'<figure><a href="{html.escape(png, quote=True)}"><img loading="lazy" src="{html.escape(png, quote=True)}"></a><figcaption>{html.escape(label)}</figcaption></figure>')
    page = '<!doctype html><meta charset="utf-8"><title>XP resource inventory</title><style>body{font:14px sans-serif;background:#ddd}figure{display:inline-block;vertical-align:top;margin:8px;padding:8px;border:1px solid #999;max-width:420px}img{max-width:400px;max-height:200px;object-fit:contain;image-rendering:pixelated}figcaption{overflow-wrap:anywhere}</style><h1>Extracted bitmap resources</h1><p>Previews only. No transparency key or theme role inferred. Index contains original bytes and provenance.</p>' + ''.join(cards)
    write(root, 'gallery.html', page.encode())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='XP ISO or installation/extracted directory')
    parser.add_argument('--output', type=Path, required=True, help='New output directory; existing directories are refused')
    parser.add_argument('--profile', choices=['luna', 'all'], default='luna')
    parser.add_argument('--previews', action='store_true', help='Generate PNG bitmap previews with ImageMagick')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output already exists; choose a new directory to preserve originals and annotations')
    for tool in ['7z', 'wrestool'] + (['magick'] if args.previews else []):
        if not shutil.which(tool):
            parser.error(f'Missing required tool: {tool}')
    source = args.source.resolve(strict=True)
    if source.is_dir() and args.output.resolve().is_relative_to(source):
        parser.error('Output must be outside the input directory')
    if source.is_dir():
        members = [str(p.relative_to(source)) for p in sorted(source.rglob('*')) if p.is_file() and not p.is_symlink() and selected(str(p), args.profile)]
        origin = dict(directory=str(source))
    else:
        members = [m for m in archive_members(source) if selected(m, args.profile)]
        with source.open('rb') as stream:
            origin = dict(iso=source.name, sha256=hashlib.file_digest(stream, 'sha256').hexdigest())
    if not members:
        parser.error('No matching source files')
    root = args.output.resolve()
    root.mkdir(parents=True)
    index = dict(schema_version=1, profile=args.profile, origin=origin, sources=[], resources=[], files=[], errors=[])
    for member in members:
        safe_path(member)
        print(f'Extracting {member}', flush=True)
        try:
            data = (source / member).read_bytes() if source.is_dir() else run(['7z', 'x', '-so', str(source), member])
            container_path = write(root, f'sources/{member}', data)
            expanded = [(member, data)]
            if member.endswith('_'):
                names = archive_members(root / container_path)
                expanded = []
                for name in names:
                    payload = run(['7z', 'x', '-so', str(root / container_path), name])
                    expanded_name = member + '.expanded/' + name
                    write(root, f'sources/{expanded_name}', payload)
                    expanded.append((expanded_name, payload))
            for name, payload in expanded:
                index['sources'].append(dict(file=f'sources/{name}', source=name, container=member,
                                             sha256=sha(payload), container_sha256=sha(data), size=len(payload)))
                if payload[:2] == b'MZ':
                    listing = run(['wrestool', '-l', str(root / 'sources' / name)])
                    write(root, f'inventories/{name}.wrestool.txt', listing)
                    records = extract_pe(root, name, payload, args.previews)
                    if len(records) != sum(line.startswith(b'--type=') for line in listing.splitlines()):
                        raise ValueError('Resource count differs from wrestool inventory')
                    index['resources'].extend(records)
                else:
                    index['files'].append(dict(source=name, file=f'sources/{name}', sha256=sha(payload),
                                               description='unknown', **metadata(payload, None)))
        except (ValueError, OSError, UnicodeError, struct.error) as error:
            index['errors'].append(dict(source=member, error=str(error)))
            print(f'  ERROR: {error}', file=sys.stderr)
    write(root, 'index.json', (json.dumps(index, ensure_ascii=True, indent=2) + '\n').encode())
    theme_references(root, index['resources'])
    gallery(root, index['resources'])
    print(f"Indexed {len(index['resources'])} PE resources, {len(index['files'])} standalone files; {len(index['errors'])} errors. {root / 'index.json'}")
    return 1 if index['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
