#!/usr/bin/env python3
"""Extract Windows XP Media WAV files into a local, provenance-indexed tree.

The input may be an XP ISO or an already extracted Windows directory.  The
original WAV/WA_ bytes are kept under ``raw/``; canonical event names are
decoded copies used by the shell.  No Microsoft media is copied into the
source repository by this tool.
"""
import argparse
import hashlib
import json
import shutil
import sys
from pathlib import Path, PurePosixPath

from xp_assets import archive_members, metadata, run, safe_path, sha, write

ROLE_STEMS = {
    'startup': {'windows xp startup', 'windows startup', 'windows start', 'xpstartu'},
    'logon': {'windows xp logon sound', 'windows logon', 'xplogon'},
    'logoff': {'windows xp logoff sound', 'windows logoff', 'xplogoff'},
    'shutdown': {'windows xp shutdown', 'windows shutdown', 'xpshutdn'},
    'minimize': {'windows xp minimize', 'xpmin'},
    'restore': {'windows xp restore', 'xprestor'},
    'navigation': {'windows xp start', 'xpstart'},
    'warning': {'windows xp exclamation', 'xpexcl'},
    'critical': {'windows xp critical stop', 'xpcrtstp'},
    'default': {'windows xp default', 'xpdef', 'xpding'},
    'menu': {'windows xp menu command', 'windows menu command', 'xpmenu'},
    'notify': {'windows xp notify', 'windows notify', 'windows xp balloon', 'xpnotify', 'xpballn'},
    'error': {'windows xp error', 'windows critical stop', 'xperror'},
}


def role_for(member):
    stem = PurePosixPath(member).stem.lower().strip()
    for role, names in ROLE_STEMS.items():
        if stem in names:
            return role
    return None


def members_for(source):
    if source.is_dir():
        return [str(path.relative_to(source)) for path in sorted(source.rglob('*'))
                if path.is_file() and not path.is_symlink() and
                path.suffix.lower() in {'.wav', '.wa_'}]
    return [member for member in archive_members(source)
            if PurePosixPath(member).suffix.lower() in {'.wav', '.wa_'}]


def read_member(source, member):
    path = source / member if source.is_dir() else None
    original = path.read_bytes() if path else run(['7z', 'x', '-so', str(source), member])
    if PurePosixPath(member).suffix.lower() == '.wa_':
        if source.is_dir():
            decoded = run(['7z', 'x', '-so', str(path)])
        else:
            # 7z can extract a compressed member directly from an ISO.
            decoded = run(['7z', 'x', '-so', str(source), member])
            decoded_path = None
            # A second pass is required for a cabinet-compressed WA_ payload.
            # Keep this explicit so no executable content is ever interpreted.
            if decoded[:4] != b'RIFF':
                import tempfile
                with tempfile.NamedTemporaryFile(suffix='.wa_', delete=False) as stream:
                    stream.write(decoded); decoded_path = Path(stream.name)
                try:
                    decoded = run(['7z', 'x', '-so', str(decoded_path)])
                finally:
                    decoded_path.unlink(missing_ok=True)
        return original, decoded
    return original, original


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='XP ISO or extracted Windows directory')
    parser.add_argument('--output', type=Path, required=True,
                        help='New output directory for local sound derivatives')
    args = parser.parse_args()
    for tool in ['7z']:
        if not shutil.which(tool): parser.error(f'Missing required tool: {tool}')
    source = args.source.resolve(strict=True)
    if args.output.exists():
        parser.error('Output already exists; choose a new directory to preserve originals')
    if source.is_dir() and args.output.resolve().is_relative_to(source):
        parser.error('Output must be outside the input directory')
    members = members_for(source)
    if not members: parser.error('No WAV/WA_ files found')
    root = args.output.resolve(); root.mkdir(parents=True)
    if source.is_dir(): origin = {'directory': str(source)}
    else:
        with source.open('rb') as stream:
            origin = {'iso': source.name, 'sha256': hashlib.file_digest(stream, 'sha256').hexdigest()}
    index = {'schema_version': 1, 'origin': origin, 'sounds': [], 'errors': []}
    selected = {}
    for member in members:
        safe_path(member)
        print(f'Extracting {member}', flush=True)
        try:
            original, decoded = read_member(source, member)
            if decoded[:4] != b'RIFF' or decoded[8:12] != b'WAVE':
                raise ValueError('decoded member is not a RIFF/WAVE file')
            raw_file = write(root, f'raw/{member}', original)
            role = role_for(member)
            record = {
                'source': member,
                'source_sha256': sha(original),
                'file': raw_file,
                'original_format': 'WA_' if PurePosixPath(member).suffix.lower() == '.wa_' else 'WAV',
                'decoded_sha256': sha(decoded),
                'role': role,
                **metadata(decoded, None),
            }
            if role:
                # Prefer an uncompressed WAV, then the shortest source path;
                # all candidates stay indexed even when one role is selected.
                priority = (record['original_format'] != 'WAV', len(member), member.lower())
                if role not in selected or priority < selected[role][0]:
                    selected[role] = (priority, decoded, record)
            index['sounds'].append(record)
        except (OSError, ValueError, UnicodeError):
            index['errors'].append({'source': member, 'error': str(sys.exc_info()[1])})
    for role, (_, decoded, record) in sorted(selected.items()):
        record['selected'] = True
        record['decoded_file'] = write(root, f'{role}.wav', decoded)
    write(root, 'index.json', (json.dumps(index, ensure_ascii=True, indent=2) + '\n').encode())
    print(f"Indexed {len(index['sounds'])} WAV files, {len(selected)} canonical XP roles; {len(index['errors'])} errors.")
    return 1 if index['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
