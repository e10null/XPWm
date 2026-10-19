"""Optional checks for the user's privately extracted XP assets."""
import hashlib
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / 'assets/local/xp-icons'


@unittest.skipUnless((ICONS / 'index.json').exists(), 'private XP resources not extracted')
class LocalIcons(unittest.TestCase):
    def test_shell_roles_have_native_variants_and_provenance(self):
        from PIL import Image
        records = json.loads((ICONS / 'index.json').read_text())
        for resource in (3, 4, 16, 17, 18, 20, 21, 23, 24, 25, 28, 32, 34, 35, 235, 236, 237, 238, 242, 512):
            for size in (16, 32):
                relative = f'shell32.dll/GROUP_ICON/{resource}/1033-{size}.png'
                record = next(r for r in records if r['file'] == relative)
                raw = ROOT / 'assets/local/xp-sp3' / record['raw_file']
                self.assertEqual(hashlib.sha256(raw.read_bytes()).hexdigest(), record['sha256'])
                with Image.open(ICONS / relative) as image:
                    self.assertEqual(image.size, (size, size))
                    self.assertEqual(image.mode, 'RGBA')

    def test_explorer_start_flag(self):
        from PIL import Image
        directory = ROOT / 'assets/local/xp-bitmaps'
        records = json.loads((directory / 'index.json').read_text())
        flag = next(r for r in records if r['id'] == 143 and r['source'].lower().endswith('explorer.exe'))
        with Image.open(directory / flag['derivative']) as image:
            self.assertEqual(image.size, (25, 20))
            self.assertEqual(image.mode, 'RGBA')
        tile = next(r for r in records if r['id'] == 114 and r['source'].lower().endswith('logonui.exe'))
        with Image.open(directory / tile['derivative']) as image:
            self.assertEqual(image.size, (48, 48))
            self.assertEqual(image.mode, 'RGBA')


@unittest.skipUnless((ROOT / 'assets/local/xp-bitmaps/msgina.dll-BITMAP-20142-1033.png').exists(),
                     'private session bitmaps not prepared')
class LocalSessionBitmaps(unittest.TestCase):
    def test_dimensions_and_source_hashes(self):
        from PIL import Image
        directory = ROOT / 'assets/local/xp-bitmaps'
        records = json.loads((directory / 'index.json').read_text())
        expected = {'msgina.dll-BITMAP-20142-1033.png': (313, 198),
                    'msgina.dll-BITMAP-20143-1033.png': (48, 40),
                    'msgina.dll-BITMAP-20150-1033.png': (32, 320),
                    'shell32.dll-BITMAP-14356-1033.png': (32, 192)}
        for name, size in expected.items():
            record = next(r for r in records if r['derivative'] == name)
            raw = ROOT / 'assets/local/xp-sp3' / record['file']
            self.assertEqual(hashlib.sha256(raw.read_bytes()).hexdigest(), record['sha256'])
            with Image.open(directory / name) as image:
                self.assertEqual(image.size, size)
                self.assertEqual(image.mode, 'RGBA')
