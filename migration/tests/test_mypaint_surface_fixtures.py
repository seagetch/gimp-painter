"""Integrity and provenance checks, not a substitute for runtime pixel tests."""
import gzip, hashlib, json
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[2]
FIXTURES=ROOT/'migration/fixtures/legacy-mypaint-surface'
class SurfaceFixtures(unittest.TestCase):
    def test_sealed_files(self):
        manifest=json.loads((FIXTURES/'manifest.json').read_text())
        self.assertEqual(manifest['source_commit'],'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(set(manifest['files']),{p.name for p in FIXTURES.iterdir() if p.is_file() and p.name!='manifest.json'})
        for name,entry in manifest['files'].items():
            with self.subTest(file=name):
                data=(FIXTURES/name).read_bytes();self.assertEqual(len(data),entry['bytes']);self.assertEqual(hashlib.sha256(data).hexdigest(),entry['sha256'])
    def test_actual_old_capture_records(self):
        report=json.loads((FIXTURES/'capture.json').read_text())
        self.assertIn('app/paint/gimpmypaintcore-surface.cpp',report['behavior_source_sha256'])
        self.assertIn('app/core/gimpbrushgenerated.c',report['behavior_source_sha256'])
        for part,tags,count in [('surface',(b'PIX ',b'SAMPLE ',b'DAB '),148),('generated',(b'GENERATED ',),72)]:
            raw=gzip.decompress((FIXTURES/(part+'-capture.log.gz')).read_bytes())
            values=b'\n'.join(x for x in raw.splitlines() if x.startswith(tags))+b'\n'
            self.assertEqual(values,gzip.decompress((FIXTURES/(part+'-values.tsv.gz')).read_bytes()))
            self.assertEqual(len(values.splitlines()),count)
            self.assertEqual(hashlib.sha256(values).hexdigest(),report['captures'][part]['values_sha256'])
        self.assertEqual(len((FIXTURES/'shape-masks.tsv').read_text().splitlines()),24)
    def test_transparent_lock_observation(self):
        raw=gzip.decompress((FIXTURES/'surface-values.tsv.gz').read_bytes())
        cases={line.split()[1]:bytes.fromhex(line.split()[2].decode()) for line in raw.splitlines() if line.startswith(b'PIX transparent-lock-')}
        offset=(8*23+11)*4
        self.assertEqual(cases[b'transparent-lock-0'][offset:offset+4],bytes([0,0,0,0]))
        self.assertEqual(cases[b'transparent-lock-1'][offset:offset+4],bytes([33,99,177,0]))
        for values in cases.values():self.assertEqual(values[:4],bytes([33,99,177,0]))
if __name__=='__main__':unittest.main()
