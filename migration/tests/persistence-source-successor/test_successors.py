#!/usr/bin/env python3
"""Focused metadata controls; no preview hooks, native GIMP, or runtime claims."""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'migration/tests'))
import persistence_successors as gate


class Successors(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / gate.PACKAGE, self.root / gate.PACKAGE)
        inventory = self.root / 'migration/inventory'
        inventory.mkdir(parents=True)
        for family in ('xcf', 'mypaint', 'config'):
            name = 'persistence-' + family + '-fields.json'
            shutil.copyfile(ROOT / 'migration/inventory' / name, inventory / name)
        self.rows = gate.reviewed(self.root)['transitions']
        self.row = next(r for r in self.rows if r['path'].endswith('/resource.cpp'))

    def resolve(self, **changes):
        row = dict(self.row, **changes)
        return gate.resolve(self.root, row['revision'], row['path'], row['predecessor_sha256'], row['successor_sha256'])

    def test_exact_seven_successors(self):
        for row in self.rows:
            self.assertEqual(gate.resolve(self.root, row['revision'], row['path'], row['predecessor_sha256'], row['successor_sha256']), row)

    def test_unchanged_source_keeps_original_binding(self):
        self.assertIsNone(gate.resolve(self.root, 'native', 'synthetic-unmodified.c', 'a'*64, 'a'*64))

    def test_unknown_source_rejected(self):
        with self.assertRaisesRegex(AssertionError, 'unreviewed persistence source drift'):
            self.resolve(path='synthetic-unreviewed.c')

    def test_unknown_current_bytes_rejected(self):
        with self.assertRaisesRegex(AssertionError, 'unreviewed persistence successor'):
            self.resolve(successor_sha256=hashlib.sha256(b'unreviewed resource bytes').hexdigest())

    def test_wrong_predecessor_rejected(self):
        with self.assertRaisesRegex(AssertionError, 'wrong persistence predecessor'):
            self.resolve(predecessor_sha256='0'*64)

    def test_legacy_revision_not_admitted(self):
        with self.assertRaisesRegex(AssertionError, 'unreviewed persistence source drift'):
            self.resolve(revision='legacy')

    def test_missing_transition_rejected(self):
        path = self.root / gate.PACKAGE / 'transitions.json'
        data = json.loads(path.read_text())
        data['transitions'].remove(self.row)
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(AssertionError, 'successor proof changed'):
            self.resolve()

    def test_forged_commit_rejected(self):
        path = self.root / gate.PACKAGE / 'transitions.json'
        data = json.loads(path.read_text())
        data['transitions'][0]['commits'][0] = '0'*40
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(AssertionError, 'successor proof changed'):
            self.resolve()

    def test_archived_predecessor_tamper_rejected(self):
        path = self.root / gate.PACKAGE / 'source-transitions.tar.gz'
        path.write_bytes(path.read_bytes() + b'tampered')
        with self.assertRaisesRegex(AssertionError, 'successor proof changed'):
            self.resolve()

    def test_original_field_removal_rejected(self):
        path = self.root / 'migration/inventory/persistence-mypaint-fields.json'
        data = json.loads(path.read_text()); data['fields'].pop()
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(AssertionError, 'predecessor family changed'):
            self.resolve()

    def test_original_seal_replacement_rejected(self):
        path = self.root / 'migration/inventory/persistence-mypaint-fields.json'
        data = json.loads(path.read_text())
        source = next(s for s in data['source_files'] if s['path'] == self.row['path'])
        source['sha256'] = self.row['successor_sha256']
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(AssertionError, 'predecessor family changed'):
            self.resolve()

    def test_missing_archive_rejected(self):
        (self.root / gate.PACKAGE / 'predecessor-ledgers.tar.gz').unlink()
        with self.assertRaises(FileNotFoundError):
            self.resolve()


if __name__ == '__main__':
    unittest.main(verbosity=2)
