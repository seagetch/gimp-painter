#!/usr/bin/env python3
"""Focused source/receipt controls, no native or held-candidate execution."""
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'migration/tests'))
import persistence_delete_successor as gate


class DeleteSuccessor(unittest.TestCase):
    def setUp(self):
        self.table = gate.reviewed(ROOT)
        with tarfile.open(ROOT / gate.PACKAGE / 'sources.tar.gz') as tar:
            self.before = tar.extractfile('predecessor.c').read()
            self.after = tar.extractfile('successor.c').read()

    def test_only_exact_one_line_is_admitted(self):
        gate.exact_change(self.before, self.after)
        for after in (self.before, self.after + b'\n/* extra edit */', self.after.replace(gate.AFTER, b'other_call ()')):
            with self.subTest(candidate=after[-40:]), self.assertRaises(AssertionError):
                gate.exact_change(self.before, after)

    def test_chain_and_wrong_endpoints(self):
        old = json.loads((ROOT / 'migration/tests/persistence-source-successor/transitions.json').read_text())
        row = next(r for r in old['transitions'] if r['path'] == gate.PATH)
        result = gate.resolve_source(ROOT, row, self.table['successor_sha256'])
        self.assertEqual(result['intermediate_sha256'], row['successor_sha256'])
        self.assertEqual(result['commits'][-1], self.table['successor_commit'])
        with self.assertRaisesRegex(AssertionError, 'predecessor differs'):
            gate.resolve_source(ROOT, dict(row, successor_sha256='0'*64), self.table['successor_sha256'])
        with self.assertRaisesRegex(AssertionError, 'successor differs'):
            gate.resolve_source(ROOT, row, '0'*64)

    def test_unknown_source_missing_transition_and_receipt_tamper(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            shutil.copytree(ROOT / gate.PACKAGE, root / gate.PACKAGE)
            (root / gate.PATH).parent.mkdir(parents=True)
            (root / gate.PATH).write_bytes(self.after)
            for path in self.table['proof']:
                (root / path).parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / path, root / path)
            gate.reviewed(root)
            (root / gate.PATH).write_bytes(self.after + b'\n/* unrelated source */')
            with self.assertRaisesRegex(AssertionError, 'unreviewed tools source drift'):
                gate.reviewed(root)
            (root / gate.PATH).write_bytes(self.after)
            for path in self.table['proof']:
                old = (root / path).read_bytes(); (root / path).write_bytes(old + b' ')
                with self.subTest(path=path), self.assertRaisesRegex(AssertionError, 'runtime receipt changed'):
                    gate.reviewed(root)
                (root / path).write_bytes(old)
            (root / gate.PACKAGE / 'transition.json').unlink()
            with self.assertRaises(FileNotFoundError): gate.reviewed(root)


if __name__ == '__main__':
    unittest.main(verbosity=2)
