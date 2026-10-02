"""Verify the asset lock from original Git blobs, not decoded/re-encoded data."""
import importlib.util
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('freeze', ROOT / 'tools/freeze_legacy_assets.py')
freeze = importlib.util.module_from_spec(spec)
spec.loader.exec_module(freeze)


class LegacyAssetTests(unittest.TestCase):
    def test_frozen_manifest_is_reproducible(self):
        manifest, summary = freeze.render(freeze.blobs())
        self.assertEqual(manifest, (freeze.DEST / 'assets.tsv').read_text())
        self.assertEqual(summary, (freeze.DEST / 'manifest.json').read_text())
        self.assertEqual(json.loads(summary)['asset_count'], 386)

    def test_version_two_is_not_mistaken_for_json(self):
        row = freeze.asset_record('test.myb', '0'*40, b'# a comment\nversion 2\nradius 1.0\n')
        self.assertEqual(row['myb_version'], '2')
        with self.assertRaises(ValueError):
            freeze.asset_record('test.myb', '0'*40, b'# no version\n')

    def test_version_three_is_recorded_without_conversion(self):
        data = b'{ "version": 3, "unknown-extension": [1,2] }\n'
        row = freeze.asset_record('test.myb', '0'*40, data)
        self.assertEqual(row['myb_version'], '3')
        self.assertEqual(row['bytes'], str(len(data)))

    def test_materialize_refuses_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            Path(temp, 'keep').write_text('keep')
            result = subprocess.run([sys.executable, str(ROOT/'tools/freeze_legacy_assets.py'),
                                     '--materialize', temp], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(Path(temp, 'keep').read_text(), 'keep')


if __name__ == '__main__':
    unittest.main()
