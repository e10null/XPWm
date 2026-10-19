"""Importer boundary checks; full media extraction is an opt-in integration run."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('iso_import', ROOT / 'tools/import-xp-iso.py')
iso_import = importlib.util.module_from_spec(spec)
spec.loader.exec_module(iso_import)


class IsoImportTests(unittest.TestCase):
    def test_missing_dependencies_write_nothing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            iso = root / 'input.iso'
            iso.write_bytes(b'fixture')
            with patch.object(iso_import.shutil, 'which', return_value=None):
                with self.assertRaisesRegex(ValueError, 'Missing dependencies'):
                    iso_import.import_iso(iso, root / 'output')
            self.assertFalse((root / 'output').exists())
            self.assertEqual(iso.read_bytes(), b'fixture')

    def test_existing_asset_sets_and_dangling_links_are_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            iso = root / 'input.iso'
            iso.write_bytes(b'fixture')
            local = root / 'output/assets/local'
            local.mkdir(parents=True)
            for name in iso_import.OUTPUTS:
                target = local / name
                target.symlink_to(root / 'does-not-exist')
                with self.assertRaisesRegex(ValueError, 'Existing assets'):
                    iso_import.import_iso(iso, root / 'output')
                self.assertTrue(target.is_symlink())
                target.unlink()

    def test_input_must_be_a_file(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'Input must'):
                iso_import.preflight(Path(directory), Path(directory) / 'output')

    def test_failed_stage_leaves_report_and_prevents_retry_overwrite(self):
        import json
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            iso = root / 'input.iso'
            iso.write_bytes(b'fixture')
            output = root / 'output'
            with patch.object(iso_import.shutil, 'which', return_value='/mock/tool'), \
                 patch.object(iso_import.subprocess, 'run', side_effect=ValueError('bad media')):
                with self.assertRaisesRegex(ValueError, 'bad media'):
                    iso_import.import_iso(iso, output)
            report = json.loads((output / 'assets/local/import-report.json').read_text())
            self.assertEqual(report['state'], 'failed')
            self.assertEqual(report['completed'], [])
            with self.assertRaisesRegex(ValueError, 'Existing assets'):
                iso_import.import_iso(iso, output)
