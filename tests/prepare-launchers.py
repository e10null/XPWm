"""Populate a private XDG application directory with harmless test launchers."""
import json
import sys
from pathlib import Path

directory = Path(sys.argv[1]) / 'applications'
directory.mkdir(parents=True)
binary = Path(__file__).resolve().parents[1] / 'compositor/shell-ui-test'
for name, hidden in [('Alpha', False), ('Beta', False), ('Hidden', True)]:
    (directory / f'{name}.desktop').write_text(
        f'[Desktop Entry]\nType=Application\nName={name}\n'
        f'Exec={json.dumps(str(binary))} --fixture {name}\n'
        f'NoDisplay={str(hidden).lower()}\nTerminal=false\n')
