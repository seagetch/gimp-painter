#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline integrity tests for the independently captured old editor previews."""
import gzip,hashlib,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
FIX=ROOT/'migration/fixtures/legacy-mypaint-preview'
class PreviewFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report=json.loads((FIX/'capture.json').read_text())
        cls.values=gzip.decompress((FIX/'preview-values.tsv.gz').read_bytes())
        cls.lines=cls.values.splitlines()
    def test_provenance(self):
        self.assertEqual(self.report['pin'],'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(self.report['returncode'],0)
        self.assertEqual(self.report['harness_sha256'],hashlib.sha256((FIX/'capture-preview.cpp').read_bytes()).hexdigest())
        self.assertEqual(self.report['base_sha256'],hashlib.sha256((ROOT/'migration/fixtures/legacy-mypaint-session/session-base.myb').read_bytes()).hexdigest())
        self.assertIn('app/core/gimpmypaintbrush.cpp',self.report['feature_sha256'])
        self.assertTrue(self.report['archives_sha256'])
        self.assertIn('settings->get_new_preview',(FIX/'capture-preview.cpp').read_text())
    def test_complete_records(self):
        self.assertEqual(len(self.lines),33)
        self.assertEqual(self.lines[-1],b'PREVIEW_CAPTURE_COMPLETE')
        self.assertEqual(len(self.values),self.report['bytes'])
        self.assertEqual(hashlib.sha256(self.values).hexdigest(),self.report['values_sha256'])
    def test_every_repeat_equal(self):
        repeats=[line.split() for line in self.lines if line.startswith(b'PREVIEW_REPEAT ')]
        self.assertEqual([int(line[1])for line in repeats],list(range(16)))
        self.assertTrue(all(line[2]==b'1'for line in repeats))
    def test_full_pixels_match_comparator_hashes(self):
        expected={int(n):digest for n,digest in (line.split()for line in (FIX/'preview-hashes.tsv').read_text().splitlines())}
        self.assertEqual(set(expected),set(range(16)))
        actual={}
        for line in self.lines:
            if not line.startswith(b'PREVIEW_PIX '):continue
            _,n,encoded=line.split();pixels=bytes.fromhex(encoded.decode())
            self.assertEqual(len(pixels),256*256*4)
            actual[int(n)]=hashlib.sha256(pixels).hexdigest()
        self.assertEqual(actual,expected)
        self.assertEqual(len(set(actual.values())),16)
if __name__=='__main__':unittest.main()
