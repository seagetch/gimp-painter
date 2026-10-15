#!/usr/bin/env python3
"""Negative controls for bounded-read provenance and deliberately open parent."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('buffer_read', ROOT / 'tools/check_drawable_buffer_read.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class SourceDutyTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(checker.MANIFEST.read_text())

    def rejects(self, edit):
        broken = copy.deepcopy(self.manifest)
        edit(broken)
        with self.assertRaises(ValueError):
            checker.validate(broken)

    def test_full_pinned_coverage(self):
        self.assertEqual(checker.validate(self.manifest), 48)

    def test_missing_duty(self):
        self.rejects(lambda m: m['duties'].pop())

    def test_duplicate_duty(self):
        self.rejects(lambda m: m['duties'].__setitem__(0, copy.deepcopy(m['duties'][1])))

    def test_changed_source_identity(self):
        for key in ('source_id', 'path', 'source_blob', 'source_range', 'source_sha256', 'blob_sha256'):
            with self.subTest(key=key):
                self.rejects(lambda m: m['duties'][0].__setitem__(key, 'changed'))

    def test_parent_cannot_be_closed(self):
        self.rejects(lambda m: m.__setitem__('parent_complete', True))

    def test_remaining_duty_cannot_be_dropped(self):
        self.rejects(lambda m: m['duties'][0].__setitem__('remaining_tasks', []))

    def test_fill_cannot_be_called_implemented(self):
        self.rejects(lambda m: next(d for d in m['duties'] if d['scope'] == 'unimplemented-fill-path').__setitem__('scope', 'implemented'))

    def test_fill_successor_cannot_be_dropped(self):
        self.rejects(lambda m: next(d for d in m['duties'] if d['scope'] == 'unimplemented-fill-path').__setitem__('remaining_tasks', ['09.003']))

    def test_missing_disposition(self):
        self.rejects(lambda m: m['duties'][0].__setitem__('source_duty', ''))


if __name__ == '__main__':
    unittest.main()
