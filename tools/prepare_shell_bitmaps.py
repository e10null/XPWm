#!/usr/bin/env python3
"""Prepare Explorer/LogonUI shell bitmaps and preserve their provenance."""
import json
from pathlib import Path
from prepare_luna import rgba_dib, png
from xp_assets import sha

def prepare(source, output):
    output.mkdir(parents=True, exist_ok=False)
    records = json.loads((source / 'index.json').read_text())['resources']
    ledger = []
    for r in records:
        source_name = r['source'].lower()
        selected = ((source_name.endswith('explorer.exe') and r['type'] == 'BITMAP' and r['id'] == 143) or
                    (source_name.endswith('logonui.exe') and r['type'] == 'BITMAP' and r['id'] in (100, 102, 103, 104, 105, 106, 107, 108, 109, 110, 112, 113, 114, 119, 121, 122, 123, 124, 125, 126, 127)) or
                    (source_name.endswith('msgina.dll') and r['type'] == 'BITMAP' and r['id'] in (20142, 20143, 20150)) or
                    (source_name.endswith('shell32.dll') and r['type'] == 'BITMAP' and r['id'] == 14356))
        if selected:
            raw = (source / r['file']).read_bytes()
            if sha(raw) != r['sha256']:
                raise ValueError(f'Shell bitmap hash mismatch: {r["file"]}')
            w, h, rows = rgba_dib(raw, True, (255, 0, 255))
            library = source_name.rsplit('/', 1)[-1]
            name = f'{library}-BITMAP-{r["id"]}-{r["language"]}.png'
            encoded = png(w, h, rows)
            if (output / name).exists() and (output / name).read_bytes() != encoded:
                raise ValueError(f'refusing to overwrite changed asset: {name}')
            (output / name).write_bytes(encoded)
            ledger.append(dict(r, derivative=name))
    (output / 'index.json').write_text(json.dumps(ledger, indent=2)+'\n')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path('assets/local/xp-sp3'))
    parser.add_argument('--output', type=Path, default=Path('assets/local/xp-bitmaps'))
    args = parser.parse_args()
    prepare(args.source, args.output)
