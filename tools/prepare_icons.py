#!/usr/bin/env python3
"""Reconstruct indexed XP GROUP_ICON resources; preserve variant provenance."""
import argparse
import io
import json
import struct
from pathlib import Path
from PIL import Image, ImageDraw
from xp_assets import sha


def prepare(source, output):
    output.mkdir(parents=True, exist_ok=False)
    records = json.loads((source / 'index.json').read_text())['resources']
    byfile = {r['file']: r for r in records}
    ledger, thumbnails = [], []
    for group in records:
        if group['type'] != 'GROUP_ICON':
            continue
        library = Path(group['source']).name.lower()
        if library not in ('shell32.dll', 'explorer.exe', 'user32.dll', 'shdocvw.dll'):
            continue
        for size in (16, 32, 48):
            variants = [c for c in group['components'] if c['width'] == size and c['height'] == size and c.get('file')]
            if not variants:
                continue
            component = max(variants, key=lambda c: c['bit_depth'])
            record = byfile[component['file']]
            raw = (source / record['file']).read_bytes()
            if sha(raw) != record['sha256']:
                raise ValueError('Resource hash mismatch')
            ico = struct.pack('<HHHBBBBHHII', 0, 1, 1, size, size, 0, 0, 1, component['bit_depth'], len(raw), 22) + raw
            image = Image.open(io.BytesIO(ico)).convert('RGBA')
            name = f'{library}/GROUP_ICON/{group["id"]}/{group["language"]}-{size}.png'
            target = output / name
            target.parent.mkdir(parents=True, exist_ok=True)
            image.save(target)
            ledger.append(dict(source=group['source'], type='GROUP_ICON', id=group['id'], language=group['language'],
                               component=record['id'], raw_file=record['file'], sha256=sha(raw), file=name,
                               width=size, height=size, bit_depth=component['bit_depth'], format='PNG RGBA',
                               description='unknown; see contact sheet'))
            if size == 32:
                thumbnails.append((library, group['id'], image))
    (output / 'index.json').write_text(json.dumps(ledger, indent=2) + '\n')
    for library in sorted({t[0] for t in thumbnails}):
        entries = [t for t in thumbnails if t[0] == library]
        sheet = Image.new('RGB', (800, ((len(entries)+9)//10)*66), '#ece9d8')
        draw = ImageDraw.Draw(sheet)
        for n, (_, ident, image) in enumerate(entries):
            x, y = (n % 10)*80, (n//10)*66
            sheet.paste(image, (x+24, y+3), image)
            draw.text((x+20, y+40), str(ident), fill='black')
        sheet.save(output / f'{library}-contact.png')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.output)
