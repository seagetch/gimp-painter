#!/usr/bin/env python3
"""Pure-Python runner regression checks; no compiler/linker/display/GIMP runs.

Run directly from any checkout: python3 migration/tests/test_sharpen_context_sanitizers.py
Optionally pass --receipt PATH.json to retain a source-sealed result receipt.
"""
import argparse
import ast
from contextlib import redirect_stderr, redirect_stdout
import fcntl
import hashlib
import importlib.util
import io
import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

sys.dont_write_bytecode = True
parser = argparse.ArgumentParser()
parser.add_argument('--runner', type=pathlib.Path, default=pathlib.Path(__file__).with_name('run_sharpen_context_sanitizers.py'))
parser.add_argument('--receipt', type=pathlib.Path, help='Optional explicit JSON receipt destination')
args = parser.parse_args()
sys.path.insert(0, str(args.runner.resolve().parent))
spec = importlib.util.spec_from_file_location('sharpen_sanitizer_runner', args.runner)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
NATIVE = '/gimp-filter-layer/sharpen_late_context_routes'
XCF = '/painter-xcf-open/sharpen_old_writer_roundtrip'


class Checks(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = pathlib.Path(temporary.name)

    def argv(self, report=None):
        return [str(self.root), '--report', str(report or self.root / 'failure.json'),
                '--lock-fd', '199', '--native-test-path', NATIVE, '--xcf-test-path', XCF]

    def make_run(self):
        return m.Run(argparse.Namespace(build=self.root, report=self.root / 'report.json',
                     total_timeout=60, command_timeout=5, test_timeout=5,
                     native_test_path=[NATIVE], xcf_test_path=[XCF]))

    def entry(self, output='app/painter/libapppainter.a.p/filter-context.cpp.o'):
        return {'directory': str(self.root), 'file': str(m.ROOT / 'app/painter/filter-context.cpp'),
                'output': output, 'arguments': ['c++', '-fno-rtti', '-MMD', '-MF', 'old.d',
                '-MQold.o', '-c', str(m.ROOT / 'app/painter/filter-context.cpp'), '-o', output]}

    def test_syntax(self):
        ast.parse(args.runner.read_text())

    def test_options_preserve_quoted_and_unrelated_settings(self):
        previous = 'handle_segv=0:log_path="/tmp/a:detect_leaks=1:log":detect_leaks=1'
        self.assertEqual(m.merge_options(previous, {'detect_leaks': '0', 'halt_on_error': '1'}),
                         previous + ':detect_leaks=0:halt_on_error=1')

    def test_exact_positive_tap(self):
        self.assertTrue(m.tap_result('TAP version 14\n1..1\nok 1 ' + NATIVE + '\n', NATIVE)['exact_positive_result'])

    def test_tap_empty_skip_prefix_failure_duplicate_and_bailout_rejected(self):
        for value in ('', '1..0\n', '1..1\nok 1 '+NATIVE+' # SKIP why\n',
                      '1..1\nok 1 '+NATIVE+'/child\n', '1..2\nok 1 '+NATIVE+'\nok 2 '+NATIVE+'\n',
                      '1..1\nnot ok 1 '+NATIVE+'\n', '1..1\nok 1 '+NATIVE+'\nBail out!\n'):
            with self.subTest(value=value):
                self.assertFalse(m.tap_result(value, NATIVE)['exact_positive_result'])

    def test_exclusive_inherited_lock_required(self):
        path = self.root / 'lock'
        path.touch()
        with path.open('r+') as held:
            with self.assertRaises(RuntimeError):
                m.verify_lock(held.fileno(), path)
            fcntl.flock(held, fcntl.LOCK_SH)
            with self.assertRaises(RuntimeError):
                m.verify_lock(held.fileno(), path)
            fcntl.flock(held, fcntl.LOCK_EX)
            self.assertTrue(m.verify_lock(held.fileno(), path)['exclusive_caller_lock_verified'])
            with path.open('r+') as wrong:
                with self.assertRaises(BlockingIOError):
                    m.verify_lock(wrong.fileno(), path)
            fcntl.flock(held, fcntl.LOCK_UN)

    def test_failed_preflight_retains_per_test_not_run_report_and_can_repeat(self):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            self.assertEqual(m.main(self.argv()), 1)
            self.assertEqual(m.main(self.argv()), 1)
        report = json.loads((self.root / 'failure.json').read_text())
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(len(report['tests']), 2)
        self.assertTrue(all(t['process_status'] == 'not_run' and not t['passed'] for t in report['tests']))

    def test_unrelated_existing_report_never_overwritten(self):
        path = self.root / 'compile_commands.json'
        path.write_text('[]')
        with redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            m.main(self.argv(path))
        self.assertEqual(raised.exception.code, 2)
        self.assertEqual(path.read_text(), '[]')

    def test_canonical_production_entry_and_conflict_detection(self):
        proper = self.entry()
        standalone = self.entry('app/painter/filter-context-test.p/filter-context.cpp.o')
        self.assertEqual(m.canonical_entry([standalone, proper, proper], 'app/painter/filter-context.cpp', self.root, {str(m.output_for(proper))}), proper)
        conflict = {**proper, 'arguments': proper['arguments'] + ['-DOTHER']}
        with self.assertRaises(RuntimeError):
            m.canonical_entry([proper, conflict], 'app/painter/filter-context.cpp', self.root, {str(m.output_for(proper))})

    def test_nested_painter_production_library_requires_actual_link_membership(self):
        source = 'app/paint/gimppainterpaper.cpp'
        output = 'app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a.p/.._gimppainterpaper.cpp.o'
        production = self.entry(output)
        production['file'] = str(m.ROOT / source)
        production['arguments'][-3] = str(m.ROOT / source)
        unrelated = self.entry('app/paint/unrelated/libunrelated.a.p/gimppainterpaper.cpp.o')
        unrelated['file'] = str(m.ROOT / source)
        actual = {str(m.output_for(production))}
        self.assertEqual(m.canonical_entry([unrelated, production], source, self.root, actual), production)
        with self.assertRaises(RuntimeError):
            m.canonical_entry([production], source, self.root, set())

    def test_linked_test_library_still_is_not_production(self):
        candidate = self.entry('app/tests/libtest-only.a.p/filter-context.cpp.o')
        with self.assertRaises(RuntimeError):
            m.canonical_entry([candidate], 'app/painter/filter-context.cpp', self.root, {str(m.output_for(candidate))})

    def test_private_compile_uses_private_objects_and_depfiles(self):
        for instrumented in (True, False):
            command = m.private_compile(self.entry(), self.root/'private.o', self.root/'private.d', instrumented)
            self.assertEqual(command[command.index('-o')+1], str(self.root/'private.o'))
            self.assertEqual(command[command.index('-MF')+1], str(self.root/'private.d'))
            self.assertNotIn('old.d', command)
            self.assertNotIn('-MQold.o', command)
            self.assertEqual(command[-1], '-frtti')
            self.assertEqual('-fsanitize=address,undefined' in command, instrumented)

    def test_mocked_failed_compile_keeps_diagnostics(self):
        run = self.make_run()
        process = Mock(returncode=2)
        process.communicate.return_value = ('compiler stdout', 'compiler diagnostic')
        with patch.object(m.subprocess, 'Popen', return_value=process), self.assertRaises(RuntimeError):
            run.command(['MOCK-COMPILER'], 'mock compile', self.root)
        saved = json.loads(run.args.report.read_text())
        self.assertEqual(saved['commands'][0]['exit_code'], 2)
        self.assertEqual(saved['commands'][0]['stderr'], 'compiler diagnostic')
        self.assertEqual(saved['commands'][0]['status'], 'failed')

    def test_mocked_timeout_kills_only_owned_process_group_and_retains_output(self):
        run = self.make_run()
        process = Mock(returncode=-9, pid=12345)
        process.communicate.side_effect = [subprocess.TimeoutExpired('MOCK-LINKER', 5), ('partial stdout', 'partial stderr')]
        with patch.object(m.subprocess, 'Popen', return_value=process), patch.object(m.os, 'killpg') as kill:
            result = run.command(['MOCK-LINKER'], 'mock link', self.root, check=False)
        kill.assert_called_once_with(12345, m.signal.SIGKILL)
        self.assertEqual(result['status'], 'timeout')
        self.assertEqual(result['stderr'], 'partial stderr')
        self.assertEqual(json.loads(run.args.report.read_text())['commands'][0]['status'], 'timeout')

    def test_source_drift_fails_final_seal(self):
        run = self.make_run()
        source = self.root/'changed.hpp'
        source.write_text('before')
        run.inputs = m.seals([source])
        source.write_text('after')
        run.report['status'] = 'passed'
        run.finish()
        self.assertEqual(run.report['status'], 'failed')
        self.assertEqual(run.report['source_seals']['changed'], [str(source)])


suite = unittest.defaultTestLoader.loadTestsFromTestCase(Checks)
result = unittest.TextTestRunner(verbosity=2).run(suite)
receipt = {
    'status': 'passed' if result.wasSuccessful() else 'failed',
    'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
    'runner': str(args.runner.resolve()), 'runner_sha256': hashlib.sha256(args.runner.read_bytes()).hexdigest(),
    'check_script': str(pathlib.Path(__file__).resolve()),
    'check_script_sha256': hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),
    'scope': 'Pure-Python syntax/helper/report checks with mocked subprocesses; no compiler, linker, display or GIMP/native harness execution',
    'instrumented_translation_units': len(m.INSTRUMENTED),
}
if args.receipt is not None:
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2)+'\n')
raise SystemExit(not result.wasSuccessful())
