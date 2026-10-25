"""Qt session DPI agrees with the native appearance preference."""
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import session_fonts


class FontSettingsTests(unittest.TestCase):
    def test_saved_and_invalid_dpi(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(os.environ, {'XDG_CONFIG_HOME': directory}):
                self.assertEqual(session_fonts.font_dpi(), 96)
                path = Path(directory) / 'xpwm/appearance.ini'
                path.parent.mkdir()
                for value, expected in [('72', 72), ('84', 84), ('96', 96), ('120', 120), ('144', 144),
                                        ('192', 96), ('bad', 96)]:
                    path.write_text(f'[Appearance]\nFontDPI={value}\n')
                    self.assertEqual(session_fonts.font_dpi(), expected)
                path.write_text('invalid ini data')
                self.assertEqual(session_fonts.font_dpi(), 96)
