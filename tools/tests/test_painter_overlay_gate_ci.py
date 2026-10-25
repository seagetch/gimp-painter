#!/usr/bin/env python3
"""Display-independent strict six-case gate and immutable-donor controls."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).parents[1]
EXPECTED = ['/overlay-paint-gate/' + name for name in
            ('native-insertion', 'pending-teardown', 'late-layout', 'late-paint',
             'redraw-token-balance', 'hide-remap')]


class OverlayGateCiTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
    def tearDown(self):
        self.temp.cleanup()
    def checker(self, ids=EXPECTED, result='OK', returncode=0, suffix=''):
        row = dict(name='gimp:app / painter-overlay-paint-gate', result=result,
                   returncode=returncode, duration=1,
                   stdout='1..' + str(len(ids)) + '\n' + ''.join(
                       f'ok {i+1} {name}{suffix}\n' for i, name in enumerate(ids)),
                   stderr='', env={'SECRET': 'not-exported'})
        log = self.root / 'log.json';log.write_text(json.dumps(row))
        output = self.root / 'report.json'
        proc = subprocess.run([sys.executable, '-B', str(TOOLS / 'check_painter_ui_ci.py'),
                '--log', str(log), '--expect', 'painter-overlay-paint-gate',
                '--case-manifest', str(TOOLS / 'painter-overlay-paint-gate-cases.json'),
                '--output', str(output)], capture_output=True, text=True)
        return proc, json.loads(output.read_text())
    def test_exact_six_required_without_images(self):
        manifest = json.loads((TOOLS / 'painter-overlay-paint-gate-cases.json').read_text())
        self.assertEqual(manifest, {'schema_version': 1, 'test': 'painter-overlay-paint-gate', 'case_ids': EXPECTED})
        proc, report = self.checker()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue(report['passed'])
        self.assertNotIn('screenshots', report)
        self.assertNotIn('not-exported', json.dumps(report))
    def test_missing_extra_or_duplicate_case_rejected(self):
        for ids in (EXPECTED[:-1], EXPECTED + ['/overlay-paint-gate/unreviewed'], EXPECTED[:-1] + EXPECTED[:1]):
            proc, report = self.checker(ids=ids)
            self.assertNotEqual(proc.returncode, 0)
            self.assertFalse(report['passed'])
    def test_skip_todo_timeout_failure_rejected(self):
        for values in ({'suffix': ' # SKIP not-ready'}, {'suffix': ' # TODO later'},
                       {'result': 'TIMEOUT', 'returncode': 124}, {'result': 'FAIL', 'returncode': -6}):
            proc, report = self.checker(**values)
            self.assertNotEqual(proc.returncode, 0)
            self.assertFalse(report['passed'])
    def driver_script(self, gate_status=0):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        definitions = driver.split('test_group() {', 1)[1].split('\nif [[ "$mode" == pre-fix-stack-978 ]]', 1)[0]
        calls = self.root / 'calls';exports = self.root / 'exports'
        script = '''set -euo pipefail
run="$1/private"; out="$1/artifacts"; build="$1/build"; repo="$1/repo"
mkdir -p "$run" "$out"
export GIMP_TEST_COMMON_LAYER_POPUP=stale GIMP_TEST_FILTER_LAYER_POPUP=stale GIMP_TEST_OVERLAY_PAINT_GATE=stale
export GIMP_TEST_COMMON_POPUP_DOCK_TRACE=stale GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES=stale
xvfb-run() { printf '%s|%s|%s|%s|%s|%s|%s|%s\\n' "$HOME" "$GDK_SCALE" "${GIMP_TEST_COMMON_LAYER_POPUP:-UNSET}" "${GIMP_TEST_FILTER_LAYER_POPUP:-UNSET}" "${GIMP_TEST_OVERLAY_PAINT_GATE:-UNSET}" "${GIMP_TEST_COMMON_POPUP_DOCK_TRACE:-UNSET}" "${GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES:-UNSET}" "$*" >> CALLS; return 0; }
python3() { printf '%s\\n' "$*" >> EXPORTS; if [[ "$*" == *check_painter_ui_ci.py* ]]; then return STATUS; fi; return 0; }
'''.replace('CALLS', '"' + str(calls) + '"').replace('EXPORTS', '"' + str(exports) + '"').replace('STATUS', str(gate_status))
        command = script + 'test_group() {' + definitions + '''
failed=0
test_group overlay-paint-gate-scale1 1 no painter-overlay-paint-gate || failed=1
test_group overlay-paint-gate-scale2 2 no painter-overlay-paint-gate || failed=1
exit "$failed"
'''
        proc = subprocess.run(['bash', '-c', command, 'control', str(self.root)], capture_output=True, text=True)
        return proc, calls.read_text().splitlines(), exports.read_text()
    def test_both_scales_fresh_profiles_and_shell_flags_unset(self):
        proc, calls, exports = self.driver_script()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(len(calls), 2)
        rows = [row.split('|') for row in calls]
        self.assertNotEqual(rows[0][0], rows[1][0])
        self.assertEqual([row[1] for row in rows], ['1', '2'])
        for row in rows:
            self.assertEqual(row[2:7], ['UNSET'] * 5)
            self.assertIn('-screen 0 1080x1920x24 -nolisten tcp', row[7])
        self.assertEqual(exports.count('--expect painter-overlay-paint-gate'), 2)
        self.assertEqual(exports.count('--case-manifest ' + str(self.root / 'repo/tools/painter-overlay-paint-gate-cases.json')), 2)
        self.assertNotIn('--screenshots', exports)
        self.assertNotIn('--diagnostic-prefix', exports)
    def test_gate_failure_propagates_and_second_scale_still_runs(self):
        proc, calls, _ = self.driver_script(gate_status=1)
        self.assertNotEqual(proc.returncode, 0)
        self.assertEqual(len(calls), 2)
    def test_candidate_adds_gate_without_replacing_previous_gates(self):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        self.assertIn('test_group overlay-paint-gate-scale1 1 no painter-overlay-paint-gate || failed=1', driver)
        self.assertIn('test_group overlay-paint-gate-scale2 2 no painter-overlay-paint-gate || failed=1', driver)
        self.assertIn('test_group common-layer-scale1 1 yes painter-common-layer-popup', driver)
        self.assertIn('test_group common-layer-scale2 2 yes painter-common-layer-popup', driver)
        self.assertIn('test_group adjacent-regressions 1 no "${regressions[@]}" || failed=1', driver)
        self.assertLess(driver.index('if [[ "$mode" == baseline-carrier-red ]]; then\n  stage=baseline-carrier-red'),
                        driver.index('stage=overlay-paint-gate-scale1'))
        self.assertIn('exit 0\nfi\nstage=overlay-paint-gate-scale1', driver)
    def test_historical_donor_is_exact_owned_public_checkout(self):
        driver = (TOOLS / 'run-painter-ui-ci.sh').read_text()
        self.assertIn('dockbook_donor_commit=1618cc5958148c7d534f9e3893681e14fcc94d80', driver)
        self.assertIn('dockbook_donor_tree=253fcbdeb7c39391ddf8bad89b7877e1e18911b8', driver)
        self.assertIn('dockbook_donor="$run/dockbook-donor"', driver)
        self.assertIn('git init "$dockbook_donor"', driver)
        self.assertIn('git -C "$dockbook_donor" remote add origin https://github.com/seagetch/gimp-painter.git', driver)
        self.assertIn('git -C "$dockbook_donor" fetch --no-tags --depth=1 origin "$dockbook_donor_commit"', driver)
        self.assertIn('--source "$src" --donor "$dockbook_donor"', driver)
        self.assertNotIn('--source "$src" --candidate "$repo"', driver)
        self.assertNotIn('safe.directory', driver)
    def test_controlled_attribution_pin_remains_historical(self):
        checker = (TOOLS / 'check_painter_controlled_order_diagnostic.py').read_text()
        self.assertIn("TEST_SHA256 = '78079b8209f0bb4a06f9c44ef15a2ee393c3d917da7ecad213cee8b259c61152'", checker)
        self.assertIn('CALL_LINE = 291', checker)
        self.assertIn('controlled_dock_updates', checker)
    def test_workflow_runs_new_unit_controls(self):
        workflow = (TOOLS.parent / '.github/workflows/painter-native-ui.yml').read_text()
        self.assertIn('test_painter_overlay_gate_ci.py', workflow)
        self.assertIn('target: [candidate, baseline-carrier-red, dockbook-controlled-order-3d6f]', workflow)
        self.assertIn('contents: read', workflow)


if __name__ == '__main__':
    unittest.main()
