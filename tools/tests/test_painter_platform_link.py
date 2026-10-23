#!/usr/bin/env python3
"""Failure controls for the original 07.014 acceptance recorder."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from test_painter_platform_link import (foundation_results, run_command,
                                       archive_objects, pe_exports, macho_exports)


class PlatformRecorder(unittest.TestCase):
    def test_apple_archive_index(self):
        objects = ['error.cpp.o', 'binding.cpp.o', 'store.cpp.o']
        for index in ['__.SYMDEF', '__.SYMDEF SORTED', '__.SYMDEF_64', '__.SYMDEF_64 SORTED']:
            self.assertEqual(archive_objects([index] + objects, 'darwin'), objects)
            with self.assertRaises(RuntimeError):
                archive_objects([index] + objects, 'linux')
        with self.assertRaises(RuntimeError):
            archive_objects(['__.SYMDEF', '__.SYMDEF SORTED'] + objects, 'darwin')
        self.assertEqual(archive_objects(objects + ['stale.o'], 'darwin'), objects + ['stale.o'])

    def test_pe_export_formats(self):
        # Current UCRT64 output captured in native CI run38026874552.
        modern = ('[Ordinal/Name Pointer] Table -- Ordinal Base 1\n'
                  '          Ordinal   Hint Name\n'
                  ' [   0] +base[   1]  0000 painter_platform_export_control\n\n'
                  'Other table\n [ 9] unrelated\n')
        old = '[Ordinal/Name Pointer] Table\n [ 0] painter_platform_export_control\n\n'
        self.assertEqual(pe_exports(modern), ['painter_platform_export_control'])
        self.assertEqual(pe_exports(old), ['painter_platform_export_control'])
        with self.assertRaises(RuntimeError):
            pe_exports('No export table')

    def test_macho_export_flags_and_reexports(self):
        output = ('0x1000 _strong\n0x1004 _weak [weak-def]\n'
                  '0x1008 _thread [per-thread]\n[re-export] _forward (from lib.dylib)\n')
        self.assertEqual(macho_exports(output), ['_strong', '_weak', '_thread', '_forward'])

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
