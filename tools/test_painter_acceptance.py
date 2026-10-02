#!/usr/bin/env python3
"""Negative checks keep acceptance evidence from becoming a checkbox shortcut."""
from copy import deepcopy
import json
from pathlib import Path
import unittest
from check_painter_acceptance import ROOT,validate
class Acceptance(unittest.TestCase):
    def setUp(self):self.matrix=json.loads((ROOT/'migration/acceptance/foundation.json').read_text())
    def test_current(self):self.assertEqual(validate(matrix=self.matrix),[])
    def test_missing_and_duplicate(self):
        self.matrix['rows'].pop();self.assertTrue(validate(matrix=self.matrix))
        self.matrix['rows'].append(deepcopy(self.matrix['rows'][0]));self.assertTrue(validate(matrix=self.matrix))
    def test_no_test_is_not_acceptance(self):
        self.matrix['rows'][0]['test_ids']=[];self.assertTrue(validate(matrix=self.matrix))
    def test_wrong_test_or_source(self):
        self.matrix['rows'][0]['test_ids']=['/painter/not-a-real-test'];self.assertTrue(validate(matrix=self.matrix))
        self.setUp();self.matrix['rows'][0]['implementation_paths']=['app/painter/filter-edge.cpp'];self.assertTrue(validate(matrix=self.matrix))
    def test_dependencies_do_not_disappear(self):
        self.matrix['rows'][0]['dependencies']='none';self.assertTrue(validate(matrix=self.matrix))
if __name__=='__main__':unittest.main()
