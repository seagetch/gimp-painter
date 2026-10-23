#!/usr/bin/env python3
"""Negative controls for exact source-duty accounting (no evidence files edited)."""
import contextlib, copy, io, json, pathlib, runpy, sys, unittest
from unittest.mock import patch
HERE = pathlib.Path(__file__).resolve().parent

class RouteAccounting(unittest.TestCase):
    def check(self, mutate=None, draft=True):
        original = json.loads
        def decode(text, *args, **kwargs):
            value = original(text, *args, **kwargs)
            if isinstance(value, dict) and value.get('task') == '07.008/untracked-signals' and 'rows' in value:
                value = copy.deepcopy(value)
                if mutate:
                    mutate(value)
            return value
        argv = ['validate-routes.py'] + (['--allow-draft'] if draft else [])
        with patch.object(sys, 'argv', argv), patch.object(json, 'loads', decode), contextlib.redirect_stdout(io.StringIO()):
            runpy.run_path(str(HERE / 'validate-routes.py'), run_name='__main__')

    def test_exact_accounting(self):
        self.check()

    def test_drop_duty_rejected(self):
        with self.assertRaises(AssertionError):
            self.check(lambda r: r['rows'].pop())

    def test_duplicate_duty_rejected(self):
        with self.assertRaises(AssertionError):
            self.check(lambda r: r['rows'].__setitem__(1, copy.deepcopy(r['rows'][0])))

    def test_weakened_original_criterion_rejected(self):
        with self.assertRaises(AssertionError):
            self.check(lambda r: r['rows'][0]['original_mapping'].__setitem__('remaining_acceptance', 'source inspection only'))

    def test_empty_runtime_coverage_rejected(self):
        with self.assertRaises(AssertionError):
            self.check(lambda r: r['rows'][0].__setitem__('case_identifiers', []))

    def test_invented_native_case_rejected(self):
        with self.assertRaises(AssertionError):
            self.check(lambda r: r['rows'][0]['case_identifiers'].append('invented-nonexistent-native-case-07008'))

    def test_draft_is_not_acceptance(self):
        def draft(r):
            r['status'] = 'IN_PROGRESS_NOT_ACCEPTANCE'
        with self.assertRaises(AssertionError):
            self.check(draft, draft=False)

if __name__ == '__main__':
    unittest.main()
