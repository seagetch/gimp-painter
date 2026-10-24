#!/usr/bin/env python3
"""Focused metadata tests only; no held providers or native execution."""
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'migration/tests'))
import persistence_undo_successor as gate
import persistence_successors

class UndoSuccessor(unittest.TestCase):
    def setUp(self):
        self.row = gate.reviewed(ROOT)
        with tarfile.open(ROOT / gate.PACKAGE / 'sources.tar.gz') as tar:
            self.before = tar.extractfile('predecessor.h').read()
            self.after = tar.extractfile('successor.h').read()

    def test_semantic_invariants_and_causal_mutants(self):
        gate.exact_change(self.before, self.after)
        mutants = [
            self.after.replace(b'GIMP_UNDO_GROUP_NONE = 0', b'GIMP_UNDO_GROUP_NONE = 1'),
            self.after.replace(b'GIMP_UNDO_GROUP_LAST = GIMP_UNDO_GROUP_MISC', b'GIMP_UNDO_GROUP_LAST = 49'),
            self.after.replace(b'GIMP_UNDO_MYPAINT = 111', b'GIMP_UNDO_MYPAINT = 112'),
            self.after.replace(b'GIMP_VIEW_SIZE_TINY        = 12', b'GIMP_VIEW_SIZE_TINY        = 13'),
            self.after.replace(b'typedef enum /*< pdb-skip >*/', b'typedef enum /* exposed */'),
        ]
        for mutant in mutants:
            self.assertNotEqual(mutant, self.after)
            with self.assertRaises(AssertionError): gate.exact_change(self.before, mutant)

    def test_original_family_endpoint_and_unknown_drift(self):
        a,b = self.row['predecessor_sha256'],self.row['successor_sha256']
        self.assertEqual(persistence_successors.resolve(ROOT,'native',gate.PATH,a,b), self.row)
        for old,new in [('0'*64,b),(a,'0'*64)]:
            with self.assertRaises(AssertionError): gate.resolve_source(ROOT,old,new)
        with self.assertRaisesRegex(AssertionError,'unreviewed persistence source drift'):
            persistence_successors.resolve(ROOT,'native','unrelated/path',a,b)

    def test_source_and_proof_integrity(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);shutil.copytree(ROOT/gate.PACKAGE,root/gate.PACKAGE)
            (root/gate.PATH).parent.mkdir(parents=True)
            (root/gate.PATH).write_bytes(self.after)
            gate.reviewed(root)
            (root/gate.PATH).write_bytes(self.after+b'\n')
            with self.assertRaisesRegex(AssertionError,'unreviewed Undo header drift'): gate.reviewed(root)
            (root/gate.PATH).write_bytes(self.after)
            for name in gate.PINS:
                f=root/gate.PACKAGE/name;data=f.read_bytes();f.write_bytes(data+b' ')
                with self.assertRaisesRegex(AssertionError,'Undo successor proof changed'):gate.reviewed(root)
                f.write_bytes(data)
            (root/gate.PACKAGE/'transition.json').unlink()
            with self.assertRaises(FileNotFoundError):gate.reviewed(root)

if __name__=='__main__': unittest.main(verbosity=2)
