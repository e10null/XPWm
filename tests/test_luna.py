"""License-free conversion fixtures; no Windows installation required."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from prepare_luna import rgba_dib, png, decode_rle8


def dib(depth, pixels, height=1, palette=b''):
    return struct.pack('<IiiHHIIiiII', 40, 1, height, 1, depth, 0,
                       len(pixels), 0, 0, len(palette) // 4, 0) + palette + pixels


class LunaConversionTests(unittest.TestCase):
    def test_straight_alpha(self):
        self.assertEqual(rgba_dib(dib(32, bytes([5, 10, 240, 80])))[2],
                         [bytes([240, 10, 5, 80])])

    def test_explicit_red_key(self):
        data = dib(24, bytes([0, 0, 255, 0]))
        self.assertEqual(rgba_dib(data, True, (255, 0, 0))[2][0][-1], 0)
        self.assertEqual(rgba_dib(data, True)[2][0][-1], 255)

    def test_palette_key(self):
        data = dib(8, bytes(4), palette=bytes([255, 0, 255, 0]))
        self.assertEqual(rgba_dib(data, True)[2], [bytes([255, 0, 255, 0])])

    def test_row_order(self):
        pixels = bytes([1, 2, 3, 0, 4, 5, 6, 0])
        self.assertEqual(rgba_dib(dib(24, pixels, 2))[2][0], bytes([6, 5, 4, 255]))
        self.assertEqual(rgba_dib(dib(24, pixels, -2))[2][0], bytes([3, 2, 1, 255]))

    def test_palette_bounds(self):
        with self.assertRaises(ValueError):
            rgba_dib(dib(8, bytes([1, 0, 0, 0]), palette=bytes(4)))

    def test_truncated(self):
        with self.assertRaises(ValueError):
            rgba_dib(dib(32, bytes(3)))

    def test_png(self):
        self.assertTrue(png(1, 1, [bytes(4)]).startswith(b'\x89PNG\r\n\x1a\n'))


class Rle8Tests(unittest.TestCase):
    def test_runs_absolute_padding_delta_and_row_order(self):
        # Bottom row: repeated blue. Top row: skipped pixel + RGB absolute run.
        encoded = bytes([4, 3, 0, 0, 0, 2, 1, 0, 0, 3, 1, 2, 3, 0, 0, 1])
        palette = bytes([0, 0, 0, 0, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 0])
        data = struct.pack('<IiiHHIIiiII', 40, 4, 2, 1, 8, 1,
                           len(encoded), 0, 0, 4, 0) + palette + encoded
        w, h, rows = rgba_dib(data)
        self.assertEqual((w, h), (4, 2))
        self.assertEqual(rows[0], bytes([0, 0, 0, 255, 255, 0, 0, 255,
                                        0, 255, 0, 255, 0, 0, 255, 255]))
        self.assertEqual(rows[1], bytes([0, 0, 255, 255]) * 4)

    def test_malformed_streams(self):
        streams = [bytes([5, 1, 0, 1]),  # run crosses row
                   bytes([0, 5, 1, 2, 3, 4, 5, 0, 0, 1]),  # absolute crosses row
                   bytes([0, 3, 1, 2]),  # truncated absolute run
                   bytes([0, 2, 1]),  # truncated delta
                   bytes([0, 2, 0, 2]),  # delta crosses bitmap
                   bytes([0, 0, 0, 0, 0, 0, 0, 1]),  # too many rows
                   bytes([4, 1])]  # no end marker
        for stream in streams:
            with self.subTest(stream=stream), self.assertRaises(ValueError):
                decode_rle8(stream, 4, 2)
