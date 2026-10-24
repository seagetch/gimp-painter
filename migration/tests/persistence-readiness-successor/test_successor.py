#!/usr/bin/env python3
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'migration/tests'))
import persistence_readiness_successor as gate
import persistence_successors as oldgate
import persistence_enum_successor as enumgate

class ReadinessSuccessor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.record=gate.reviewed(ROOT)
        with tarfile.open(ROOT/gate.PACKAGE/'sources.tar.gz') as t:
            cls.before={r['path']:t.extractfile('predecessor/'+r['path']).read().decode() for r in cls.record['providers']}
            cls.after={r['path']:t.extractfile('successor/'+r['path']).read().decode() for r in cls.record['providers']}

    def test_semantics_and_saved_metadata_mutants(self):
        gate.semantic_proof(self.before,self.after)
        cases=[(gate.OPTIONS,'GIMP_TYPE_MYPAINT_BRUSH_MODE,0,flags)','GIMP_TYPE_MYPAINT_BRUSH_MODE,1,flags)'),
               (gate.OPTIONS,'i.legacy_brush_mode=g_value_get_enum(value)','i.legacy_brush_mode=0'),
               (gate.OPTIONS,'g_value_set_enum(value,i.legacy_brush_mode)','g_value_set_enum(value,0)'),
               (gate.OPTIONS,'i.legacy_adopted ? TRUE : FALSE','i.selected ? TRUE : FALSE'),
               (gate.CONTEXT,'GIMP_CONFIG_PROP_DOUBLE','GIMP_CONFIG_PROP_ENUM'),
               (gate.CONTEXT,'G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_HOOKS','G_SIGNAL_RUN_LAST')]
        for p,a,b in cases:
            self.assertIn(a,self.after[p]);candidate=self.after.copy();candidate[p]=candidate[p].replace(a,b,1)
            with self.subTest(path=p,mutation=b),self.assertRaises(AssertionError):gate.semantic_proof(self.before,candidate)

    def test_exact_original_and_enum_chain_endpoints(self):
        for row in self.record['providers']:
            p=row['path'];a=row['predecessor_sha256'];b=row['successor_sha256']
            self.assertEqual(gate.provider(ROOT,p,a,b)[1],row)
            for old,new in [('0'*64,b),(a,'0'*64)]:
                with self.assertRaises(AssertionError):gate.provider(ROOT,p,old,new)
        c=next(r for r in self.record['providers'] if r['path']==gate.CONTEXT)
        out=oldgate.resolve(ROOT,'native',gate.CONTEXT,c['predecessor_sha256'],c['successor_sha256'])
        self.assertEqual(out['commits'],[self.record['successor_commit']])
        row=next(r for r in oldgate.reviewed(ROOT)['transitions'] if r['path']==gate.OPTIONS)
        current=next(r for r in self.record['providers'] if r['path']==gate.OPTIONS)
        out=enumgate.resolve_source(ROOT,row,current['successor_sha256'])
        self.assertEqual(out['commits'][-2:],[enumgate.reviewed(ROOT)['successor_commit'],self.record['successor_commit']])
        self.assertEqual(out['predecessor_sha256'],row['predecessor_sha256'])

    def test_every_source_and_receipt_is_bound(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);shutil.copytree(ROOT/gate.PACKAGE,root/gate.PACKAGE)
            for p,text in self.after.items():
                (root/p).parent.mkdir(parents=True,exist_ok=True);(root/p).write_text(text)
            for p in self.record['proof']:
                (root/p).parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/p,root/p)
            gate.reviewed(root)
            for p,text in self.after.items():
                (root/p).write_text(text+'\n/* unknown edit */')
                with self.assertRaisesRegex(AssertionError,'unreviewed readiness source drift'):gate.reviewed(root)
                (root/p).write_text(text)
            for p in self.record['proof']:
                f=root/p;b=f.read_bytes();f.write_bytes(b+b' ')
                with self.assertRaisesRegex(AssertionError,'readiness runtime receipt changed'):gate.reviewed(root)
                f.write_bytes(b)
            f=root/gate.PACKAGE/'transition.json';f.write_bytes(f.read_bytes()+b' ')
            with self.assertRaisesRegex(AssertionError,'readiness proof changed'):gate.reviewed(root)
            f.unlink()
            with self.assertRaises(FileNotFoundError):gate.reviewed(root)

if __name__=='__main__':unittest.main(verbosity=2)
