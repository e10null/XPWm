#!/usr/bin/env python3
"""Prepare Explorer/LogonUI shell bitmaps and preserve their provenance."""
import json
from pathlib import Path
from prepare_luna import rgba_dib, png
from xp_assets import sha

source = Path('assets/local/xp-sp3')
output = Path('assets/local/xp-bitmaps')
output.mkdir(exist_ok=True)
records = json.loads((source / 'index.json').read_text())['resources']
ledger = []
for r in records:
    source_name = r['source'].lower()
    selected = ((source_name.endswith('explorer.exe') and r['type'] == 'BITMAP' and r['id'] == 143) or
                (source_name.endswith('logonui.exe') and r['type'] == 'BITMAP' and r['id'] in (103, 104, 105, 106, 107, 108, 109, 110, 113, 114, 119, 121, 122)))
    if selected:
        raw = (source / r['file']).read_bytes()
        assert sha(raw) == r['sha256']
        w, h, rows = rgba_dib(raw, True, (255, 0, 255))
        library = 'explorer.exe' if source_name.endswith('explorer.exe') else 'logonui.exe'
        name = f'{library}-BITMAP-{r["id"]}-{r["language"]}.png'
        encoded = png(w, h, rows)
        if (output / name).exists() and (output / name).read_bytes() != encoded:
            raise ValueError(f'refusing to overwrite changed asset: {name}')
        (output / name).write_bytes(encoded)
        ledger.append(dict(r, derivative=name))
(output / 'index.json').write_text(json.dumps(ledger, indent=2)+'\n')
