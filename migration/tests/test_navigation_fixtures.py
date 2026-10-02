#!/usr/bin/env python3
"""Validate pinned source-arithmetic fixture integrity and honest scope."""
import hashlib,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
FIX=ROOT/'migration/fixtures/legacy-navigation'
class Fixtures(unittest.TestCase):
 def test_hashes(self):
  m=json.loads((FIX/'manifest.json').read_text())
  self.assertEqual(m['source_commit'],'afa43fae3e920210146abed514f136fd49f671b5')
  self.assertIn('not GUI',m['scope'])
  for name,digest in m['files'].items(): self.assertEqual(hashlib.sha256((FIX/name).read_bytes()).hexdigest(),digest)
 def test_matrix(self):
  rows=[x.split('\t') for x in (FIX/'rotation.tsv').read_text().splitlines()]
  self.assertEqual(len(rows),5184)
  self.assertEqual({int(r[0]) for r in rows},{100,101})
  self.assertEqual({int(r[2]) for r in rows},{0,1})
  self.assertEqual({float(r[3]) for r in rows},{0,7.5,22.5,337.5,352.5,359.999})
  for r in rows: self.assertTrue(0<=float(r[9])<360)
 def test_zoom(self):
  folder=ROOT/'migration/fixtures/legacy-zoom'
  m=json.loads((folder/'manifest.json').read_text())
  for name,digest in m['files'].items(): self.assertEqual(hashlib.sha256((folder/name).read_bytes()).hexdigest(),digest)
  rows=[x.split('\t') for x in (folder/'zoom.tsv').read_text().splitlines()]
  self.assertEqual(len(rows),2400)
  self.assertTrue(any(float(r[9])<0 for r in rows))
  self.assertEqual({int(r[2]) for r in rows},{0,1})
if __name__=='__main__': unittest.main()
