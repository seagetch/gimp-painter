#!/usr/bin/env python3
"""Source-only controls; no paused provider callback diagnostics."""
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'migration/tests'))
import persistence_catalog_successor as gate
import persistence_successors as dispatch
class CatalogSuccessor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.record=gate.reviewed(ROOT)
        with tarfile.open(ROOT/gate.PACKAGE/'sources.tar.gz') as t:
            cls.before={gate.GIMP:t.extractfile('predecessor/'+gate.GIMP).read().decode()}
            cls.after={r['path']:t.extractfile('successor/'+r['path']).read().decode() for r in cls.record['providers']}
    def test_saved_metadata_and_dynamic_schema_controls(self):
        cases=[(gate.GIMP,'g_param_spec_boolean ("verbose"','g_param_spec_int ("verbose"'),
               (gate.GIMP,'g_value_get_boolean (value)','g_value_get_int (value)'),
               (gate.GIMP,'g_value_set_boolean (value','g_value_set_int (value'),
               (gate.GIMP,'gimp_unitrc_load (gimp);','gimp_unitrc_save (gimp);'),
               (gate.DESCRIPTOR,'GIMP_CONFIG_PATH_DIR_LIST','GIMP_CONFIG_PATH_FILE'),
               (gate.DESCRIPTOR,'G_PARAM_READWRITE','G_PARAM_READABLE'),
               (gate.DESCRIPTOR,'gimp_layer_preset_load','different_loader')]
        for p,a,b in cases:
            self.assertIn(a,self.after[p]);candidate=self.after.copy();candidate[p]=candidate[p].replace(a,b)
            with self.subTest(mutation=b),self.assertRaises(AssertionError):gate.semantic_proof(self.before,candidate)
    def test_exact_dispatch_endpoints(self):
        row=next(r for r in self.record['providers'] if r['path']==gate.GIMP)
        a,b=row['predecessor_sha256'],row['successor_sha256']
        result=dispatch.resolve(ROOT,'native',gate.GIMP,a,b)
        self.assertEqual(result['commits'],[self.record['successor_commit']])
        for before,after in [('0'*64,b),(a,'0'*64)]:
            with self.assertRaises(AssertionError):gate.resolve_source(ROOT,before,after)
    def test_all_sources_receipts_and_proof_bound(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);shutil.copytree(ROOT/gate.PACKAGE,root/gate.PACKAGE)
            for p,text in self.after.items():
                (root/p).parent.mkdir(parents=True,exist_ok=True);(root/p).write_text(text)
            for p in self.record['proof']:
                (root/p).parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/p,root/p)
            gate.reviewed(root)
            for p,text in self.after.items():
                (root/p).write_text(text+'\n/* unreviewed source */')
                with self.assertRaisesRegex(AssertionError,'unreviewed catalog source drift'):gate.reviewed(root)
                (root/p).write_text(text)
            for p in self.record['proof']:
                f=root/p;b=f.read_bytes();f.write_bytes(b+b' ')
                with self.assertRaisesRegex(AssertionError,'catalog runtime receipt changed'):gate.reviewed(root)
                f.write_bytes(b)
            for name in gate.PINS:
                f=root/gate.PACKAGE/name;b=f.read_bytes();f.write_bytes(b+b' ')
                with self.assertRaisesRegex(AssertionError,'catalog proof changed'):gate.reviewed(root)
                f.write_bytes(b)
            (root/gate.PACKAGE/'sources.tar.gz').unlink()
            with self.assertRaises(FileNotFoundError):gate.reviewed(root)
if __name__=='__main__':unittest.main(verbosity=2)
