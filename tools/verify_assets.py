#!/usr/bin/env python3
"""Verify indexed originals against source bytes, hashes, lengths and group links."""
import argparse
import json
from pathlib import Path
from xp_assets import sha, safe_path


def verify(root):
    index = json.loads((root / 'index.json').read_text())
    if index['errors']:
        raise ValueError(f"Extraction has {len(index['errors'])} errors")
    sources = {}
    for entry in index['sources']:
        raw = root.joinpath(*safe_path(entry['file']).parts).read_bytes()
        if sha(raw) != entry['sha256'] or len(raw) != entry['size']:
            raise ValueError(f"Source mismatch: {entry['file']}")
        sources[entry['source']] = raw
    paths = {entry['file'] for entry in index['resources']}
    for entry in index['resources']:
        raw = root.joinpath(*safe_path(entry['file']).parts).read_bytes()
        source = sources[entry['source']]
        expected = source[entry['offset']:entry['offset'] + entry['size']]
        if raw != expected or len(raw) != entry['size'] or sha(raw) != entry['sha256']:
            raise ValueError(f"Resource mismatch: {entry['file']}")
        for derivative in entry['derivatives']:
            path = root.joinpath(*safe_path(derivative['file']).parts)
            if not path.is_file():
                raise ValueError(f'Missing derivative: {path}')
            if path.suffix == '.bmp' and path.read_bytes()[14:] != raw:
                raise ValueError(f'BMP payload changed: {path}')
        for component in entry.get('components', []):
            if component['file'] not in paths:
                raise ValueError(f"Unresolved group component: {entry['file']} / {component['id']}")
    for entry in index['files']:
        if sha(root.joinpath(*safe_path(entry['file']).parts).read_bytes()) != entry['sha256']:
            raise ValueError(f"Standalone file mismatch: {entry['file']}")
    print(f"PASS: {len(sources)} sources, {len(paths)} PE resources, {len(index['files'])} standalone files; source bytes, hashes, BMP payloads and group links verified")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    verify(args.directory)
