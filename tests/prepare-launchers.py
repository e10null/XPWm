"""Populate a private XDG application directory with harmless test launchers."""
import json
import sys
from pathlib import Path

directory = Path(sys.argv[1]) / 'applications'
directory.mkdir(parents=True)
binary = Path(__file__).resolve().parents[1] / 'compositor/shell-ui-test'
for name, hidden, categories in [
    ('Alpha', False, 'Utility;'), ('Beta', False, 'Utility;'),
    ('Hidden', True, 'Utility;'), ('CGame', False, 'Network;Game;'),
    ('DGraphics', False, 'Graphics;'), ('ESettings', False, 'Utility;Settings;'),
    ('FOther', False, '')]:
    (directory / f'{name}.desktop').write_text(
        f'[Desktop Entry]\nType=Application\nName={name}\n'
        f'Exec={json.dumps(str(binary))} --fixture {name}\n'
        f'NoDisplay={str(hidden).lower()}\nTerminal=false\nCategories={categories}\n'
        + ('MimeType=x-scheme-handler/https;\n' if name == 'Alpha' else
           'MimeType=x-scheme-handler/mailto;\n' if name == 'Beta' else ''))

(directory / 'mimeinfo.cache').write_text(
    '[MIME Cache]\nx-scheme-handler/https=Alpha.desktop;\nx-scheme-handler/mailto=Beta.desktop;\n')
(directory / 'mimeapps.list').write_text(
    '[Default Applications]\nx-scheme-handler/https=Alpha.desktop\nx-scheme-handler/mailto=Beta.desktop\n')
