#!/usr/bin/env python3
"""Pure-Python controls for the private Filter runtime diagnostic gate.

Run with python3 -B migration/tests/test_filter_acceptance_diagnostics.py.
No native runtime, build lock, installed bundle, or display is needed.
"""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location(
    'filter_acceptance_runtime', Path(__file__).with_name('run_filter_acceptance_runtime.py'))
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class DiagnosticGate(unittest.TestCase):
    def setUp(self):
        self.known = ''.join((line+'\n')*count
                             for line, count in runtime.PROCEDURE_DIAGNOSTICS.items())

    def test_exact_negative_counts(self):
        self.assertEqual(runtime.diagnostic_errors('', self.known,
                                                   runtime.PROCEDURE_DIAGNOSTICS), [])
        self.assertTrue(runtime.diagnostic_errors('', self.known.split('\n', 1)[1],
                                                  runtime.PROCEDURE_DIAGNOSTICS))
        self.assertTrue(runtime.diagnostic_errors('', self.known+self.known.splitlines()[0]+'\n',
                                                  runtime.PROCEDURE_DIAGNOSTICS))

    def test_unexpected_warnings_on_both_streams(self):
        warnings = (
            '(worker:1): GEGL-WARNING **: tile-backend-swap writer queue was not empty',
            '(worker:1): GLib-GObject-CRITICAL **: invalid pointer',
            '** ERROR **: Private Filter GEGL swap drain exceeded the 5 second deadline',
            '(blinds:1): LibGimp-WARNING **: gimp_flush(): error: Broken pipe',
        )
        for warning in warnings:
            for stream in ('stdout', 'stderr'):
                with self.subTest(warning=warning, stream=stream):
                    stdout = warning if stream == 'stdout' else ''
                    stderr = self.known+(warning+'\n' if stream == 'stderr' else '')
                    self.assertTrue(runtime.diagnostic_errors(stdout, stderr,
                                                              runtime.PROCEDURE_DIAGNOSTICS))

    def test_empty_and_unexpected_plain_stderr(self):
        self.assertEqual(runtime.diagnostic_errors('corpus completed', '', {}), [])
        self.assertTrue(runtime.diagnostic_errors('', 'Unexpected child error', {}))

    def test_nested_logs_preserve_existing_startup_policy(self):
        with tempfile.TemporaryDirectory(prefix='filter-diagnostics-') as folder:
            root = Path(folder)
            relocated = root/'relocated'
            relocated.mkdir()
            (relocated/'irrelevant.log').write_text('GEGL-WARNING **: packaged text\n')
            (root/'console.log').write_text(
                '(script-fu:1): scriptfu-WARNING **: existing startup resource warning\n'
                'GEGL-WARNING **: unexpected swap warning\n')
            self.assertEqual(runtime.gegl_log_diagnostics(root, relocated), [
                dict(log='console.log', line=2, text='GEGL-WARNING **: unexpected swap warning')])


if __name__ == '__main__':
    unittest.main()
