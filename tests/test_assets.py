import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('xp_assets', Path(__file__).parents[1] / 'tools/xp_assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


def fixture():
    """Small synthetic PE: RVA differs from file offset, two languages, named ID."""
    data = bytearray(0x400)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 0x3c, 0x80)
    data[0x80:0x84] = b'PE\0\0'
    struct.pack_into('<H', data, 0x86, 1)
    struct.pack_into('<H', data, 0x94, 224)
    struct.pack_into('<H', data, 0x98, 0x10b)
    struct.pack_into('<II', data, 0x98 + 96 + 16, 0x1000, 0x200)
    struct.pack_into('<IIII', data, 0x98 + 224 + 8, 0x200, 0x1000, 0x200, 0x200)
    # Type -> named resource -> two language leaves.
    struct.pack_into('<HHII', data, 0x200 + 12, 0, 1, 10, 0x80000020)
    struct.pack_into('<HHII', data, 0x220 + 12, 1, 0, 0x800000b0, 0x80000040)
    struct.pack_into('<HHIIII', data, 0x240 + 12, 0, 2, 1033, 0x80, 1041, 0x90)
    struct.pack_into('<IIII', data, 0x280, 0x1100, 3, 1252, 0)
    struct.pack_into('<IIII', data, 0x290, 0x1103, 3, 932, 0)
    struct.pack_into('<H', data, 0x2b0, 3)
    data[0x2b2:0x2b8] = '123'.encode('utf-16le')
    data[0x300:0x306] = b'abcdef'
    return bytes(data)


class AssetTests(unittest.TestCase):
    def test_identity_languages_rva(self):
        pe = assets.PE(fixture())
        a, b = pe.resources()
        self.assertEqual((a['id'], a['language'], a['codepage']), ('123', 1033, 1252))
        self.assertEqual((b['language'], b['offset']), (1041, 0x303))
        self.assertEqual(pe.slice(a['offset'], a['size']), b'abc')
        self.assertNotEqual(assets.ident('123'), assets.ident(123))

    def test_cycle_and_outside_rva(self):
        for offset, value in [(0x214, 0x80000000), (0x280, 0xdeadbeef)]:
            data = bytearray(fixture())
            struct.pack_into('<I', data, offset, value)
            with self.assertRaises(ValueError):
                assets.PE(data).resources()

    def test_truncated_file(self):
        for length in (0, 2, 63, 0x90, 0x220):
            with self.assertRaises(ValueError):
                assets.PE(fixture()[:length]).resources()

    def test_dib_and_icon_height(self):
        dib = struct.pack('<IiiHHIIiiII', 40, 16, 32, 1, 32, 0, 2048, 0, 0, 0, 0) + bytes(2048)
        self.assertEqual(assets.metadata(dib, 2)['height'], 32)
        self.assertEqual(assets.metadata(dib, 3)['height'], 16)
        self.assertIsNone(assets.metadata(dib, 3)['alpha_channel'])
        cursor = struct.pack('<HH', 3, 4) + dib
        self.assertEqual(assets.metadata(cursor, 1)['hotspot'], [3, 4])

    def test_group_variants(self):
        group = struct.pack('<HHH', 0, 1, 2)
        group += struct.pack('<BBBBHHIH', 16, 16, 0, 0, 1, 32, 100, 7)
        group += struct.pack('<BBBBHHIH', 0, 0, 0, 0, 1, 32, 200, 8)
        self.assertEqual([e['width'] for e in assets.group_info(group, 14)], [16, 256])
        with self.assertRaises(ValueError):
            assets.group_info(group[:-1], 14)

    def test_standalone_media(self):
        dib = struct.pack('<IiiHHIIiiII', 40, 16, 32, 1, 32, 0, 2048, 0, 0, 0, 0) + bytes(2048)
        cur = struct.pack('<HHHBBBBHHII', 0, 2, 1, 16, 16, 0, 0, 3, 4, len(dib), 22) + dib
        info = assets.metadata(cur, None)
        self.assertEqual(info['format'], 'CUR')
        self.assertEqual(info['variants'][0]['hotspot'], [3, 4])
        fmt = struct.pack('<HHIIHH', 1, 2, 44100, 176400, 4, 16)
        wav = b'RIFF' + struct.pack('<I', 28) + b'WAVEfmt ' + struct.pack('<I', 16) + fmt
        self.assertEqual(assets.metadata(wav, None)['sample_rate'], 44100)

    def test_immutable_and_path_safety(self):
        for name in ('../escape', '/absolute', 'a/../../escape', 'C:/Windows', 'a\\b'):
            with self.assertRaises(ValueError):
                assets.safe_path(name)
        self.assertNotIn('/', assets.ident('../escape'))
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            entries = assets.extract_pe(root, 'source.dll', fixture(), False)
            self.assertEqual(len(entries), 2)
            self.assertEqual((root / entries[0]['file']).read_bytes(), b'abc')
            with self.assertRaises(FileExistsError):
                assets.extract_pe(root, 'source.dll', fixture(), False)


if __name__ == '__main__':
    unittest.main()
