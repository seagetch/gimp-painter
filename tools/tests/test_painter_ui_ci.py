#!/usr/bin/env python3
"""Display-independent negative controls for the native CI evidence gate."""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('painter_ui_ci', Path(__file__).parents[1] / 'check_painter_ui_ci.py')
ci = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ci)


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.log = self.root / 'result.json'
    def tearDown(self):
        self.temp.cleanup()
    def row(self, **changes):
        row = dict(name='gimp:app / painter-common-layer-popup', result='OK', returncode=0,
                   stdout='1..2\nok 1 /one\nok 2 /two\n', stderr='', env={'TOKEN': 'not-for-artifacts'})
        row.update(changes)
        return row
    def report(self, rows, cases=None):
        self.log.write_text('\n'.join(json.dumps(row) for row in rows))
        return ci.read_results(self.log, ['painter-common-layer-popup'], cases)
    def test_success_strips_environment(self):
        report = self.report([self.row()], 2)
        self.assertTrue(report['passed'])
        self.assertNotIn('not-for-artifacts', json.dumps(report))
        self.assertNotIn('env', report['tests'][0])
    def test_missing_registration(self):
        self.assertFalse(self.report([])['passed'])
    def test_duplicate_registration(self):
        self.assertFalse(self.report([self.row(), self.row()])['passed'])
    def test_skip_rejected_even_with_zero_exit(self):
        self.assertFalse(self.report([self.row(stdout='ok 1 /one # SKIP no display')])['passed'])
    def test_failed_and_incomplete(self):
        self.assertFalse(self.report([self.row(result='FAIL', returncode=1)])['passed'])
        self.assertFalse(self.report([self.row()], 30)['passed'])
    def test_exact_case_manifest(self):
        self.log.write_text(json.dumps(self.row()))
        self.assertTrue(ci.read_results(self.log, ['painter-common-layer-popup'],
                                       case_ids=['/one', '/two'])['passed'])
        self.assertFalse(ci.read_results(self.log, ['painter-common-layer-popup'],
                                        case_ids=['/one', '/missing'])['passed'])
    def test_committed_manifest_four_routes_and_58_ids(self):
        manifest = json.loads((Path(__file__).parents[1] / 'painter-common-layer-popup-cases.json').read_text())
        ids = manifest['case_ids']
        self.assertEqual(len(ids), 58)
        self.assertEqual(len(set(ids)), 58)
        for route in ci.ROUTES:
            self.assertIn(f'/common-layer-native-popup/{route}/actual-entry', ids)
            self.assertIn(f'/common-layer-native-popup/{route}/lower-controls', ids)
        for route in ci.ROUTES:
            for case in ('closing-provenance-exact', 'closing-provenance-mismatch'):
                self.assertIn(f'/common-layer-native-popup/{route}/{case}', ids)
    def test_intended_baseline_red_only(self):
        path = '/common-layer-native-popup/ordinary-tile/actual-entry'
        row = self.row(result='FAIL', returncode=-6, command=['painter-layer-ui', '-p', path],
                       stdout="ERROR:test-common-layer-popup.cpp:205: 'GTK_IS_WINDOW (popup)' should be TRUE")
        self.log.write_text(json.dumps(row))
        self.assertTrue(ci.read_baseline_red(self.log, path)['passed'])
        row['stdout'] = "ERROR:test-common-layer-popup.cpp:203: 'popup' should not be nullptr"
        self.log.write_text(json.dumps(row))
        self.assertFalse(ci.read_baseline_red(self.log, path)['passed'])
    def test_baseline_timeout_and_wrong_selected_case_rejected(self):
        path = '/common-layer-native-popup/clone-tree/actual-entry'
        row = self.row(result='TIMEOUT', returncode=-15, command=['painter-layer-ui', '-p', path],
                       stdout="ERROR:test-common-layer-popup.cpp:205: 'GTK_IS_WINDOW (popup)' should be TRUE")
        self.log.write_text(json.dumps(row))
        self.assertFalse(ci.read_baseline_red(self.log, path)['passed'])
        row['result'] = 'FAIL'
        self.log.write_text(json.dumps(row))
        self.assertFalse(ci.read_baseline_red(self.log, '/common-layer-native-popup/ordinary-tile/actual-entry')['passed'])
    def pngs(self, width=1080, height=1920, scale=1):
        # Deliberate header-only unit fixture: gate checks header geometry, not
        # a substitute for successful GdkPixbuf save and actual image viewing.
        data = b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', width, height)
        for route in ci.ROUTES:
            for variant in ('', '-bottom'):
                (self.root / f'{route}{variant}-scale{scale}.png').write_bytes(data)
    def test_screenshots_complete_geometry_and_hashes(self):
        self.pngs()
        report = ci.check_pngs(self.root, 1)
        self.assertTrue(report['passed'])
        self.assertEqual(len(report['images']), 8)
        self.assertIn('pending', report['visual_inspection'])
    def test_screenshots_missing_and_wrong_geometry(self):
        self.assertFalse(ci.check_pngs(self.root, 1)['passed'])
        self.pngs(1280, 1024)
        self.assertFalse(ci.check_pngs(self.root, 1)['passed'])
    def test_screenshots_originals_without_bottom_rejected(self):
        self.pngs()
        for route in ci.ROUTES:
            (self.root / f'{route}-bottom-scale1.png').unlink()
        self.assertFalse(ci.check_pngs(self.root, 1)['passed'])
    def test_screenshots_bottom_without_originals_rejected(self):
        self.pngs()
        for route in ci.ROUTES:
            (self.root / f'{route}-scale1.png').unlink()
        self.assertFalse(ci.check_pngs(self.root, 1)['passed'])
    def test_screenshots_extra_or_wrong_scale_rejected(self):
        self.pngs()
        data = (self.root / 'ordinary-tile-scale1.png').read_bytes()
        for name in ('failure-diagnostic.png', 'ordinary-tile-bottom-scale2.png'):
            with self.subTest(name=name):
                path = self.root / name
                path.write_bytes(data)
                self.assertFalse(ci.check_pngs(self.root, 1)['passed'])
                path.unlink()
    def test_screenshots_scale2_complete(self):
        self.pngs(scale=2)
        report = ci.check_pngs(self.root, 2)
        self.assertTrue(report['passed'])
        self.assertEqual(len(report['images']), 8)
        self.assertEqual({item['file'] for item in report['images']},
                         {f'{route}{variant}-scale2.png'
                          for route in ci.ROUTES for variant in ('', '-bottom')})
    def test_screenshots_no_symlink(self):
        self.pngs()
        path = self.root / 'ordinary-tile-scale1.png'
        path.unlink()
        path.symlink_to(self.root / 'clone-tile-scale1.png')
        self.assertFalse(ci.check_pngs(self.root, 1)['passed'])
    def route_report(self, **changes):
        prefix = '/common-layer-native-popup/ordinary-tile'
        ids = [prefix + '/one', prefix + '/two', '/common-layer-native-popup/clone-tree/other']
        row = self.row(stdout=f'1..2\nok 1 {ids[0]}\nok 2 {ids[1]}\n')
        row.update(changes)
        self.log.write_text(json.dumps(row))
        return ci.read_route_diagnostic(self.log, ['painter-common-layer-popup'], ids, prefix)
    def test_route_success_is_never_acceptance(self):
        report = self.route_report()
        self.assertTrue(report['diagnostic_passed'])
        self.assertTrue(report['diagnostic_only'])
        self.assertFalse(report['acceptance_eligible'])
        self.assertNotIn('passed', report)
        self.assertNotIn('not-for-artifacts', json.dumps(report))
        self.assertEqual(report['diagnostic_status'], 'passed_subset')
    def test_route_incomplete_ids_rejected(self):
        self.assertFalse(self.route_report(stdout='1..1\nok 1 /common-layer-native-popup/ordinary-tile/one\n')['diagnostic_passed'])
    def test_route_timeout_is_incomplete(self):
        report = self.route_report(result='TIMEOUT', returncode=-15)
        self.assertFalse(report['diagnostic_passed'])
        self.assertEqual(report['diagnostic_status'], 'incomplete_timeout')
    def test_route_skip_rejected(self):
        self.assertFalse(self.route_report(stdout='ok 1 /common-layer-native-popup/ordinary-tile/one # SKIP\n')['diagnostic_passed'])
    def test_route_prefix_must_be_exact_manifest_route(self):
        self.route_report()
        for prefix in ('/common-layer-native-popup', '/common-layer-native-popup/ordinary-tile-extra'):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                ci.read_route_diagnostic(self.log, ['painter-common-layer-popup'], ['/one'], prefix)
    def test_route_cli_rejects_acceptance_screenshots(self):
        self.route_report()
        tools = Path(__file__).parents[1]
        output = self.root / 'diagnostic.json'
        result = subprocess.run([sys.executable, str(tools / 'check_painter_ui_ci.py'),
            '--log', str(self.log), '--expect', 'painter-common-layer-popup',
            '--case-manifest', str(tools / 'painter-common-layer-popup-cases.json'),
            '--diagnostic-prefix', '/common-layer-native-popup/ordinary-tile',
            '--screenshots', str(self.root), '--scale', '1', '--output', str(output)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        report = json.loads(output.read_text())
        self.assertFalse(report['diagnostic_passed'])
        self.assertFalse(report['acceptance_eligible'])
        self.assertNotIn('passed', report)
    def test_driver_route_bound_and_fresh_capture_isolation(self):
        # Shell orchestration control only: neither Xvfb nor native GUI runs.
        driver = (Path(__file__).parents[1] / 'run-painter-ui-ci.sh').read_text()
        definitions = driver.split('test_group() {', 1)[1].split('\nif [[ "$mode" == pre-fix-stack-978 ]]', 1)[0]
        script = '''set -euo pipefail
run="$1/private"; out="$1/artifacts"; build="$1/build"; repo="$1/repo"
mkdir -p "$run" "$out"
export GIMP_TEST_COMMON_POPUP_SCREENSHOTS=must-be-cleared
xvfb-run() { printf '%s|%s|%s|%s|%s\n' "$HOME" "$GDK_SCALE" "${GIMP_TEST_COMMON_POPUP_SCREENSHOTS:-UNSET}" "$GIMP_TEST_COMMON_POPUP_DIAGNOSTICS" "$*" >> "$1_unused"; }
python3() { return 0; }
'''.replace('"$1_unused"', '"' + str(self.root / 'calls') + '"')
        result = subprocess.run(['bash', '-c', script + 'test_group() {' + definitions + '\nroute_diagnostics 2\n',
                                 'control', str(self.root)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = [line.split('|') for line in (self.root / 'calls').read_text().splitlines()]
        self.assertEqual(len(rows), 4)
        self.assertEqual(len({row[0] for row in rows}), 4)
        for row, route in zip(rows, ci.ROUTES):
            self.assertEqual(row[1:3], ['2', 'UNSET'])
            self.assertIn('/artifacts/route-diagnostics/diagnostics-route-', row[3])
            self.assertIn('--timeout-multiplier 0.5', row[4])
            self.assertIn('--run-prefix /common-layer-native-popup/' + route, row[4])
    def test_driver_diagnosis_never_resets_original_failure(self):
        driver = (Path(__file__).parents[1] / 'run-painter-ui-ci.sh').read_text()
        tail = '\nstage=common-layer-scale1\n' + driver.split('\nstage=common-layer-scale1\n', 1)[1]
        for failure, scale in (('common-layer-scale1', '1'), ('common-layer-scale2', '2'), ('both', '1'), ('none', None)):
            with self.subTest(failure=failure):
                out = self.root / failure
                out.mkdir()
                script = '''set -euo pipefail
out="$1"; failed=0; regressions=(adjacent); failure="$2"
test_group() { if [[ "$failure" == both && "$1" == common-layer-* ]]; then return 1; fi; [[ "$1" != "$failure" ]]; }
route_diagnostics() { printf '%s' "$1" > "$out/diagnostic-scale"; return 0; }
verify_source() { return 0; }
locale_state() { return 0; }
'''
                result = subprocess.run(['bash', '-c', script + tail, 'control', str(out), failure],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 1 if scale else 0, result.stderr)
                self.assertEqual((out / 'acceptance.txt').exists(), scale is None)
                if scale:
                    self.assertEqual((out / 'diagnostic-scale').read_text(), scale)
                else:
                    self.assertFalse((out / 'diagnostic-scale').exists())
                    self.assertIn('16 PNGs', (out / 'acceptance.txt').read_text())


class StackDiagnosticTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        spec = importlib.util.spec_from_file_location('stack_diagnostic',
            Path(__file__).parents[1] / 'check_painter_stack_diagnostic.py')
        self.stack = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.stack)
        self.case = self.stack.SELECTED_CASE
    def tearDown(self):
        self.temp.cleanup()
    def report(self, **changes):
        row = dict(name='gimp:app / painter-common-layer-popup', result='FAIL', returncode=255,
                   command=['/private/gdb-stack-wrapper.sh', '/private/painter-layer-ui', '-p', self.case],
                   env={'TOKEN': 'never-export'}, duration=1,
                   stdout='Thread 1 "painter-layer-ui" received signal SIGSEGV, Segmentation fault.\n#0  crash () at test.cpp:1\n',
                   stderr='')
        row.update(changes)
        log = self.root / 'meson.json'
        log.write_text(json.dumps(row))
        return self.stack.read_diagnostic(log, self.case)
    def test_genuine_sigsegv_stack_is_not_acceptance(self):
        report = self.report()
        self.assertTrue(report['stack_captured'])
        self.assertFalse(report['acceptance_eligible'])
        self.assertTrue(report['diagnostic_only'])
        self.assertNotIn('passed', report)
        self.assertNotIn('command', report['tests'][0])
        self.assertNotIn('never-export', json.dumps(report))
    def test_sigabrt_or_no_frames_not_stack_success(self):
        for output in ('Program received signal SIGABRT, Aborted.\n#0 abort ()\n',
                       'Program received signal SIGSEGV, Segmentation fault.\n',
                       '1..1\nok 1 ' + self.case):
            with self.subTest(output=output):
                self.assertFalse(self.report(stdout=output)['stack_captured'])
    def test_bad_status_or_missing_returncode_cannot_capture_success(self):
        for changes in ({'result': 'OK'}, {'returncode': None}, {'returncode': 0}, {'returncode': '255'}):
            with self.subTest(changes=changes):
                self.assertFalse(self.report(**changes)['stack_captured'])
    def test_timeout_remains_incomplete(self):
        report = self.report(result='TIMEOUT')
        self.assertFalse(report['stack_captured'])
        self.assertEqual(report['diagnostic_status'], 'incomplete_timeout')
    def test_wrong_selector_or_wrapper_rejected(self):
        for command in (['binary', '-p', self.case],
                        ['/private/gdb-stack-wrapper.sh', 'binary', '-p', '/different']):
            self.assertFalse(self.report(command=command)['stack_captured'])
    def test_ptrace_denial_is_reported_not_bypassed(self):
        report = self.report(stdout='warning: Could not trace the inferior process.\nptrace: Operation not permitted.\n')
        self.assertEqual(report['diagnostic_status'], 'incomplete_debugger_or_runtime')
        self.assertFalse(report['stack_captured'])
    def test_output_bounded_and_environment_lines_removed(self):
        output, truncated = self.stack.safe_output('x' * (self.stack.MAX_OUTPUT + 1) +
            '\n>>> TOKEN=never-export\nInherited environment: never-export\n#0 frame ()\n')
        self.assertTrue(truncated)
        self.assertLessEqual(len(output), self.stack.MAX_OUTPUT + 1)
        self.assertNotIn('never-export', output)
        self.assertIn('#0 frame', output)
    def test_stack_invocation_preserves_fresh_profile_and_no_acceptance_capture(self):
        driver = (Path(__file__).parents[1] / 'run-painter-ui-ci.sh').read_text()
        definitions = driver.split('test_group() {', 1)[1].split('\nif [[ "$mode" == pre-fix-stack-978 ]]', 1)[0]
        calls = self.root / 'calls'
        script = """set -euo pipefail
run="$1/private"; out="$1/artifacts"; build="$1/build"; repo="$1/repo"
mkdir -p "$run" "$out"
export GIMP_TEST_COMMON_POPUP_SCREENSHOTS=must-be-cleared
xvfb-run() { printf '%s|%s|%s|%s|%s\\n' "$HOME" "$GDK_SCALE" "${GIMP_TEST_COMMON_POPUP_SCREENSHOTS:-UNSET}" "$GIMP_TEST_COMMON_POPUP_DIAGNOSTICS" "$*" >> "$1_unused"; return 1; }
python3() { printf '%s\\n' "$*" >> "$2_unused"; return 0; }
""".replace('"$1_unused"', '"' + str(calls) + '"').replace('"$2_unused"', '"' + str(self.root / 'exporter') + '"')
        result = subprocess.run(['bash', '-c', script + 'test_group() {' + definitions +
            '\nstack_case=' + self.case + '\ntest_group pre-fix-staged-child-1-scale2 2 no painter-common-layer-popup\n',
            'control', str(self.root)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        row = calls.read_text().strip().split('|')
        self.assertEqual(row[1:3], ['2', 'UNSET'])
        self.assertIn('/private/profile-pre-fix-staged-child-1-scale2/home', row[0])
        self.assertIn('/artifacts/diagnostics-pre-fix-staged-child-1-scale2', row[3])
        self.assertIn('--wrapper ' + str(self.root / 'private/gdb-stack-wrapper.sh'), row[4])
        self.assertIn('--test-args -p ' + self.case, row[4])
        self.assertIn('--timeout-multiplier 0.5', row[4])
        self.assertIn('check_painter_stack_diagnostic.py', (self.root / 'exporter').read_text())
        self.assertIn('pre-fix-staged-child-1-scale2-gdb-stack-wrapper.sh.json', (self.root / 'exporter').read_text())
        self.assertNotIn('check_painter_ui_ci.py', (self.root / 'exporter').read_text())
    def test_driver_stack_wrapper_and_exact_source_isolation(self):
        driver = (Path(__file__).parents[1] / 'run-painter-ui-ci.sh').read_text()
        self.assertIn('stack_commit=' + self.stack.SOURCE_COMMIT, driver)
        self.assertIn('stack_tree=5e0d6678c6eda0f042e458ec5cb3c3c21a6d53a2', driver)
        self.assertIn('stack_test_sha256=88f76f3e98c09168a1a8a9da1363808c25191fafdd1d88d652c462e31321e8c1', driver)
        self.assertIn('--nx --batch --quiet --return-child-result', driver)
        self.assertIn("-ex 'set disable-randomization off'", driver)
        self.assertIn("-ex 'set print frame-arguments none'", driver)
        self.assertIn("-ex 'set print entry-values no'", driver)
        self.assertIn("-ex 'thread apply all bt -no-filters 40'", driver)
        self.assertIn('ulimit -c 0', driver)
        self.assertIn('test_group pre-fix-staged-child-1-scale2 2 no painter-common-layer-popup', driver)
        block = driver.split('stage=historical-stack-diagnostic', 1)[1].split('stage=baseline-carrier-red', 1)[0]
        self.assertNotIn('prepare-painter-popup-baseline.py', block)
        self.assertNotIn('acceptance.txt', block)


if __name__ == '__main__':
    unittest.main()
