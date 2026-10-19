import json
import os
import time
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class SoundPreparationTests(unittest.TestCase):
    def test_xp_window_event_roles(self):
        sys.path.insert(0, str(ROOT / 'tools'))
        from prepare_sounds import role_for
        for name, expected in [('XPMIN', 'minimize'), ('XPRESTOR', 'restore'),
                               ('XPSTART', 'navigation'), ('XPEXCL', 'warning'),
                               ('XPCRTSTP', 'critical'), ('XPDEF', 'default')]:
            self.assertEqual(role_for('I386/'+name+'.WA_'), expected)

    def test_wav_role_and_provenance(self):
        with tempfile.TemporaryDirectory(prefix="xpwm-sounds-") as directory:
            source = Path(directory) / "Windows" / "Media"
            output = Path(directory) / "out"
            source.mkdir(parents=True)
            wav = source / "Windows XP Logon Sound.wav"
            with wave.open(str(wav), "wb") as stream:
                stream.setnchannels(1)
                stream.setsampwidth(2)
                stream.setframerate(8000)
                stream.writeframes(b"\0\0" * 80)
            result = subprocess.run(
                [sys.executable, "tools/prepare_sounds.py", str(Path(directory) / "Windows"),
                 "--output", str(output)], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue((output / "logon.wav").is_file())
            index = json.loads((output / "index.json").read_text())
            self.assertEqual(len(index["sounds"]), 1)
            record = index["sounds"][0]
            self.assertEqual(record["source"], "Media/Windows XP Logon Sound.wav")
            self.assertEqual(record["role"], "logon")
            self.assertTrue(record["selected"])
            self.assertEqual(record["decoded_file"], "logon.wav")
            self.assertEqual(record["sample_rate"], 8000)


@unittest.skipUnless((ROOT / 'shell/xpwm-sound').exists(), 'sound helper not built')
class SoundPlaybackTests(unittest.TestCase):
    def test_mute_and_literal_backend_argument(self):
        with tempfile.TemporaryDirectory(prefix='xpwm sound ') as directory:
            root=Path(directory); marker=root/'played'; player=root/'fake player'
            player.write_text('#!/bin/sh\nprintf \'%s\\n\' "$1" >> "$XPWM_SOUND_MARKER"\n')
            player.chmod(0o700)
            wav=root/'minimize.wav'
            with wave.open(str(wav), 'wb') as stream:
                stream.setnchannels(1); stream.setsampwidth(2); stream.setframerate(8000)
                stream.writeframes(b'\0\0'*80)
            env=dict(os.environ, XPWM_SOUND_DIR=str(root), XPWM_SOUND_PLAYER=str(player),
                     XPWM_SOUND_MARKER=str(marker), XPWM_SOUND_MUTE='1')
            command=[str(ROOT/'shell/xpwm-sound'), 'minimize']
            subprocess.run(command, env=env, check=True)
            self.assertFalse(marker.exists())
            del env['XPWM_SOUND_MUTE']
            subprocess.run(command, env=env, check=True)
            deadline=time.monotonic()+2
            while time.monotonic()<deadline:
                if marker.exists() and marker.read_text(): break
                time.sleep(.02)
            self.assertEqual(marker.read_text().splitlines(), [str(wav)])


if __name__ == "__main__":
    unittest.main()
