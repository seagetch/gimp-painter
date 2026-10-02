#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip,hashlib,json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'migration/fixtures/legacy-bounded-fill'
class FixtureTest(unittest.TestCase):
 def test_hashes(self):
  for name,h in json.loads((ROOT/'runtime.json').read_text())['fixture_sha256'].items():self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),h,name)
 def test_mask_completeness(self):
  lines=gzip.decompress((ROOT/'masks.tsv.gz').read_bytes()).splitlines();self.assertEqual(len(lines),257);self.assertEqual(lines[-1],b'FILL_CAPTURE_COMPLETE')
  for i,line in enumerate(lines[:-1]):
   _,case,phase,data=line.split();self.assertEqual(int(case),i//2);self.assertEqual(phase,b'grow' if i%2 else b'search');self.assertEqual(len(bytes.fromhex(data.decode())),130*70)
 def test_raw_normalization(self):
  raw=gzip.decompress((ROOT/'runtime-capture.log.gz').read_bytes());expected=b'\n'.join(x for x in raw.splitlines() if x.startswith(b'MASK ') or x==b'FILL_CAPTURE_COMPLETE')+b'\n';self.assertEqual(expected,gzip.decompress((ROOT/'masks.tsv.gz').read_bytes()))
if __name__=='__main__':unittest.main()
