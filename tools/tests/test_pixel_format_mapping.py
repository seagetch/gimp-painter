#!/usr/bin/env python3
"""Fail-closed source/format ledger mutation controls for task 09.001."""
import copy,importlib.util,json,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('formats',Path(__file__).resolve().parents[1]/'check_pixel_format_mapping.py')
M=importlib.util.module_from_spec(spec);spec.loader.exec_module(M)
class MappingTests(unittest.TestCase):
    def setUp(self):self.manifest=json.loads(M.MANIFEST.read_text())
    def test_original(self):M.validate(self.manifest)
    def test_missing_duty(self):
        self.manifest['duties'].pop()
        with self.assertRaises(AssertionError):M.validate(self.manifest)
    def test_duplicate_duty(self):
        self.manifest['duties'][0]=self.manifest['duties'][1]
        with self.assertRaises(AssertionError):M.validate(self.manifest)
    def test_changed_source_identity(self):
        for key in ('source_id','source_blob','source_range','source_sha256','blob_sha256'):
            with self.subTest(key=key):
                bad=copy.deepcopy(self.manifest);bad['duties'][0][key]='invalid'
                with self.assertRaises(AssertionError):M.validate(bad)
    def test_missing_format(self):
        self.manifest['formats'].pop()
        with self.assertRaises(AssertionError):M.validate(self.manifest)
    def test_duplicate_format(self):
        self.manifest['formats'][0]=self.manifest['formats'][1]
        with self.assertRaises(AssertionError):M.validate(self.manifest)
    def test_encoding_and_layout(self):
        for key,value in [('encoding','RGBA u8'),('bytes',99),('alpha',1)]:
            with self.subTest(key=key):
                bad=copy.deepcopy(self.manifest);bad['formats'][0][key]=value
                with self.assertRaises(AssertionError):M.validate(bad)
    def test_indexed_high_precision_forbidden(self):
        self.manifest['indexed']['precision']='FLOAT_LINEAR'
        with self.assertRaises(AssertionError):M.validate(self.manifest)
    def test_indexed_layout_not_rgb(self):
        self.manifest['indexed']['bytes']=[3,4]
        with self.assertRaises(AssertionError):M.validate(self.manifest)
if __name__=='__main__':unittest.main(verbosity=2)
