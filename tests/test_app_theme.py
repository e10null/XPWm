"""Synthetic source strips verify app-theme conversion without Windows media."""
import contextlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import prepare_app_theme as app_theme
from xp_assets import sha


class AppThemeTests(unittest.TestCase):
    def make_source(self, root):
        resources, properties = [], []
        for line, (role, (section, key)) in enumerate(app_theme.ROLES.items()):
            # Twelve distinct top-down frames, each at a native 20x23 size.
            count = 16 if role == 'scroll-arrows' else 12
            pixels = b''.join(bytes((40+state, 20+state, state))*20*23 for state in range(count))
            data = struct.pack('<IiiHHIIiiII', 40, 20, -23*count, 1, 24, 0, len(pixels), 0, 0, 0, 0)+pixels
            name = f'{role}.dib'; (root/name).write_bytes(data)
            resources.append(dict(file=name, sha256=sha(data), source='synthetic.dll', id=role, language=1033))
            properties.extend([dict(section=section, property=key, bitmap_file=name, line=line),
                               dict(section=section, property='ImageCount', value=str(count)),
                               dict(section=section, property='SizingMargins', value='5,5,5,5')])
        (root/'index.json').write_text(json.dumps(dict(resources=resources)))
        (root/'theme-references.json').write_text(json.dumps([dict(resource_id='NORMALBLUE_INI',
                                                                 raw_file='synthetic.ini', properties=properties)]))

    def test_frames_manifest_css_and_session_font_config(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as name:
            root = Path(name); source=root/'source'; source.mkdir(); self.make_source(source)
            with patch.object(app_theme, 'prepare_icons'), contextlib.redirect_stdout(io.StringIO()):
                app_theme.prepare(source, root/'share')
            theme=root/'share/themes/XPWm-Luna'
            manifest=json.loads((theme/'provenance.json').read_text())
            self.assertEqual(len(manifest), 12*len(app_theme.ROLES)+4)
            with Image.open(theme/'gtk-3.0/assets/button-2.png') as image:
                self.assertEqual(image.size, (20,23))
                self.assertEqual(image.getpixel((10,10)), (2,22,42,255))
            css=(theme/'gtk-3.0/gtk.css').read_text()
            self.assertIn('checkbutton check:checked', css)
            self.assertIn('button-4.png', css)
            self.assertIn('scrollbar.vertical slider', css)
            self.assertIn('scroll-arrows-15.png', css)
            self.assertIn('slider-v-4.png', css)
            self.assertIn('progress-v-0.png', css)
            self.assertTrue((theme/'gtk-3.0/assets/arrow-down.svg').exists())
            self.assertTrue((theme/'gtk-2.0/gtkrc').exists())

    def test_hash_mismatch_does_not_write_theme(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name); source=root/'source'; source.mkdir(); self.make_source(source)
            (source/'button.dib').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'Source hash mismatch'):
                app_theme.prepare(source, root/'share')
            self.assertFalse((root/'share').exists())
