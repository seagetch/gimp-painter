#!/usr/bin/env python3
"""Display-independent controls for active dispatch attribution/isolation."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1]
sys.path.insert(0, str(TOOLS))
import check_painter_controlled_order_diagnostic as controlled
import check_painter_dockbook_diagnostic as passive

PHASE = 'phase=8 fixture/controlled-offscreen-updates test=' + passive.SELECTED_PREFIX + '/actual-entry\n'
BEGIN = controlled.BEGIN + ' book=0x123:GimpDockbook dockable=0x234:GimpDockable offscreen=0x345 type=5 clock=0x456 widget-clock=0x567\n'
RETURNED = controlled.RETURNED + ' window=0x345 destroyed=0\n'


class ControlledOrderTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.log = self.root / 'log.json'
        self.trace = self.root / 'trace.txt'
    def tearDown(self):
        self.temp.cleanup()
    def report(self, trace=PHASE + BEGIN, **changes):
        row = dict(name='gimp:app / painter-common-layer-popup', result='FAIL', returncode=255,
                   command=['/private/gdb-dockbook-wrapper.sh', 'binary', '--run-prefix', passive.SELECTED_PREFIX],
                   env={'TOKEN': 'not-public'},
                   stdout='Thread 1 "painter-layer-ui" received signal SIGTRAP, Trace/breakpoint trap.\n#0 fatal ()\n' + controlled.MAIN_TRACE + '\n#68 0x123 in (anonymous namespace)::controlled_dock_updates () at ../source/app/tests/test-common-layer-popup.cpp:291\n',
                   stderr='Gtk-WARNING **: ' + passive.WARNING)
        row.update(changes)
        self.log.write_text(json.dumps(row))
        self.trace.write_text(trace)
        return controlled.read_controlled(self.log, passive.SELECTED_PREFIX, self.trace)
    def test_only_unreturned_warning_dispatch_stack_is_reproduction(self):
        result = self.report()
        self.assertTrue(result['controlled_order_reproduced'])
        self.assertTrue(result['stack_captured'])
        self.assertTrue(result['warning_stack_observed'])
        self.assertFalse(result['acceptance_eligible'])
        self.assertTrue(result['diagnostic_only'])
        self.assertEqual(result['source_identity_record'], 'dockbook-controlled-order-overlay.json')
        self.assertNotIn('passed', result)
        self.assertNotIn('not-public', json.dumps(result))
    def test_prior_returns_then_one_pending_is_valid(self):
        result = self.report(trace=PHASE + BEGIN + RETURNED + PHASE + BEGIN)
        self.assertTrue(result['controlled_order_reproduced'])
        self.assertEqual(result['controlled_trace']['begin_count'], 2)
        self.assertEqual(result['controlled_trace']['returned_count'], 1)
    def test_natural_warning_after_return_is_not_controlled(self):
        result = self.report(trace=PHASE + BEGIN + RETURNED)
        self.assertFalse(result['controlled_order_reproduced'])
        self.assertTrue(result['warning_stack_observed'])
        self.assertEqual(result['diagnostic_status'], 'natural_warning_outside_controlled_dispatch')
    def test_no_warning_normal_return_inconclusive_not_resilience(self):
        result = self.report(trace=PHASE + BEGIN + RETURNED, stdout='Inferior exited normally.\n', stderr='', result='OK', returncode=0)
        self.assertFalse(result['controlled_order_reproduced'])
        self.assertEqual(result['diagnostic_status'], 'no_controlled_warning_reproduced')
        self.assertIn('no update was pending', result['qualification'])
    def test_no_pending_marker_never_success(self):
        self.assertFalse(self.report(trace=PHASE)['controlled_order_reproduced'])
    def test_missing_dispatch_frame_incomplete(self):
        result = self.report(stdout='Program received signal SIGTRAP\n#0 other_function ()\n')
        self.assertEqual(result['diagnostic_status'], 'incomplete_controlled_dispatch_stack')
        self.assertFalse(result['controlled_order_reproduced'])
    def test_dispatch_name_outside_frame_is_not_evidence(self):
        result = self.report(stdout='Program received signal SIGTRAP\n#0 other_function ()\ngdk_window_process_updates ()\n')
        self.assertFalse(result['controlled_order_reproduced'])
    def test_dispatch_frame_only_in_another_thread_is_incomplete(self):
        result = self.report(stdout='Program received signal SIGTRAP\n#0 controlled_dock_updates () at ../source/app/tests/test-common-layer-popup.cpp:291\n' + controlled.MAIN_TRACE + '\n#0 unrelated ()\n')
        self.assertFalse(result['controlled_order_reproduced'])
    def test_helper_wrong_line_file_or_function_rejected(self):
        for frame in ('#68 controlled_dock_updates () at ../source/app/tests/test-common-layer-popup.cpp:295',
                      '#68 controlled_dock_updates () at ../other.cpp:291',
                      '#68 another_helper () at ../source/app/tests/test-common-layer-popup.cpp:291',
                      '#68 controlled_dock_updates ()'):
            result = self.report(stdout='Program received signal SIGTRAP\n' + controlled.MAIN_TRACE + '\n' + frame + '\n')
            self.assertFalse(result['controlled_order_reproduced'])
    def test_public_gdk_frame_alone_is_not_callsite_attribution(self):
        result = self.report(stdout='Program received signal SIGTRAP\n' + controlled.MAIN_TRACE + '\n#68 gdk_window_process_updates ()\n')
        self.assertFalse(result['controlled_order_reproduced'])
        self.assertTrue(result['public_gdk_frame_observed'])
    def test_wrapped_exact_helper_callsite_is_accepted(self):
        result = self.report(stdout='Program received signal SIGTRAP\n' + controlled.MAIN_TRACE + '\n#68 (anonymous namespace)::controlled_dock_updates ()\n    at ../source/app/tests/test-common-layer-popup.cpp:291\n')
        self.assertTrue(result['controlled_order_reproduced'])
    def test_same_phase_observation_inside_dispatch_is_valid(self):
        observation = PHASE.rstrip() + ' event=size-allocate allocations=3 book=0x123:GimpDockbook\n'
        result = self.report(trace=PHASE + observation + BEGIN + observation)
        self.assertTrue(result['controlled_order_reproduced'])
    def test_changed_number_or_test_inside_dispatch_is_rejected(self):
        for phase in (PHASE.replace('phase=8', 'phase=9'), PHASE.replace('actual-entry', 'lower-controls')):
            result = self.report(trace=PHASE + BEGIN + phase.rstrip() + ' event=draw\n')
            self.assertFalse(result['controlled_order_reproduced'])
    def test_fixture_advanced_after_unreturned_marker_rejected(self):
        result = self.report(trace=PHASE + BEGIN + PHASE.replace('controlled-offscreen-updates', 'native-dock-settle'))
        self.assertFalse(result['controlled_order_reproduced'])
    def test_wrong_signal_warning_timeout_or_zero_status(self):
        for changes in ({'stdout': 'Program received signal SIGSEGV\n#0 gdk_window_process_updates ()\n'},
                        {'stderr': 'Gtk-WARNING **: unrelated'}, {'result': 'TIMEOUT'},
                        {'returncode': 0}, {'returncode': True}):
            with self.subTest(changes=changes):
                self.assertFalse(self.report(**changes)['controlled_order_reproduced'])
    def test_cap_anywhere_rejects_absence_based_attribution(self):
        for trace in ('TRACE LIMIT REACHED\n' + PHASE + BEGIN, PHASE + BEGIN + 'TRACE LIMIT REACHED\n'):
            self.assertEqual(self.report(trace=trace)['diagnostic_status'], 'incomplete_controlled_trace')
    def test_oversize_trace_is_bounded(self):
        result = self.report(trace='x' * (controlled.MAX_TRACE + 1))
        self.assertFalse(result['controlled_order_reproduced'])
        self.assertIn('2MiB', result['errors'][0])
    def test_bad_marker_order_rejected(self):
        for trace in (PHASE + BEGIN + BEGIN, PHASE + RETURNED, PHASE + BEGIN + RETURNED + RETURNED,
                      PHASE + BEGIN + RETURNED.replace('0x345', '0x999'), PHASE + controlled.MARKER + 'UNKNOWN\n'):
            with self.subTest(trace=trace):
                self.assertFalse(self.report(trace=trace)['controlled_order_reproduced'])
    def test_missing_or_wrong_phase_and_target_rejected(self):
        for trace in (BEGIN, PHASE.replace('clone-tree', 'ordinary-tree') + BEGIN,
                      PHASE.replace('controlled-offscreen-updates', 'native-dock-settle') + BEGIN,
                      PHASE + BEGIN.replace('type=5', 'type=1'),
                      PHASE + BEGIN.replace('GimpDockbook', 'GtkNotebook')):
            with self.subTest(trace=trace):
                self.assertFalse(self.report(trace=trace)['controlled_order_reproduced'])
    def test_missing_or_invalid_utf8_trace_rejected(self):
        self.report()
        self.trace.unlink()
        self.assertFalse(controlled.read_controlled(self.log, passive.SELECTED_PREFIX, self.trace)['controlled_order_reproduced'])
        self.trace.write_bytes(b'\xff')
        self.assertFalse(controlled.read_controlled(self.log, passive.SELECTED_PREFIX, self.trace)['controlled_order_reproduced'])
    def test_wrong_log_prefix_wrapper_registration_rejected(self):
        for changes in ({'name': 'gimp:app / another'}, {'command': ['binary', '--run-prefix', passive.SELECTED_PREFIX]},
                        {'command': ['/private/gdb-dockbook-wrapper.sh', 'binary', '--run-prefix', '/other']}):
            self.assertFalse(self.report(**changes)['controlled_order_reproduced'])
    def test_active_mode_isolated_fresh_profile_and_checker(self):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        definitions = driver.split('test_group() {', 1)[1].split('\nif [[ "$mode" == pre-fix-stack-978 ]]', 1)[0]
        calls = self.root / 'calls'; exports = self.root / 'exports'
        script = '''set -euo pipefail
run="$1/private"; out="$1/artifacts"; build="$1/build"; repo="$1/repo"
mkdir -p "$run" "$out"
export GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES=stale GIMP_TEST_COMMON_POPUP_DOCK_TRACE=stale
xvfb-run() { printf '%s|%s|%s|%s|%s\\n' "$HOME" "$GDK_SCALE" "${GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES:-UNSET}" "${GIMP_TEST_COMMON_POPUP_DOCK_TRACE:-UNSET}" "$*" >> CALLS; return 1; }
python3() { printf '%s\\n' "$*" >> EXPORTS; return 0; }
'''.replace('CALLS', '"' + str(calls) + '"').replace('EXPORTS', '"' + str(exports) + '"')
        command = script + 'test_group() {' + definitions + '\ndockbook_prefix=' + passive.SELECTED_PREFIX + '''
dockbook_controlled_order=1
test_group dockbook-controlled-order-clone-tree-scale1 1 no painter-common-layer-popup
unset dockbook_controlled_order
test_group passive 1 no painter-common-layer-popup
unset dockbook_prefix
test_group normal 1 no painter-common-layer-popup || true
'''
        proc = subprocess.run(['bash', '-c', command, 'control', str(self.root)], capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        rows = [line.split('|') for line in calls.read_text().splitlines()]
        self.assertEqual(rows[0][1:3], ['1', '1'])
        self.assertIn('profile-dockbook-controlled-order-clone-tree-scale1/home', rows[0][0])
        self.assertTrue(rows[0][3].endswith('/dockbook-controlled-order-trace.txt'))
        self.assertEqual(rows[1][2], 'UNSET'); self.assertEqual(rows[2][2:4], ['UNSET', 'UNSET'])
        self.assertIn('--timeout-multiplier 0.5', rows[0][4])
        first = exports.read_text().splitlines()[0]
        self.assertIn('check_painter_controlled_order_diagnostic.py', first)
        self.assertIn('dockbook-controlled-order-clone-tree-scale1-gdb-dockbook-wrapper.sh.json', first)
        self.assertNotIn('check_painter_ui_ci.py', first)
    def test_workflow_replaces_only_automatic_diagnostic(self):
        workflow = (TOOLS.parent / '.github/workflows/painter-native-ui.yml').read_text()
        self.assertIn('target: [candidate, baseline-carrier-red, dockbook-controlled-order-3d6f]', workflow)
        self.assertIn('contents: read', workflow)
        self.assertNotIn('continue-on-error:', workflow)
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        self.assertIn('pre-fix-stack-978|dockbook-stack-3d6f|dockbook-controlled-order-3d6f)', driver)
        self.assertIn('--diagnostic-mode "$dockbook_diagnostic_mode"', driver)
        self.assertIn('dockbook_overlay=dockbook-controlled-order-overlay.json', driver)
        self.assertIn('test_group dockbook-clone-tree-scale1 1 no painter-common-layer-popup', driver)
        self.assertIn('test_group dockbook-controlled-order-clone-tree-scale1 1 no painter-common-layer-popup', driver)
        old = (TOOLS / 'check_painter_stack_diagnostic.py').read_text()
        self.assertIn('received signal SIGSEGV', old); self.assertNotIn('SIGTRAP', old)


if __name__ == '__main__':
    unittest.main()
