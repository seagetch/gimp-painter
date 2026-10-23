#!/usr/bin/env python3
"""Failure controls for the original 07.014 acceptance recorder."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from test_painter_platform_link import foundation_results, run_command


class PlatformRecorder(unittest.TestCase):
    def test_tap_acceptance_and_rejections(self):
        valid = '1..69\n' + ''.join(f'ok {i} /painter/example-{i}\n' for i in range(1, 70))
        self.assertEqual(len(foundation_results(valid)), 69)
        bad = [valid.replace('ok 69 /painter/example-69', replacement) for replacement in
               ['ok 69 /painter/example-69 # SKIP unavailable',
                'ok 69 /painter/example-69 # TODO not implemented',
                'not ok 69 /painter/example-69', 'ok 69 /painter/example-1',
                'ok 68 /painter/example-69', '']]
        bad += [valid.replace('1..69', '1..70'), valid + '1..69\n']
        for output in bad:
            with self.subTest(output=output[-100:]), self.assertRaises(RuntimeError):
                foundation_results(output)

    def test_timeout_keeps_command_and_partial_output(self):
        report = {'commands': []}
        command = [sys.executable, '-c', 'import time; print("started", flush=True); time.sleep(10)']
        with self.assertRaisesRegex(RuntimeError, 'timed out'):
            run_command(report, command, timeout=1)
        self.assertEqual(report['commands'][0]['argv'], command)
        self.assertEqual(report['commands'][0]['output'].strip(), 'started')
        self.assertEqual(report['commands'][0]['error'], 'TimeoutExpired')
        self.assertIsNone(report['commands'][0]['exit_code'])

    def test_launch_failure_is_recorded(self):
        report = {'commands': []}
        with tempfile.TemporaryDirectory() as directory:
            command = [str(Path(directory) / 'missing-executable')]
            with self.assertRaisesRegex(RuntimeError, 'could not start'):
                run_command(report, command)
        self.assertEqual(report['commands'][0]['argv'], command)
        self.assertTrue(report['commands'][0]['error'])

    def test_nonzero_is_recorded(self):
        report = {'commands': []}
        with self.assertRaisesRegex(RuntimeError, 'Command failed'):
            run_command(report, [sys.executable, '-c', 'print("failure"); raise SystemExit(7)'])
        self.assertEqual(report['commands'][0]['exit_code'], 7)
        self.assertEqual(report['commands'][0]['output'].strip(), 'failure')


if __name__ == '__main__':
    unittest.main()
