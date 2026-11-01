"""Installer deployment, uninstall and distribution boundaries in private roots."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xpwm_install', ROOT / 'tools/install.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallerTests(unittest.TestCase):
    def test_distribution_and_versioned_dependencies(self):
        for text, expected in [('ID=arch', 'pacman'), ('ID=ubuntu\nID_LIKE=debian', 'apt'),
                               ('ID=rocky\nID_LIKE="rhel centos fedora"', 'dnf'),
                               ('ID=opensuse-tumbleweed', 'zypper'), ('ID=unknown', None)]:
            self.assertEqual(installer.manager_for(text), expected)
        for manager in ('pacman', 'apt', 'dnf', 'zypper'):
            command = installer.dependency_command(manager)
            self.assertTrue(any('0.18' in item for item in command))
            self.assertFalse(any('firefox' in item for item in command))
        with self.assertRaises(ValueError): installer.dependency_command(None)

    def test_staged_paths_update_and_uninstall(self):
        with tempfile.TemporaryDirectory(prefix='xpwm installer ') as directory:
            stage = Path(directory)
            prefix = Path('/usr/local')
            files = {Path('bin/xpwm'): b'#!/bin/sh\necho fixture\n',
                     Path('lib/xpwm/shell/test'): b'runtime fixture'}
            root = stage / 'usr/local'
            unrelated = root / 'bin/unrelated'
            unrelated.parent.mkdir(parents=True)
            unrelated.write_text('leave me')
            with patch.object(installer, 'installed_files', return_value=files):
                installer.deploy(prefix, stage)
                self.assertTrue(os.access(root / 'bin/xpwm', os.X_OK))
                installer.deploy(prefix, stage)
                (root / 'bin/xpwm').write_text('user modification')
                with self.assertRaisesRegex(ValueError, 'modified'): installer.deploy(prefix, stage)
                installer.uninstall(prefix, stage)
                self.assertEqual((root / 'bin/xpwm').read_text(), 'user modification')
                self.assertTrue(unrelated.is_file())
                self.assertFalse((root / 'lib/xpwm/shell/test').exists())
                (root / 'bin/xpwm').unlink()
                installer.uninstall(prefix, stage)
                self.assertFalse((root / installer.MANIFEST).exists())

    def test_unmanaged_file_and_symlink_refused_before_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / 'install'
            (prefix / 'bin').mkdir(parents=True)
            target = prefix / 'bin/xpwm'
            target.write_text('unrelated')
            files = {Path('lib/xpwm/test'): b'new', Path('bin/xpwm'): b'overwrite'}
            with patch.object(installer, 'installed_files', return_value=files):
                with self.assertRaisesRegex(ValueError, 'Unmanaged'): installer.deploy(prefix)
                self.assertFalse((prefix / 'lib').exists())
                target.unlink()
                target.symlink_to(Path(directory) / 'outside')
                with self.assertRaisesRegex(ValueError, 'symlink'): installer.deploy(prefix)

    def test_manifest_path_traversal_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / 'install'
            manifest = prefix / installer.MANIFEST
            manifest.parent.mkdir(parents=True)
            manifest.write_text(json.dumps({'files': {'lib/xpwm/../../../outside': 'digest'}}))
            with self.assertRaisesRegex(ValueError, 'Unsafe'): installer.uninstall(prefix)
            self.assertTrue(manifest.is_file())

    def test_sddm_session_command_executes_without_literal_quotes(self):
        with tempfile.TemporaryDirectory(prefix='xpwm-sddm-') as directory:
            prefix = Path(directory)
            launcher = prefix / 'bin/xpwm'
            launcher.parent.mkdir()
            launcher.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
            launcher.chmod(0o755)
            # Only generated launcher metadata is relevant; no built binaries
            # or Windows resources are needed for this regression.
            with patch.object(Path, 'read_bytes', return_value=b'fixture'):
                files = installer.installed_files(prefix)
            desktop = files[Path('share/wayland-sessions/xpwm.desktop')].decode()
            command = next(line[5:] for line in desktop.splitlines() if line.startswith('Exec='))
            self.assertEqual(command, str(launcher) + ' tty')
            # Match /usr/share/sddm/scripts/wayland-session's exec $@ behavior:
            # quotes in the input value are literal, not shell syntax.
            result = subprocess.run(['/bin/sh', '-c', 'exec $@', 'sddm-fixture', command],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(result.stdout.strip(), 'tty')
            old = subprocess.run(['/bin/sh', '-c', 'exec $@', 'sddm-fixture',
                                  '"' + str(launcher) + '" tty'], capture_output=True, text=True)
            self.assertNotEqual(old.returncode, 0)

    def test_session_repair_updates_manifest_and_preserves_other_files(self):
        import hashlib
        with tempfile.TemporaryDirectory(prefix='xpwm-repair-') as directory:
            prefix = Path(directory)
            name = Path('share/wayland-sessions/xpwm.desktop')
            legacy = installer.session_entry(prefix).replace(
                ('Exec=' + str(prefix / 'bin/xpwm') + ' tty').encode(),
                ('Exec="' + str(prefix / 'bin/xpwm') + '" tty').encode())
            files = {Path('bin/xpwm'): b'#!/bin/sh\nexit 0\n', name: legacy,
                     Path('lib/xpwm/test'): b'keep runtime'}
            with patch.object(installer, 'installed_files', return_value=files):
                installer.deploy(prefix)
            before = json.loads((prefix / installer.MANIFEST).read_text())
            installer.repair_session_entry(prefix)
            after = json.loads((prefix / installer.MANIFEST).read_text())
            self.assertEqual((prefix / name).read_bytes(), installer.session_entry(prefix))
            self.assertEqual(after['files'][str(name)],
                hashlib.sha256(installer.session_entry(prefix)).hexdigest())
            for path in ('bin/xpwm', 'lib/xpwm/test'):
                self.assertEqual(after['files'][path], before['files'][path])
                self.assertEqual((prefix / path).read_bytes(), files[Path(path)])
            installer.repair_session_entry(prefix)  # Safe to repeat.
            (prefix / name).write_text('user edited entry')
            with self.assertRaisesRegex(ValueError, 'modified'):
                installer.repair_session_entry(prefix)

    def test_application_launchers_keep_quoting_for_spaces(self):
        prefix = Path('/tmp/XPWm prefix with spaces')
        with patch.object(Path, 'read_bytes', return_value=b'fixture'):
            files = installer.installed_files(prefix)
        desktop = files[Path('share/applications/xpwm-theme-preview.desktop')].decode()
        self.assertIn('Exec="/tmp/XPWm prefix with spaces/bin/xpwm-theme-preview"', desktop)

    def test_dry_run_is_side_effect_free(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'not-created'
            for manager in ('pacman', 'apt', 'dnf', 'zypper'):
                result = subprocess.run([sys.executable, str(ROOT / 'tools/install.py'),
                    '--dry-run', '--deps', '--package-manager', manager, '--prefix', str(output)],
                    capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('Would install', result.stdout)
            self.assertFalse(output.exists())
