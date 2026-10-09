#!/usr/bin/env python3
"""Keep explicit incompatible casts out of accepted native slot expressions."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check_painter_vfunc_types import expression_kind


class VfuncExpressions(unittest.TestCase):
    def test_native_function_and_null(self):
        for expression in ('callback', '&callback', '& callback'):
            self.assertEqual(expression_kind(expression), 'function')
        for expression in ('nullptr', 'NULL', '0'):
            self.assertEqual(expression_kind(expression), 'null')

    def test_complete_captureless_lambda(self):
        for expression in ('[](GObject *) {}', '+ [] (GObject *owner) { if (owner) { return; } }',
                           '[](GObject *) { const char *s = "}"; (void)s; } /* end */'):
            self.assertEqual(expression_kind(expression), 'captureless-lambda')

    def test_explicit_cast_and_signal_transport_rejected(self):
        for expression in ('reinterpret_cast<void (*)(GObject *)>(wrong)',
                           'static_cast<NativeSlot>(wrong)', '(NativeSlot) wrong',
                           '(void (*)(GObject *)) wrong', 'G_CALLBACK(wrong)'):
            with self.assertRaises(ValueError):
                expression_kind(expression)

    def test_immediate_lambda_invocation_rejected(self):
        expression = '[]() { return reinterpret_cast<void (*)(GObject *)>(wrong_owner); }()'
        with self.assertRaises(ValueError):
            expression_kind(expression)

    def test_lambda_postfix_and_selection_rejected(self):
        for suffix in ('()', ', other', ' ? other : callback', '[0]'):
            with self.assertRaises(ValueError):
                expression_kind('[](GObject *) {}' + suffix)

    def test_capture_and_unbalanced_body_rejected(self):
        for expression in ('[owner](GObject *) {}', '[&](GObject *) {}', '[](GObject *) {'):
            with self.assertRaises(ValueError):
                expression_kind(expression)


if __name__ == '__main__':
    unittest.main(verbosity=2)
