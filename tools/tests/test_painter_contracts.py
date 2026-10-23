#!/usr/bin/env python3
"""Negative controls for reviewed bounded handle claims, without modifying repo."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import tarfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('contracts', ROOT / 'tools/check_painter_contracts.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


class HandleProofTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.rows = c.read('migration/contracts/cpp-handle-registry.tsv')
        for proof in [q for p in c.BOUNDED_HANDLE_PROOFS.values()
                      for q in p.get('predecessors', []) + [p]]:
            source = ROOT / proof['directory']
            dest = self.root / proof['directory']
            dest.mkdir(parents=True, exist_ok=True)
            for name in proof['files']:
                shutil.copyfile(source / name, dest / name)
            # Immutable tested checkpoint: current source evolution is checked
            # separately by the aggregate, not silently blessed by this fixture.
            with tarfile.open(source / next(n for n in proof['files'] if n.endswith('.tar.gz'))) as archive:
                for item in json.loads((source / 'source-manifest.json').read_text()):
                    target = self.root / item['path']
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(archive.extractfile(item['path']).read())

    def check(self, rows=None):
        return c.verify_handle_implementations(self.rows if rows is None else rows, self.root)

    def test_reviewed_claims_and_original_todos_pass(self):
        self.assertEqual([], self.check())

    def test_unknown_claim_rejected(self):
        row = next(r for r in self.rows if r['implementation'] == 'TODO')
        row['implementation'] = 'IMPLEMENTED bounded native; tests PASS'
        self.assertTrue(self.check())

    def test_every_reviewed_row_field_is_bound(self):
        for name in c.BOUNDED_HANDLE_PROOFS:
            for field in self.rows[0]:
                with self.subTest(name=name, field=field):
                    rows = copy.deepcopy(self.rows)
                    row = next(r for r in rows if r['legacy_type'] == name)
                    row[field] += ' forged'
                    self.assertTrue(self.check(rows))

    def test_receipts_missing_or_changed_rejected(self):
        for proof in [q for p in c.BOUNDED_HANDLE_PROOFS.values()
                      for q in p.get('predecessors', []) + [p]]:
            for filename in proof['files']:
                with self.subTest(proof=proof['directory'], filename=filename):
                    path = self.root / proof['directory'] / filename
                    data = path.read_bytes()
                    path.unlink()
                    self.assertTrue(self.check())
                    path.write_bytes(data + b'\n# tampered')
                    self.assertTrue(self.check())
                    path.write_bytes(data)

    def test_current_source_drift_rejected(self):
        for proof in c.BOUNDED_HANDLE_PROOFS.values():
            manifest = json.loads((self.root / proof['directory'] / 'source-manifest.json').read_text())
            for item in manifest:
                with self.subTest(source=item['path']):
                    path = self.root / item['path']
                    data = path.read_bytes()
                    path.write_bytes(data + b'\n/* untested change */\n')
                    self.assertTrue(self.check())
                    path.write_bytes(data)

    def test_reviewed_claim_rollback_rejected(self):
        for row in self.rows:
            if row['legacy_type'] in c.BOUNDED_HANDLE_PROOFS:
                row['implementation'] = 'TODO'
        self.assertEqual(len(c.BOUNDED_HANDLE_PROOFS), len(self.check()))

    def test_incomplete_todo_contract_rejected(self):
        next(r for r in self.rows if r['implementation'] == 'TODO')['contract'] = ''
        self.assertTrue(self.check())

    def test_full_historical_contract_coverage(self):
        # Isolate original inventory coverage from independently tested proof
        # validation; the CLI remains the mandatory current-source integration.
        with mock.patch.object(c, 'verify_handle_implementations', return_value=[]):
            errors, handles, entries = c.verify()
        self.assertEqual([], errors)
        self.assertEqual((90, 76), (handles, entries))


if __name__ == '__main__':
    unittest.main()
