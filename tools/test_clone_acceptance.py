#!/usr/bin/env python3
"""Negative checks for current-core versus immutable integration evidence."""
from copy import deepcopy
import json
import unittest
from check_clone_acceptance import ROOT, validate


class CloneAcceptance(unittest.TestCase):
    def setUp(self):
        self.matrix = json.loads((ROOT / 'migration/acceptance/clone.json').read_text())

    def test_current(self):
        self.assertEqual(validate(matrix=self.matrix), [])

    def test_missing_and_duplicate_rows(self):
        self.matrix['rows'].pop()
        self.assertTrue(validate(matrix=self.matrix))
        self.matrix['rows'].append(deepcopy(self.matrix['rows'][0]))
        self.assertTrue(validate(matrix=self.matrix))

    def test_invented_core_test(self):
        self.matrix['rows'][0]['named_tests'] = ['/gimp-clone-layer/invented']
        self.assertTrue(validate(matrix=self.matrix))

    def test_unsealed_core_source(self):
        self.matrix['rows'][0]['source'].append('app/core/gimpitem.c')
        self.assertTrue(validate(matrix=self.matrix))

    def test_historical_source_mismatch(self):
        self.matrix['historical_snapshots']['xcf']['test_source_sha256'] = '0' * 64
        self.assertTrue(validate(matrix=self.matrix))

    def test_invented_historical_dialog_test(self):
        self.matrix['rows'][0]['additional_dialog_tests'] = ['/painter-layer-ui/invented']
        self.assertTrue(validate(matrix=self.matrix))

    def test_do_not_hide_known_gaps(self):
        row = next(row for row in self.matrix['rows'] if row['id'] == '14.019')
        row['state'] = 'COMPONENT_VERIFIED'
        self.assertTrue(validate(matrix=self.matrix))

    def test_dependencies_and_acceptance_are_not_weakened(self):
        self.matrix['rows'][0]['dependencies'] = 'none'
        self.assertTrue(validate(matrix=self.matrix))
        self.setUp()
        self.matrix['rows'][0]['acceptance_condition'] = 'representative tests only'
        self.assertTrue(validate(matrix=self.matrix))


if __name__ == '__main__':
    unittest.main()
