"""Seal/provenance checks for the real legacy MyPaint reader/engine corpus."""
import gzip
import hashlib
import json
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / 'migration/fixtures/legacy-mypaint'
class MyPaintFixtures(unittest.TestCase):
    def test_sealed_capture_files(self):
        manifest=json.loads((FIXTURES/'manifest.json').read_text())
        self.assertEqual(manifest['source_commit'],'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(set(manifest['files']),{p.name for p in FIXTURES.iterdir() if p.is_file() and p.name!='manifest.json'})
        for name,entry in manifest['files'].items():
            with self.subTest(file=name):
                data=(FIXTURES/name).read_bytes()
                self.assertEqual(len(data),entry['bytes'])
                self.assertEqual(hashlib.sha256(data).hexdigest(),entry['sha256'])
    def test_exact_pinned_asset_bytes(self):
        manifest=json.loads((FIXTURES/'assets.json').read_text())
        assets=ROOT/'data/painter-mypaint-brushes'
        self.assertEqual(len(manifest['assets']),363)
        self.assertEqual(sum(x['path'].endswith('.myb') for x in manifest['assets']),177)
        for entry in manifest['assets']:
            with self.subTest(asset=entry['path']):
                data=(assets/entry['path']).read_bytes()
                self.assertEqual(len(data),entry['bytes'])
                self.assertEqual(hashlib.sha256(data).hexdigest(),entry['sha256'])
    def test_parser_capture_is_independent_and_complete(self):
        report=json.loads((FIXTURES/'resource-capture.json').read_text())
        raw=gzip.decompress((FIXTURES/'resource-capture.log.gz').read_bytes())
        values=b'\n'.join(line for line in raw.splitlines() if line.startswith(b'MYB '))+b'\n'
        self.assertEqual(values,gzip.decompress((FIXTURES/'resource-values.tsv.gz').read_bytes()))
        self.assertEqual(hashlib.sha256(values).hexdigest(),report['oracle_values_sha256'])
        self.assertEqual(len(values.splitlines()),81420)
        self.assertEqual(sum(line.startswith(b'MYB B ') for line in values.splitlines()),177)
        self.assertIn('app/core/gimpmypaintbrush-load.cpp',report['behavior_source_sha256'])
    def test_evaluator_capture_is_separate(self):
        report=json.loads((ROOT/'migration/tests/mypaint-engine-comparison.json').read_text())
        trace=gzip.decompress((FIXTURES/'engine.trace.gz').read_bytes())
        self.assertTrue(report['equal'])
        self.assertEqual(len(trace),5637911)
        self.assertEqual(hashlib.sha256(trace).hexdigest(),report['legacy_sha256'])
        lines=trace.splitlines()
        self.assertEqual(sum(x.startswith(b'B ') for x in lines),178)
        self.assertEqual(sum(x.startswith(b'D ') for x in lines),27392)
        self.assertEqual(sum(x.startswith(b'C ') for x in lines),3172)
        self.assertEqual(sum(x.startswith(b'S ') for x in lines),178*16)
if __name__ == '__main__': unittest.main()
