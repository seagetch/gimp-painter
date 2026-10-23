#!/usr/bin/env python3
"""Mutation controls for source-duty and native TAP evidence validation."""
import copy
import importlib.util
import json
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    'pixbuf_owner', Path(__file__).resolve().parents[1] / 'check_popup_pixbuf_owner.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)


class PixbufOwnerEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(M.MANIFEST.read_text())
        self.tap = '1..8\n' + '\n'.join(
            f'ok {i} {name}' for i, name in enumerate(sorted(M.EXPECTED), 1))

    def test_current_contract(self):
        M.validate(self.manifest)

    def test_missing_or_duplicate_duty(self):
        bad = copy.deepcopy(self.manifest)
        bad['duties'].pop()
        with self.assertRaises(AssertionError):
            M.validate(bad)
        bad = copy.deepcopy(self.manifest)
        bad['duties'][0] = bad['duties'][1]
        with self.assertRaises(AssertionError):
            M.validate(bad)

    def test_wrong_source_identity(self):
        for key in ('source_id', 'path', 'source_blob', 'source_range',
                    'source_sha256', 'blob_sha256', 'source_scope'):
            with self.subTest(key=key):
                bad = copy.deepcopy(self.manifest)
                bad['duties'][0][key] = 'wrong'
                with self.assertRaises(AssertionError):
                    M.validate(bad)

    def test_support_cannot_claim_pixel_owner(self):
        self.manifest['duties'][2]['role'] = 'direct-owner'
        with self.assertRaises(AssertionError):
            M.validate(self.manifest)

    def test_missing_native_case(self):
        self.manifest['focused_cases'].pop()
        with self.assertRaises(AssertionError):
            M.validate(self.manifest)

    def test_complete_tap(self):
        result = M.validate_tap(self.tap)
        self.assertEqual(result['focused_count'], 8)

    def test_missing_or_duplicate_tap(self):
        for bad in ('\n'.join(self.tap.splitlines()[:-1]),
                    self.tap + '\n' + self.tap.splitlines()[-1],
                    self.tap.replace(self.tap.splitlines()[-1].split(' ', 2)[-1],
                                     self.tap.splitlines()[-2].split(' ', 2)[-1])):
            with self.assertRaises(AssertionError):
                M.validate_tap(bad)

    def test_incomplete_plan(self):
        with self.assertRaises(AssertionError):
            M.validate_tap(self.tap.replace('1..8', '1..9'))

    def test_failure_skip_and_sanitizer_rejected(self):
        for line in ('not ok 9 /anything', 'Bail out! Error', '# SKIP no display',
                     'ERROR: AddressSanitizer: heap-use-after-free',
                     'AddressSanitizer:DEADLYSIGNAL', 'test.cpp:1: runtime error: null'):
            with self.subTest(line=line), self.assertRaises(AssertionError):
                M.validate_tap(self.tap + '\n' + line)


if __name__ == '__main__':
    unittest.main()
