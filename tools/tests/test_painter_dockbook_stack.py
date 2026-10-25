#!/usr/bin/env python3
"""Display-independent controls for the pinned dockbook stack diagnostic."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1]
sys.path.insert(0, str(TOOLS))
import check_painter_dockbook_diagnostic as dock


class DockbookStackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
    def tearDown(self):
        self.temp.cleanup()
    def report(self, **changes):
        row = dict(name='gimp:app / painter-common-layer-popup', result='FAIL', returncode=255,
                   command=['/private/gdb-dockbook-wrapper.sh', 'binary', '--run-prefix', dock.SELECTED_PREFIX],
                   env={'TOKEN': 'must-not-export'},
                   stdout='Thread 1 "painter-layer-ui" received signal SIGTRAP, Trace/breakpoint trap.\n#0 frame ()\n',
                   stderr='Gtk-WARNING **: ' + dock.WARNING)
        row.update(changes)
        path = self.root / 'log.json'
        path.write_text(json.dumps(row))
        return dock.read_diagnostic(path, dock.SELECTED_PREFIX)
    def test_matching_warning_signal_frames_not_acceptance(self):
        result = self.report()
        self.assertTrue(result['stack_captured'])
        self.assertTrue(result['diagnostic_only'])
        self.assertFalse(result['acceptance_eligible'])
        self.assertNotIn('passed', result)
        self.assertNotIn('must-not-export', json.dumps(result))
        self.assertNotIn('command', result['tests'][0])
    def test_wrong_signal_no_frames_or_no_warning_rejected(self):
        for changes in ({'stdout': 'Program received signal SIGSEGV\n#0 frame ()\n'},
                        {'stdout': 'Program received signal SIGTRAP\n'},
                        {'stderr': 'Gtk-WARNING **: another warning'},
                        {'stderr': 'plain text ' + dock.WARNING},
                        {'stderr': 'Gtk-WARNING **: ' + dock.WARNING.replace('GimpDockbook', 'GtkNotebook')}):
            with self.subTest(changes=changes):
                self.assertFalse(self.report(**changes)['stack_captured'])
    def test_timeout_and_bad_status_cannot_be_success(self):
        for changes in ({'result': 'TIMEOUT'}, {'result': 'OK'}, {'returncode': None},
                        {'returncode': 0}, {'returncode': '255'}, {'returncode': True}):
            with self.subTest(changes=changes):
                self.assertFalse(self.report(**changes)['stack_captured'])
        self.assertEqual(self.report(result='TIMEOUT')['diagnostic_status'], 'incomplete_timeout')
    def test_wrong_wrapper_selector_registration_rejected(self):
        for changes in ({'command': ['binary', '--run-prefix', dock.SELECTED_PREFIX]},
                        {'command': ['/private/gdb-dockbook-wrapper.sh', 'binary', '-p', dock.SELECTED_PREFIX]},
                        {'command': ['/private/gdb-dockbook-wrapper.sh', 'binary', '--run-prefix', '/different']},
                        {'name': 'gimp:app / unrelated'}):
            with self.subTest(changes=changes):
                self.assertFalse(self.report(**changes)['stack_captured'])
    def test_debugger_denial_is_incomplete_not_bypassed(self):
        result = self.report(stdout='ptrace: Operation not permitted.\n', stderr='')
        self.assertEqual(result['diagnostic_status'], 'incomplete_debugger_or_runtime')
        self.assertFalse(result['stack_captured'])
    def test_empty_duplicate_and_wrong_prefix_rejected(self):
        path = self.root / 'log.json'
        for text in ('', '{}\n{}\n', '[]'):
            path.write_text(text)
            self.assertFalse(dock.read_diagnostic(path, dock.SELECTED_PREFIX)['stack_captured'])
        self.assertFalse(dock.read_diagnostic(path, '/different')['stack_captured'])
    def test_streams_bounded_and_known_environment_lines_removed(self):
        result = self.report(stdout='x' * (1024 * 1024 + 1) + '\n>>> TOKEN=not-public\nInherited environment: not-public\n')
        self.assertTrue(result['tests'][0]['stdout_truncated'])
        self.assertLessEqual(len(result['tests'][0]['stdout']), 1024 * 1024 + 1)
        self.assertNotIn('not-public', result['tests'][0]['stdout'])
    def test_driver_fresh_profile_trace_no_acceptance(self):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        definitions = driver.split('test_group() {', 1)[1].split('\nif [[ "$mode" == pre-fix-stack-978 ]]', 1)[0]
        calls = self.root / 'calls'
        exports = self.root / 'exports'
        script = '''set -euo pipefail
run="$1/private"; out="$1/artifacts"; build="$1/build"; repo="$1/repo"
mkdir -p "$run" "$out"
export GIMP_TEST_COMMON_POPUP_SCREENSHOTS=stale GIMP_TEST_COMMON_POPUP_DOCK_TRACE=stale
xvfb-run() { printf '%s|%s|%s|%s|%s\\n' "$HOME" "$GDK_SCALE" "${GIMP_TEST_COMMON_POPUP_SCREENSHOTS:-UNSET}" "${GIMP_TEST_COMMON_POPUP_DOCK_TRACE:-UNSET}" "$*" >> CALLS; return 1; }
python3() { printf '%s\\n' "$*" >> EXPORTS; return 0; }
'''.replace('CALLS', '"' + str(calls) + '"').replace('EXPORTS', '"' + str(exports) + '"')
        command = script + 'test_group() {' + definitions + '\ndockbook_prefix=' + dock.SELECTED_PREFIX + '\ntest_group dockbook-clone-tree-scale1 1 no painter-common-layer-popup\n'
        proc = subprocess.run(['bash', '-c', command, 'control', str(self.root)], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        row = calls.read_text().strip().split('|')
        self.assertIn('/profile-dockbook-clone-tree-scale1/home', row[0])
        self.assertEqual(row[1:3], ['1', 'UNSET'])
        self.assertEqual(row[3], str(self.root / 'artifacts/dockbook-lifecycle-trace.txt'))
        self.assertIn('--timeout-multiplier 0.5', row[4])
        self.assertIn('--wrapper ' + str(self.root / 'private/gdb-dockbook-wrapper.sh'), row[4])
        self.assertIn('--run-prefix ' + dock.SELECTED_PREFIX, row[4])
        self.assertIn('dockbook-clone-tree-scale1-gdb-dockbook-wrapper.sh.json', exports.read_text())
        self.assertIn('check_painter_dockbook_diagnostic.py', exports.read_text())
        self.assertNotIn('check_painter_ui_ci.py', exports.read_text())
        # A subsequent ordinary invocation explicitly discards inherited tracing.
        calls.unlink()
        command += '\nunset dockbook_prefix\ntest_group normal 1 no painter-common-layer-popup || true\n'
        proc = subprocess.run(['bash', '-c', command, 'control', str(self.root)], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(calls.read_text().splitlines()[-1].split('|')[3], 'UNSET')
    def test_pinned_mode_preserves_ordinary_targets_and_old_signal_predicate(self):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        self.assertIn('dockbook_commit=' + dock.SOURCE_COMMIT, driver)
        self.assertIn('dockbook_tree=' + dock.SOURCE_TREE, driver)
        self.assertIn('test_group dockbook-clone-tree-scale1 1 no painter-common-layer-popup', driver)
        block = driver.split('stage=dockbook-stack-diagnostic', 1)[1].split('stage=baseline-carrier-red', 1)[0]
        self.assertNotIn('acceptance.txt', block)
        self.assertNotIn('prepare-painter-popup-baseline.py', block)
        self.assertIn("-ex 'set disable-randomization off'", block)
        self.assertIn("-iex 'set auto-load off'", block)
        self.assertIn('timeout --kill-after=1s 5s', block)
        self.assertIn('ulimit -c 0', block)
        self.assertIn("-ex 'thread apply all bt -no-filters 40'", block)
        old = (TOOLS / 'check_painter_stack_diagnostic.py').read_text()
        self.assertIn('received signal SIGSEGV', old)
        self.assertNotIn('SIGTRAP', old)
    def test_deeper_main_trace_is_bounded_and_only_in_dockbook_recipe(self):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        dock_block = driver.split('stage=dockbook-stack-diagnostic', 1)[1].split('stage=baseline-carrier-red', 1)[0]
        old_block = driver.split('stage=historical-stack-diagnostic', 1)[1].split('stage=dockbook-stack-diagnostic', 1)[0]
        marker = '=== DOCKBOOK MAIN THREAD BACKTRACE (MAX 200 FRAMES) ==='
        self.assertEqual(driver.count(marker), 1)
        self.assertEqual(driver.count("-ex 'bt -no-filters 200'"), 1)
        self.assertLess(dock_block.index("-ex 'thread apply all bt -no-filters 40'"), dock_block.index(marker))
        self.assertLess(dock_block.index(marker), dock_block.index("-ex 'thread 1'"))
        self.assertLess(dock_block.index("-ex 'thread 1'"), dock_block.index("-ex 'bt -no-filters 200'"))
        self.assertNotIn(marker, old_block)
        self.assertNotIn("-ex 'thread 1'", old_block)
        self.assertNotIn('bt -no-filters 200', old_block)
        self.assertIn("-ex 'set print frame-arguments none'", dock_block)
        self.assertIn("-ex 'set print entry-values no'", dock_block)
        self.assertNotIn('bt full', dock_block)
        self.assertNotIn('info locals', dock_block)
        self.assertNotIn('show environment', dock_block)
    def test_workflow_keeps_required_gates_and_readonly_permissions(self):
        text = (TOOLS.parent / '.github/workflows/painter-native-ui.yml').read_text()
        self.assertIn('target: [candidate, baseline-carrier-red, dockbook-controlled-order-3d6f]', text)
        self.assertIn('contents: read', text)
        self.assertIn('fail-fast: false', text)
        self.assertNotIn('continue-on-error:', text)
        self.assertIn('test_painter_dockbook_diagnostic.py', text)
        self.assertIn('test_painter_dockbook_stack.py', text)


if __name__ == '__main__':
    unittest.main()
