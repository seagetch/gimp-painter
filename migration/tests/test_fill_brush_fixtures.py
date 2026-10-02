#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip,hashlib,json,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]/'migration/fixtures/legacy-fill-brush'
class FixtureTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  lines=gzip.decompress((ROOT/'pixels.tsv.gz').read_bytes()).splitlines();assert len(lines)==37 and lines[-1]==b'FILL_BRUSH_CAPTURE_COMPLETE'
  cls.p={}
  for line in lines[:-1]:
   _,case,phase,data=line.split();cls.p[int(case),phase.decode()]=bytes.fromhex(data.decode())
 def test_hashes(self):
  for name,h in json.loads((ROOT/'runtime.json').read_text())['fixture_sha256'].items():self.assertEqual(hashlib.sha256((ROOT/name).read_bytes()).hexdigest(),h,name)
 def test_rate_has_no_effect(self):
  for group in range(4):
   for i in range(1,3):self.assertEqual(self.p[group*3,'finish'],self.p[group*3+i,'finish'])
 def test_undo_redo_exact(self):
  initial=bytearray()
  for y in range(70):
   for x in range(90):
    v=200 if x==47 and 4<y<65 else 70;initial.extend([v,v+15,v+30,255])
  for case in range(12):
   self.assertEqual(self.p[case,'undo'],initial);self.assertNotEqual(self.p[case,'finish'],initial);self.assertEqual(self.p[case,'finish'],self.p[case,'redo'])
 def test_raw_normalization(self):
  raw=gzip.decompress((ROOT/'runtime-capture.log.gz').read_bytes());normalized=b'\n'.join(x for x in raw.splitlines() if x.startswith(b'BRUSH ') or x==b'FILL_BRUSH_CAPTURE_COMPLETE')+b'\n';self.assertEqual(normalized,gzip.decompress((ROOT/'pixels.tsv.gz').read_bytes()))
if __name__=='__main__':unittest.main()
