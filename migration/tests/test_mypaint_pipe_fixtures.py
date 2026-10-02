#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import collections,gzip,hashlib,json,pathlib,unittest
from compare_mypaint_pipe import compare
ROOT=pathlib.Path(__file__).resolve().parents[2]
FIX=ROOT/'migration/fixtures/legacy-mypaint-pipe'
class PipeFixture(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw=gzip.decompress((FIX/'pipe-values.tsv.gz').read_bytes())
        cls.rows=[x.split() for x in cls.raw.decode().splitlines()]
        cls.report=json.loads((FIX/'capture.json').read_text())
        cls.states={(int(p[1]),p[2]):list(map(int,p[3:])) for p in cls.rows if p[0]=='PIPE_STATE'}
        cls.pixels={(int(p[1]),p[2]):bytes.fromhex(p[3]) for p in cls.rows if p[0]=='PIPE_PIX'}
    def test_provenance(self):
        self.assertEqual(self.report['source_commit'],'afa43fae3e920210146abed514f136fd49f671b5')
        self.assertEqual(self.report['exit_code'],0)
        self.assertEqual(self.report['values_sha256'],hashlib.sha256(self.raw).hexdigest())
        self.assertEqual(self.report['harness_sha256'],hashlib.sha256((FIX/'capture-pipe.cpp').read_bytes()).hexdigest())
        self.assertGreaterEqual(len(self.report['behavior_sha256']),6)
    def test_independent_reproduction(self):
        repeated=json.loads((FIX/'reproduction.json').read_text())
        self.assertTrue(repeated['reproduced_fixture'])
        self.assertEqual(repeated['values_sha256'],self.report['values_sha256'])
        self.assertEqual(repeated['harness_sha256'],self.report['harness_sha256'])
    def test_cold_nonincremental_failure_is_separate(self):
        cold=json.loads((FIX/'cold-nonincremental-drawable-switch/capture.json').read_text())
        self.assertEqual(cold['exit_code'],-11)
        self.assertNotEqual(cold['values_sha256'],self.report['values_sha256'])
    def test_complete_records(self):
        self.assertEqual(collections.Counter(p[0] for p in self.rows),{'PIPE_SELECT':1920,'PIPE_STATE':416,'PIPE_PIX':416,'PIPE_UNDO':32,'PIPE_RNG':32,'PIPE_CAPTURE_COMPLETE':1})
        self.assertEqual({int(p[1]) for p in self.rows if p[0]=='PIPE_PIX'},set(range(32)))
        self.assertTrue(all(len(p)==64*48*4 for p in self.pixels.values()))
    def test_only_warmed_previous_coordinates(self):
        for p in self.rows:
            if p[0]=='PIPE_SELECT':self.assertEqual(p[6],'1');self.assertEqual(len(p),21)
    def test_native_child_matches_index(self):
        for value in self.states.values():
            _,a,achild,b,bchild,_=value
            self.assertEqual(a,achild);self.assertEqual(b,bchild)
            self.assertIn(a,range(4));self.assertIn(b,range(4))
    def test_release_tail_changes_real_old_pixels(self):
        for n in range(32):self.assertNotEqual(self.pixels[n,'moving'],self.pixels[n,'hover-split'])
    def test_pure_hover_keeps_pixels(self):
        for n in range(32):
            self.assertEqual(self.pixels[n,'hover-split'],self.pixels[n,'hover-idle'])
            self.assertEqual(self.pixels[n,'hover-idle'],self.pixels[n,'idle-finish'])
    def test_hover_selects_zero_pressure_preserves_other_axes(self):
        for p in self.rows:
            if p[0]=='PIPE_SELECT' and p[2] in ('4','5'):
                current=p[-7:]
                self.assertEqual(current[2],'0');self.assertEqual(current[3:5],['-0.8','0.75'])
    def test_undo_does_not_restore_native_pipe_state(self):
        for n in range(32):
            self.assertEqual(self.states[n,'repeated'][:-1],self.states[n,'undo'][:-1]);self.assertEqual(self.states[n,'repeated'][:-1],self.states[n,'redo'][:-1])
            self.assertEqual(self.pixels[n,'repeated'],self.pixels[n,'redo']);self.assertNotEqual(self.pixels[n,'repeated'],self.pixels[n,'undo'])
    def test_explicit_finish_preserves_pixels_and_native_state(self):
        for n in range(32):
            self.assertEqual(self.states[n,'first'][:-1],self.states[n,'explicit-finish'][:-1]);self.assertEqual(self.pixels[n,'first'],self.pixels[n,'explicit-finish'])
    def test_comparator_detects_pre_fix_hover_regression(self):
        before=gzip.decompress((ROOT/'migration/tests/mypaint-pipe-before-fix.log.gz').read_bytes())
        report=compare(self.raw,before);self.assertFalse(report['equal']);self.assertGreater(report['different_records'],200)
    def test_comparator_omits_only_previous_coordinates(self):
        altered=self.raw.replace(b'18 18 0.85 -0.25 0.4 0.125 0.2',b'0 0 1 0 0 0 0',1)
        self.assertTrue(compare(self.raw,altered)['equal'])
        altered=self.raw.replace(b'PIPE_SELECT 0 0 1 A 0',b'PIPE_SELECT 0 0 1 A 1',1)
        self.assertFalse(compare(self.raw,altered)['equal'])
if __name__=='__main__':unittest.main()
