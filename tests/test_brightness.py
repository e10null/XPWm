import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('brightness', Path(__file__).resolve().parents[1] / 'tools/brightness.py')
brightness = importlib.util.module_from_spec(spec)
spec.loader.exec_module(brightness)


class BrightnessTests(unittest.TestCase):
    def test_login_session_and_raw_brightness_limits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            device = root / 'intel_backlight'
            device.mkdir()
            (device / 'max_brightness').write_text('1515')
            with patch.object(brightness, 'BACKLIGHT_ROOT', root), \
                    patch.object(brightness.shutil, 'which', return_value=None), \
                    patch.dict(brightness.os.environ, {'XDG_SESSION_ID': '2'}), \
                    patch.object(brightness.subprocess, 'run') as run:
                run.return_value = subprocess.CompletedProcess([], 0, "(objectpath '/org/freedesktop/login1/session/_32',)\n")
                for current, direction, expected in ((500, 'up', '652'), (5, 'down', '16'), (1500, 'up', '1515')):
                    (device / 'brightness').write_text(str(current))
                    brightness.adjust(direction)
                    command = run.call_args.args[0]
                    self.assertIn('/org/freedesktop/login1/session/_32', command)
                    self.assertEqual(command[-3:], ['backlight', 'intel_backlight', expected])

    def test_missing_backlight_is_reported(self):
        with tempfile.TemporaryDirectory() as directory, \
                patch.object(brightness, 'BACKLIGHT_ROOT', Path(directory)), \
                patch.object(brightness.shutil, 'which', return_value=None):
            with self.assertRaisesRegex(ValueError, 'No display backlight'):
                brightness.adjust('up')

    def test_authorization_failure_is_not_silenced(self):
        with patch.object(brightness.shutil, 'which', return_value='/usr/bin/brightnessctl'), \
                patch.object(brightness.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, ['brightnessctl'])):
            with self.assertRaises(subprocess.CalledProcessError):
                brightness.adjust('down')
