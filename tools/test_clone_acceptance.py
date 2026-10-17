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
        self.assertIn('WBS14 coverage is incomplete or duplicated', validate(matrix=self.matrix))
        self.matrix['rows'].append(deepcopy(self.matrix['rows'][0]))
        self.assertIn('WBS14 coverage is incomplete or duplicated', validate(matrix=self.matrix))

    def test_invented_core_test(self):
        self.matrix['rows'][0]['named_tests'] = ['/gimp-clone-layer/invented']
        self.assertTrue(validate(matrix=self.matrix))

    def test_unsealed_core_source(self):
        self.matrix['rows'][0]['source'].append('app/core/missing-clone-evidence-source.c')
        self.assertIn('14.001: unsealed claimed core source app/core/missing-clone-evidence-source.c', validate(matrix=self.matrix))

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
        self.assertTrue(any('stale task dependency/acceptance snapshot' in error for error in validate(matrix=self.matrix)))
        self.setUp()
        self.matrix['rows'][0]['acceptance_condition'] = 'representative tests only'
        self.assertTrue(any('stale task dependency/acceptance snapshot' in error for error in validate(matrix=self.matrix)))

    def test_absent_current_receipt_fails_closed(self):
        self.matrix['current_core_receipt'] = {'directory': 'missing-current-evidence', 'receipt_sha256': '0' * 64}
        self.assertTrue(any('Invalid current Clone receipt' in error for error in validate(matrix=self.matrix)))

    def test_incomplete_receipt_route_fails_closed(self):
        self.matrix['current_core_receipt'] = {'directory': 'migration/tests'}
        self.assertIn('Invalid current core receipt route', validate(matrix=self.matrix))

    def test_component_child_keeps_predecessors_and_broad_parent_open(self):
        row = next(row for row in self.matrix['rows'] if row['id'] == '14.007/component-visibility-regression')
        self.assertEqual(row['dependencies'], '14.001/core-implementation, 02.005, 09.002')
        self.assertFalse(next(row for row in self.matrix['rows'] if row['id'] == '14.007')['existing_checkbox'])


if __name__ == '__main__':
    unittest.main()
