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
