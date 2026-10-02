#!/usr/bin/env python3
"""Execute the exact pinned legacy selection-penalty expression, not GIMP.

This source-level counterexample is intentionally not labeled a runtime fixture.
"""
import ctypes
import json
import re
import subprocess
import tempfile
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

class BucketSelectionContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        baseline=json.loads((ROOT/'migration/baseline/baseline.json').read_text());source=baseline['source']['commit']
        cls.text=subprocess.check_output(['git','show',source+':app/core/gimpimage-contiguous-region.c'],cwd=ROOT).decode()
        start=cls.text.index('  if (src_mask) {',cls.text.index('pixel_difference (const guchar'))
        end=cls.text.index('\n}\n',start)
        tail=cls.text[start:end]
        assert 'max = (*src_mask * max + (255 - *src_mask) * 255) / 255;' in tail
        cls.tmp=tempfile.TemporaryDirectory(prefix='painter-bucket-contract-')
        path=Path(cls.tmp.name);c=path/'selection.c'
        c.write_text('typedef unsigned char guchar; typedef float gfloat;\nint legacy_coverage(float max, const unsigned char *src_mask, int antialias, int threshold)\n{\n'+tail+'\n}\n')
        subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fPIC','-shared',str(c),'-o',str(path/'selection.so')],check=True)
        cls.lib=ctypes.CDLL(str(path/'selection.so'));cls.f=cls.lib.legacy_coverage
        cls.f.argtypes=[ctypes.c_float,ctypes.POINTER(ctypes.c_ubyte),ctypes.c_int,ctypes.c_int];cls.f.restype=ctypes.c_int
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def coverage(self,difference,mask,threshold,antialias=1):
        m=ctypes.c_ubyte(mask)
        return self.f(difference,ctypes.byref(m),antialias,threshold)
    def test_zero_mask_usual_threshold_stops_traversal(self):
        self.assertEqual(self.coverage(0,0,30),0)
    def test_zero_mask_maximum_threshold_can_traverse(self):
        self.assertEqual(self.coverage(0,0,255),255)
        self.assertEqual(self.coverage(0,0,255,0),255)
    def test_soft_mask_is_not_boolean_or_post_multiply(self):
        self.assertEqual(self.coverage(0,128,30),0)
        self.assertEqual(self.coverage(0,128,100),117)
        self.assertEqual(self.coverage(0,230,30),255)
        self.assertNotEqual(self.coverage(0,128,100),128)
    def test_unmasked_and_fully_selected_match(self):
        for threshold in (0,1,30,100,255):
            for difference in range(256):
                self.assertEqual(self.f(difference,None,1,threshold),self.coverage(difference,255,threshold))
    def test_interior_selection_hole_changes_reachability(self):
        # Same-color 1D corridor inside selection bounding box: selected ends,
        # unselected middle. This isolates traversal from final display masking.
        def reachable(threshold,mask_during_search):
            masks=[255,0,255];seen={0};queue=[0]
            while queue:
                i=queue.pop()
                for j in (i-1,i+1):
                    if 0<=j<3 and j not in seen and self.coverage(0,masks[j] if mask_during_search else 255,threshold):
                        seen.add(j);queue.append(j)
            return {i for i in seen if masks[i]}
        self.assertEqual(reachable(30,True),{0})
        self.assertEqual(reachable(30,False),{0,2})
        self.assertEqual(reachable(255,True),{0,2})
    def test_transparency_and_seed_guards_precede_penalty(self):
        # Source-only witnesses: full RGB/HSV conversion and the old app were
        # not compiled into the isolated coverage test above.
        fn=self.text[self.text.index('// updated by gimp-painter 2.8\nstatic gint\npixel_difference'):]
        self.assertLess(fn.index('if (! select_transparent && has_alpha && col2[bytes - 1] == 0)'),fn.index('if (src_mask)'))
        self.assertRegex(self.text,r'if \(start\[bytes - 1\] > 0\)\s+select_transparent = FALSE;')
        self.assertIn('if (x1 >= x2 || y1 >= y2)',self.text)
if __name__=='__main__':unittest.main(verbosity=2)
