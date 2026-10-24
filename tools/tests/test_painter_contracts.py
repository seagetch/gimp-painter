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
        # The Undo validator replays its immutable native/engine predecessors.
        # Copy evidence only, never overwrite successor provider sources.
        for name in ['native-stroke-undo-09011', 'paired-stroke-09011',
                     'prepared-pixels-09011', 'engine-historical-09011',
                     'engine-prepared-09011', 'engine-checkpoint-09011']:
            shutil.copytree(ROOT / 'migration/tests' / name,
                            self.root / 'migration/tests' / name,
                            ignore=shutil.ignore_patterns('__pycache__'))
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

    def test_bounded_c_entry_claim_fields_are_bound(self):
        rows = c.read('migration/contracts/cpp-c-entry-map.tsv')
        self.assertEqual([], c.verify_bounded_entry_implementations(rows))
        original = next(r for r in rows if r['c_entry'] == 'gimp_mypaint_core_undo_get_type')
        for field in original:
            with self.subTest(field=field):
                altered = copy.deepcopy(rows)
                next(r for r in altered if r['c_entry'] == original['c_entry'])[field] += ' forged'
                self.assertTrue(c.verify_bounded_entry_implementations(altered))
        self.assertTrue(c.verify_bounded_entry_implementations([r for r in rows if r is not original]))
        self.assertTrue(c.verify_bounded_entry_implementations(rows + [original]))

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
                    self.assertTrue(self.check([row]))

    def test_receipts_missing_or_changed_rejected(self):
        for owner, proof in [(name, q) for name, p in c.BOUNDED_HANDLE_PROOFS.items()
                             for q in p.get('predecessors', []) + [p]]:
            rows = [r for r in self.rows if r['legacy_type'] == owner]
            self.assertEqual([], self.check(rows))
            for filename in proof['files']:
                with self.subTest(proof=proof['directory'], filename=filename):
                    path = self.root / proof['directory'] / filename
                    data = path.read_bytes()
                    path.unlink()
                    self.assertTrue(self.check(rows))
                    path.write_bytes(data + b'\n# tampered')
                    self.assertTrue(self.check(rows))
                    path.write_bytes(data)

    def test_transitive_replay_receipts_missing_or_changed_rejected(self):
        proof = c.BOUNDED_HANDLE_PROOFS['GimpMypaintCoreUndo']
        rows = [r for r in self.rows if r['legacy_type'] == 'GimpMypaintCoreUndo']
        self.assertEqual([], self.check(rows))
        for filename in proof['support_files']:
            with self.subTest(filename=filename):
                path = self.root / filename
                data = path.read_bytes()
                path.unlink()
                self.assertTrue(self.check(rows))
                path.write_bytes(data + b'\n# tampered')
                self.assertTrue(self.check(rows))
                path.write_bytes(data)

    def test_current_source_drift_rejected(self):
        for owner, proof in c.BOUNDED_HANDLE_PROOFS.items():
            rows = [r for r in self.rows if r['legacy_type'] == owner]
            self.assertEqual([], self.check(rows))
            manifest = json.loads((self.root / proof['directory'] / 'source-manifest.json').read_text())
            for item in manifest:
                with self.subTest(source=item['path']):
                    path = self.root / item['path']
                    data = path.read_bytes()
                    path.write_bytes(data + b'\n/* untested change */\n')
                    self.assertTrue(self.check(rows))
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
