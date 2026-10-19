"""Color-depth selection must not silently fall back to the Win9x speaker."""
import io
import struct
import sys
import unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from prepare_tray import native_icon

class TrayIcons(unittest.TestCase):
    def test_prefers_32bit_native_frame(self):
        def dib(depth):
            header = struct.pack('<IiiHHIIiiII', 40, 16, 32, 1, depth, 0, 0, 0, 0, 0, 0)
            if depth == 4:
                return header + bytes([0, 255, 0, 0])*16 + bytes(128) + bytes(64)
            return header + bytes([0, 0, 255, 255])*256 + bytes(64)
        low, high = dib(4), dib(32)
        ico = struct.pack('<HHH', 0, 1, 2)
        ico += struct.pack('<BBBBHHII', 16, 16, 16, 0, 1, 4, len(low), 38)
        ico += struct.pack('<BBBBHHII', 16, 16, 0, 0, 1, 32, len(high), 38+len(low))
        image, depth = native_icon(Image.open(io.BytesIO(ico+low+high)))
        self.assertEqual(depth, 32)
        self.assertEqual(image.size, (16, 16))
        self.assertEqual(image.getpixel((0, 0)), (255, 0, 0, 255))

if __name__ == '__main__':
    unittest.main()
