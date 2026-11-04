#!/usr/bin/env python3
"""Draft-validator tests only; these are NOT Brush GUI/runtime evidence."""
import copy
import contextlib
import io
import sys
from unittest import mock
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / (name + '.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


gate = module('check_brush_ci')
overlay = module('prepare_brush_baseline')


def row():
    return {'name': gate.TEST, 'result': 'FAIL', 'returncode': -6,
            'command': ['/build/app/tests/painter-compact-options', '-p', gate.CASE],
            'stdout': '# ' + gate.STOCK + '\nBail out! ERROR:/src/app/tests/test-painter-brush-thumbnail-popup.inc:274:brush_popup_assert_body: \'body\' should not be NULL\n',
            'stderr': '', 'duration': 2.0}


class Baseline(unittest.TestCase):
    def test_intended_failure(self):
        self.assertTrue(gate.baseline_red([row()])['passed'])

    def test_only_expected_abort(self):
        for result, code in [('OK', 0), ('SKIP', 77), ('TIMEOUT', -15), ('FAIL', 1), ('FAIL', -11), ('FAIL', True)]:
            r = row(); r.update(result=result, returncode=code)
            self.assertFalse(gate.baseline_red([r])['passed'], (result, code))

    def test_no_window_only_oracle(self):
        r = row(); r['stdout'] = 'GTK_IS_WINDOW (popup) should be TRUE'
        self.assertFalse(gate.baseline_red([r])['passed'])

    def test_missing_body_is_required(self):
        r = row(); r['stdout'] = r['stdout'].replace("'body'", "'host'")
        self.assertFalse(gate.baseline_red([r])['passed'])

    def test_real_stock_marker_required(self):
        for bad in ('extended-body=1', 'carrier=GtkWindow', 'selector=1'):
            r = row()
            original = {'extended-body=1':'extended-body=0', 'carrier=GtkWindow':'carrier=GimpContainerPopup', 'selector=1':'selector=0'}[bad]
            r['stdout'] = r['stdout'].replace(original, bad)
            self.assertFalse(gate.baseline_red([r])['passed'])

    def test_exact_case_command(self):
        for command in ([], ['painter-compact-options'], ['painter-compact-options', '-p', '/wrong'],
                        ['painter-compact-options', '-p', gate.CASE, '-p', '/other'],
                        ['painter-compact-options', '-p', gate.CASE, '-s', '/other']):
            r = row(); r['command'] = command
            self.assertFalse(gate.baseline_red([r])['passed'])

    def test_one_registration(self):
        self.assertFalse(gate.baseline_red([])['passed'])
        self.assertFalse(gate.baseline_red([row(), row()])['passed'])
        r = row(); r['name'] = 'wrong'
        self.assertFalse(gate.baseline_red([r])['passed'])

    def test_no_unrelated_failure(self):
        for line in ('ok 1 /other # SKIP no X11', 'AddressSanitizer: heap-use-after-free',
                     'Gtk-WARNING: missing theme', 'Bail out! ERROR:foo.c:3:foo: assertion failed: (x)'):
            r = row(); r['stderr'] = line
            self.assertFalse(gate.baseline_red([r])['passed'])

    def test_no_environment_or_command_in_export(self):
        r = row(); r['env'] = {'SECRET': 'hidden'}
        report = gate.baseline_red([r])
        self.assertNotIn('env', report['tests'][0])
        self.assertNotIn('command', report['tests'][0])


class PositiveSanitizerDiagnostics(unittest.TestCase):
    def report(self, output='', stream='stderr'):
        r = {'passed': True, 'errors': [], 'tests': [
            {'name': 'gimp:app / ' + gate.TEST, 'result': 'OK',
             'returncode': 0, 'stdout': '', 'stderr': ''}]}
        r['tests'][0][stream] = output
        return r

    def test_parent_ok_child_lsan_in_either_stream_is_rejected(self):
        for stream in ('stdout', 'stderr'):
            r = self.report('==22109==ERROR: LeakSanitizer: detected memory leaks', stream)
            gate.reject_sanitizer_diagnostics(r)
            self.assertFalse(r['passed'])
            self.assertEqual(r['tests'][0]['result'], 'OK')
            self.assertEqual(r['tests'][0]['returncode'], 0)
            self.assertIn(stream, r['errors'][0])

    def test_actual_asan_ubsan_summary_and_fatal_markers_rejected(self):
        for text in ('==plugin==42==ERROR: AddressSanitizer: heap-use-after-free',
                     'SUMMARY: AddressSanitizer: 240 byte(s) leaked in 2 allocation(s).',
                     'SUMMARY: UndefinedBehaviorSanitizer: undefined-behavior foo.c:5',
                     'SUMMARY: LeakSanitizer: 240 byte(s) leaked',
                     'foo.c:5:7: runtime error: signed integer overflow',
                     'AddressSanitizer:DEADLYSIGNAL',
                     'UndefinedBehaviorSanitizer:DEADLYSIGNAL',
                     '==42==LeakSanitizer has encountered a fatal error.'):
            self.assertFalse(gate.reject_sanitizer_diagnostics(self.report(text))['passed'], text)

    def test_benign_sanitizer_names_and_flags_do_not_fail(self):
        text = 'AddressSanitizer and UndefinedBehaviorSanitizer enabled; LeakSanitizer default. -fsanitize=address,undefined ASAN_OPTIONS=halt_on_error=1'
        self.assertTrue(gate.reject_sanitizer_diagnostics(self.report(text))['passed'])

    def test_existing_failure_not_relabeled_or_removed(self):
        r = self.report(); r.update(passed=False, errors=['original case failure'])
        self.assertFalse(gate.reject_sanitizer_diagnostics(r)['passed'])
        self.assertEqual(r['errors'], ['original case failure'])

    def test_one_error_records_both_streams_without_raw_snippet(self):
        r = self.report('==42==ERROR: LeakSanitizer: private-path-detail')
        r['tests'][0]['stdout'] = 'foo.c:2: runtime error: private-detail'
        gate.reject_sanitizer_diagnostics(r)
        self.assertEqual(len(r['errors']), 1)
        self.assertIn('stdout, stderr', r['errors'][0])
        self.assertNotIn('private', r['errors'][0])

    def test_cli_rejects_child_leak_despite_passing_common_parent(self):
        common_path = ROOT.parent / 'ci-donor/tools/check_painter_ui_ci.py'
        if not common_path.is_file():
            common_path = ROOT.parents[1] / 'tools/check_painter_ui_ci.py'
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp); log = tmp / 'meson.json'; out = tmp / 'result.json'; manifest = tmp / 'cases.json'
            r = {'name': 'gimp:app / ' + gate.TEST, 'result': 'OK', 'returncode': 0,
                 'duration': 1, 'stdout': 'TAP version 14\n1..1\nok 1 ' + gate.CASE + '\n',
                 'stderr': '==22109==ERROR: LeakSanitizer: detected memory leaks\n',
                 'env': {'PRIVATE': 'hidden'}, 'command': ['private-command']}
            log.write_text(json.dumps(r) + '\n')
            manifest.write_text(json.dumps({'schema_version': 1, 'test': gate.TEST, 'case_ids': [gate.CASE]}))
            self.assertTrue(gate.load_common(common_path).read_results(log, [gate.TEST], case_ids=[gate.CASE])['passed'])
            argv = ['check_brush_ci.py', '--log', str(log), '--output', str(out),
                    '--common-checker', str(common_path), '--case-manifest', str(manifest)]
            with mock.patch.object(sys, 'argv', argv), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(gate.main(), 1)
            result = json.loads(out.read_text())
            self.assertFalse(result['passed']); self.assertTrue(any('Sanitizer diagnostic' in x for x in result['errors']))
            self.assertFalse(result['raw_environment_exported'])
            self.assertNotIn('env', result['tests'][0]); self.assertNotIn('command', result['tests'][0])


class ProtocolGate(unittest.TestCase):
    def run_cli(self, ids=None, omitted=False, leak=False, skip=False):
        common = ROOT.parent / 'ci-donor/tools/check_painter_ui_ci.py'
        if not common.is_file(): common = ROOT.parents[1] / 'tools/check_painter_ui_ci.py'
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp); log = tmp/'meson.json'; out = tmp/'result.json'; manifest = tmp/'cases.json'
            actual = gate.PROTOCOL_CASES[:-1] if omitted else gate.PROTOCOL_CASES
            output = 'TAP version 14\n1..%d\n' % len(actual)
            output += ''.join('ok %d %s%s\n' % (i, case, ' # SKIP wrong' if skip and i == 1 else '') for i, case in enumerate(actual, 1))
            log.write_text(json.dumps({'name':'gimp:libgimp+C / plug-in-protocol','result':'OK','returncode':0,'duration':1,'stdout':output,'stderr':'==fixture==42==ERROR: LeakSanitizer: detected memory leaks' if leak else ''})+'\n')
            manifest.write_text(json.dumps({'schema_version':1,'test':gate.PROTOCOL,'case_ids':gate.PROTOCOL_CASES if ids is None else ids}))
            with mock.patch.object(sys,'argv',['gate','--log',str(log),'--output',str(out),'--common-checker',str(common),'--case-manifest',str(manifest)]), contextlib.redirect_stdout(io.StringIO()):
                status=gate.main()
            return status,json.loads(out.read_text())

    def test_exact_twelve_manifest_passes_separately(self):
        manifest=json.loads((ROOT/'protocol-cases.json').read_text())
        self.assertEqual(manifest['test'],gate.PROTOCOL)
        self.assertEqual(manifest['case_ids'],gate.PROTOCOL_CASES)
        self.assertEqual(len(set(gate.PROTOCOL_CASES)),12)
        self.assertTrue(all(x.startswith('/plug-in-protocol/') for x in gate.PROTOCOL_CASES))
        self.assertEqual(self.run_cli()[0],0)
        brush=json.loads((ROOT/'brush-cases.json').read_text())['case_ids']
        prior=json.loads((ROOT/'previous-positive-cases.json').read_text())['case_ids']+json.loads((ROOT/'toolbar-window-cases.json').read_text())['case_ids']
        self.assertEqual(len(set(brush+prior)),153)
        self.assertFalse(set(gate.PROTOCOL_CASES)&set(brush+prior))

    def test_incomplete_or_wrong_protocol_manifest_rejected(self):
        for ids in (gate.PROTOCOL_CASES[:-1], [gate.CASE], gate.PROTOCOL_CASES[::-1]):
            self.assertEqual(self.run_cli(ids=ids)[0],1)

    def test_protocol_missing_case_skip_and_child_leak_rejected(self):
        for kwargs in ({'omitted':True},{'skip':True},{'leak':True}):
            self.assertEqual(self.run_cli(**kwargs)[0],1)


class ChoiceGate(unittest.TestCase):
    def run_cli(self, ids=None, omitted=False, leak=False, skip=False):
        common = ROOT.parent / 'ci-donor/tools/check_painter_ui_ci.py'
        if not common.is_file(): common = ROOT.parents[1] / 'tools/check_painter_ui_ci.py'
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp); log = tmp/'meson.json'; out = tmp/'result.json'; manifest = tmp/'cases.json'
            actual = gate.CHOICE_CASES[:-1] if omitted else gate.CHOICE_CASES
            output = 'TAP version 14\n1..%d\n' % len(actual)
            output += ''.join('ok %d %s%s\n' % (i, case, ' # SKIP wrong' if skip and i == 1 else '') for i, case in enumerate(actual, 1))
            log.write_text(json.dumps({'name':'gimp:libgimpbase / choice-lifecycle','result':'OK','returncode':0,'duration':1,'stdout':output,'stderr':'==fixture==42==ERROR: LeakSanitizer: detected memory leaks' if leak else ''})+'\n')
            manifest.write_text(json.dumps({'schema_version':1,'test':gate.CHOICE,'case_ids':gate.CHOICE_CASES if ids is None else ids}))
            with mock.patch.object(sys,'argv',['gate','--log',str(log),'--output',str(out),'--common-checker',str(common),'--case-manifest',str(manifest)]), contextlib.redirect_stdout(io.StringIO()):
                status=gate.main()
            return status,json.loads(out.read_text())

    def test_exact_four_manifest_passes_separately(self):
        manifest=json.loads((ROOT/'choice-cases.json').read_text())
        self.assertEqual(manifest['test'],gate.CHOICE)
        self.assertEqual(manifest['case_ids'],gate.CHOICE_CASES)
        self.assertEqual(len(set(gate.CHOICE_CASES)),4)
        self.assertTrue(all(x.startswith('/choice-lifecycle/') for x in gate.CHOICE_CASES))
        self.assertEqual(self.run_cli()[0],0)
        brush=json.loads((ROOT/'brush-cases.json').read_text())['case_ids']
        prior=json.loads((ROOT/'previous-positive-cases.json').read_text())['case_ids']+json.loads((ROOT/'toolbar-window-cases.json').read_text())['case_ids']
        self.assertEqual(len(set(brush+prior)),153)
        self.assertFalse(set(gate.CHOICE_CASES)&set(brush+prior))

    def test_incomplete_or_wrong_protocol_manifest_rejected(self):
        for ids in (gate.CHOICE_CASES[:-1], [gate.CASE], gate.CHOICE_CASES[::-1]):
            self.assertEqual(self.run_cli(ids=ids)[0],1)

    def test_protocol_missing_case_skip_and_child_leak_rejected(self):
        for kwargs in ({'omitted':True},{'skip':True},{'leak':True}):
            self.assertEqual(self.run_cli(**kwargs)[0],1)


    def test_historical_two_case_choice_subset_cannot_replace_four_case_gate(self):
        self.assertEqual(gate.CHOICE_CASES[:2], ['/choice-lifecycle/direct-owned-qdata', '/choice-lifecycle/param-spec-ownership-and-values'])
        self.assertEqual(gate.CHOICE_CASES[2:], ['/choice-lifecycle/wire-roundtrip', '/choice-lifecycle/wire-truncated-choice'])
        self.assertEqual(self.run_cli(ids=gate.CHOICE_CASES[:2])[0],1)
        targets=json.loads((ROOT/'build-targets.json').read_text())
        self.assertEqual(targets['supplementary_choice_case_count'],4)
        self.assertEqual(len(targets['candidate_union']),35)


class GeometryGate(unittest.TestCase):
    def run_cli(self, ids=None, omitted=False, leak=False, skip=False):
        common = ROOT.parent / 'ci-donor/tools/check_painter_ui_ci.py'
        if not common.is_file(): common = ROOT.parents[1] / 'tools/check_painter_ui_ci.py'
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp); log = tmp/'meson.json'; out = tmp/'result.json'; manifest = tmp/'cases.json'
            actual = gate.GEOMETRY_CASES[:-1] if omitted else gate.GEOMETRY_CASES
            output = 'TAP version 14\n1..%d\n' % len(actual)
            output += ''.join('ok %d %s%s\n' % (i, case, ' # SKIP wrong' if skip and i == 1 else '') for i, case in enumerate(actual, 1))
            log.write_text(json.dumps({'name':'gimp:painter / painter-brush-geometry','result':'OK','returncode':0,'duration':1,'stdout':output,'stderr':'==fixture==42==ERROR: LeakSanitizer: detected memory leaks' if leak else ''})+'\n')
            manifest.write_text(json.dumps({'schema_version':1,'test':gate.GEOMETRY,'case_ids':gate.GEOMETRY_CASES if ids is None else ids}))
            with mock.patch.object(sys,'argv',['gate','--log',str(log),'--output',str(out),'--common-checker',str(common),'--case-manifest',str(manifest)]), contextlib.redirect_stdout(io.StringIO()):
                status=gate.main()
            return status,json.loads(out.read_text())

    def test_exact_seven_manifest_passes_separately(self):
        manifest=json.loads((ROOT/'geometry-cases.json').read_text())
        self.assertEqual(manifest['test'],gate.GEOMETRY)
        self.assertEqual(manifest['case_ids'],gate.GEOMETRY_CASES)
        self.assertEqual(len(set(gate.GEOMETRY_CASES)),7)
        self.assertTrue(all(x.startswith('/painter/geometry/') for x in gate.GEOMETRY_CASES))
        self.assertEqual(self.run_cli()[0],0)
        brush=json.loads((ROOT/'brush-cases.json').read_text())['case_ids']
        prior=json.loads((ROOT/'previous-positive-cases.json').read_text())['case_ids']+json.loads((ROOT/'toolbar-window-cases.json').read_text())['case_ids']
        self.assertEqual(len(set(brush+prior)),153)
        self.assertFalse(set(gate.GEOMETRY_CASES)&set(brush+prior))

    def test_incomplete_or_wrong_protocol_manifest_rejected(self):
        for ids in (gate.GEOMETRY_CASES[:-1], [gate.CASE], gate.GEOMETRY_CASES[::-1]):
            self.assertEqual(self.run_cli(ids=ids)[0],1)

    def test_protocol_missing_case_skip_and_child_leak_rejected(self):
        for kwargs in ({'omitted':True},{'skip':True},{'leak':True}):
            self.assertEqual(self.run_cli(**kwargs)[0],1)


class Images(unittest.TestCase):
    def test_geometry_precheck_is_not_visual_acceptance(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'caller-selected.png'
            # Header fixture only: proves dimension logic, not actual decoding.
            path.write_bytes(b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 1080, 1920))
            report = gate.screenshot(path, 2, 'Brush popup logical=0/0 510x740 scale=2')
            self.assertTrue(report['passed'])
            self.assertIn('PENDING', report['visual_inspection'])
            self.assertFalse(gate.screenshot(path, 1, 'Brush popup logical=0/0 510x740 scale=2')['passed'])
            self.assertFalse(gate.screenshot(path, 1, '')['passed'])

    def test_lower_capture_requires_all_physical_controls(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'bottom.png'
            path.write_bytes(b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 1080, 1920))
            geometry = 'Brush popup logical=0/0 510x740 scale=2\n'
            marker = 'Brush lower controls reachable=5 numeric-entry=41 preview-visible=1 wheel-events=4 scale=2\n'
            controls = ''.join('Brush lower physical control '+x+' focused/edited value=1\n' for x in ('radius','spikes','hardness','aspect-ratio','angle'))
            self.assertTrue(gate.screenshot(path, 2, geometry + marker + controls, gate.LOWER_CASE)['passed'])
            self.assertFalse(gate.screenshot(path, 2, geometry + marker, gate.LOWER_CASE)['passed'])
            self.assertFalse(gate.screenshot(path, 2, geometry + controls, gate.LOWER_CASE)['passed'])
            self.assertFalse(gate.screenshot(path, 2, geometry + marker + controls + controls, gate.LOWER_CASE)['passed'])

    def test_missing_symlink_wrong_dimensions(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'image.png'
            self.assertFalse(gate.screenshot(path, 1, '')['passed'])
            path.write_bytes(b'\x89PNG\r\n\x1a\n' + struct.pack('>I', 13) + b'IHDR' + struct.pack('>II', 540, 960))
            self.assertFalse(gate.screenshot(path, 1, 'Brush popup logical=0/0 510x740 scale=1')['passed'])
            link = Path(tmp) / 'link.png'; link.symlink_to(path)
            self.assertFalse(gate.screenshot(link, 1, '')['passed'])


class Overlay(unittest.TestCase):
    def test_two_insertions_only(self):
        original = b'before\n#include "test-painter-clone-popup.inc"\nmiddle\n  register_clone_popup_tests ();\nafter\n'
        actual = overlay.inject_main(original)
        restored = actual.replace(b'#include "test-painter-brush-thumbnail-popup.inc"\n', b'').replace(b'  register_brush_thumbnail_popup_tests ();\n', b'')
        self.assertEqual(original, restored)

    def test_ambiguous_anchor_rejected(self):
        for data in (b'', b'#include "test-painter-clone-popup.inc"\n' * 2,
                     b'#include "test-painter-brush-thumbnail-popup.inc"\n'):
            with self.assertRaises(ValueError):
                overlay.inject_main(data)

    def test_unknown_baseline_blocks_preparation(self):
        pins = json.loads((ROOT / 'pins.json').read_text())
        with self.assertRaisesRegex(ValueError, 'BLOCKED'):
            overlay.commits(pins)
        pins.update(accepted_task307_baseline_commit='a' * 40, candidate_final_commit='b' * 40, draft_only=True)
        with self.assertRaisesRegex(ValueError, 'draft_only'):
            overlay.commits(pins)
        pins['draft_only'] = False
        self.assertEqual(overlay.commits(pins), ['a' * 40, 'b' * 40])


class Cases(unittest.TestCase):
    def test_exact_positive_union(self):
        groups = json.loads((ROOT / 'positive-groups.json').read_text())
        manifest = json.loads((ROOT / 'previous-positive-cases.json').read_text())
        actual = [x for group in groups['groups'] for x in group['case_ids']]
        toolbar = json.loads((ROOT / 'toolbar-window-cases.json').read_text())
        self.assertEqual(len(actual), 132)
        self.assertEqual(set(actual), set(manifest['case_ids'] + toolbar['case_ids']))
        self.assertEqual(len(set(actual)), 132)
        self.assertEqual(groups['excluded_case_ids'], [])
        self.assertEqual(len(groups['groups']), 7)
        self.assertIn('/standard-toggle-popup/baseline-jitter', actual)
        self.assertIn('/standard-toggle-popup/baseline-texture', actual)
        self.assertIn('/standard-toggle-popup/baseline-smoothing', actual)
        self.assertEqual(len(toolbar['case_ids']), 26)
        self.assertIn('/transform-native-popup/baseline-rotate', actual)
        self.assertIn('/painter-compact-options/rectangle-units-native-baseline', actual)

    def test_overlay_reveal_precedes_strict_bar_wheel(self):
        source = (ROOT / 'lower-case.inc.fragment').read_text()
        wheel = source[source.index('brush_popup_lower_wheel ('):source.index('static guint\nbrush_popup_lower_bottom')]
        self.assertLess(wheel.index('brush_popup_lower_reveal_bar (s)'), wheel.index('brush_popup_lower_aim (s, s->bar, FALSE)'))
        self.assertLess(wheel.index('brush_popup_lower_aim (s, s->bar, FALSE)'), wheel.index('s->input.button'))
        self.assertIn('brush_popup_lower_aim (s, hover, FALSE)', source)
        self.assertIn('2 * G_TIME_SPAN_SECOND', source)
        for forbidden in ('gtk_scrolled_window_set_overlay_scrolling', 'gdk_window_show', 'gtk_widget_set_opacity', 'gtk_adjustment_set_value'):
            self.assertNotIn(forbidden, source)

    def test_numeric_point_uses_strict_shared_aim(self):
        source = (ROOT / 'lower-case.inc.fragment').read_text()
        click = source[source.index('brush_popup_lower_click_number ('):source.index('#endif\n\nstatic void\nbrush_popup_lower_controls_actual_input')]
        self.assertLess(click.index('brush_popup_lower_aim (s, spin, TRUE)'), click.index('s->input.button'))
        self.assertNotIn('input.motion', click)
        aim = source[source.index('brush_popup_lower_aim ('):source.index('static void\nbrush_popup_lower_reveal_bar')]
        self.assertIn('brush_popup_lower_wait_point (s, target, number)', aim)
        self.assertIn('gdk_device_get_window_at_position', aim)
        self.assertIn('g_assert_cmpint (px, ==, point.x)', aim)
        self.assertIn('g_assert_cmpint (py, ==, point.y)', aim)
        self.assertIn('gtk_widget_is_ancestor (GTK_WIDGET (owner), target)', aim)

    def test_retained_children_and_host_first_getters(self):
        source = (ROOT / 'lower-case.inc.fragment').read_text()
        for field in ('host', 'body', 'viewport', 'bar', 'adjustment', 'preview', 'display', 'pointer'):
            self.assertIn('input.' + field + ' = g_object_ref', source)
        self.assertIn('input.spins[i] = g_object_ref', source)
        guard = source[source.index('brush_popup_lower_live ('):source.index('/* Geometry/adjustment')]
        self.assertLess(guard.index('gtk_widget_get_mapped (s->host)'), guard.index('gtk_bin_get_child'))
        self.assertLess(guard.index('gtk_widget_get_mapped (target)'), guard.index('gtk_range_get_adjustment'))
        value = source[source.index('brush_popup_lower_value ('):source.index('static void\nbrush_popup_lower_wheel')]
        self.assertLess(value.index('brush_popup_lower_live'), value.index('gtk_adjustment_get_value'))

    def test_frame_clock_owned_and_callback_guarded(self):
        source = (ROOT / 'lower-case.inc.fragment').read_text()
        self.assertNotIn('transform_popup_wait_stable_geometry', source)
        wait = source[source.index('brush_popup_lower_wait_point ('):source.index('/* Strict point-based aim')]
        self.assertLess(wait.index('g_object_ref (clock)'), wait.index('g_signal_connect'))
        self.assertIn('g_signal_handler_disconnect (clock, handler); g_object_unref (clock)', wait)
        callback = source[source.index('brush_popup_lower_after_paint ('):source.index('static BrushLowerPoint\nbrush_popup_lower_wait_point')]
        self.assertIn('brush_popup_lower_point (f->scope, f->target, f->number)', callback)
        point = source[source.index('brush_popup_lower_point ('):source.index('static gboolean\nbrush_popup_lower_same_point')]
        self.assertLess(point.index('brush_popup_lower_live'), point.index('gtk_widget_get_allocated_width'))

    def test_hover_semantic_entry_precedes_stable_aim(self):
        source = (ROOT / 'lower-case.inc.fragment').read_text()
        wheel = source[source.index('brush_popup_lower_wheel ('):source.index('static guint\nbrush_popup_lower_bottom')]
        self.assertLess(wheel.index('brush_popup_lower_reveal_bar'), wheel.index('brush_popup_lower_enter_bar'))
        self.assertLess(wheel.index('brush_popup_lower_enter_bar'), wheel.index('brush_popup_lower_aim'))
        self.assertLess(wheel.index('brush_popup_lower_aim'), wheel.index('s->input.button'))
        enter = source[source.index('brush_popup_lower_enter_bar ('):source.index('static void\nbrush_popup_lower_wheel')]
        for required in ('gdk_device_get_window_at_position', '"overlay-indicator"', '"hovering"',
                         'g_assert_true (intended); g_assert_true (hovering)', '3 * G_TIME_SPAN_SECOND'):
            self.assertIn(required, enter)
        for forbidden in ('input.button', 'g_usleep', 'rectangle_popup_settle', 'gtk_style_context_add_class'):
            self.assertNotIn(forbidden, enter)

    def test_runtime_preflight_checks_current_source_hashes(self):
        runner = (ROOT / 'run_brush_runtime.sh').read_text()
        for pin in ('test_include_sha256', 'candidate_main_sha256', 'case_manifest_sha256'):
            self.assertIn(pin, runner)
        self.assertIn("if mode != 'baseline-red':", runner)
        self.assertIn('not path.is_symlink()', runner)

    def test_original_twenty_plus_one_exact(self):
        manifest = json.loads((ROOT / 'brush-cases.json').read_text())
        self.assertEqual(len(manifest['case_ids']), 21)
        self.assertEqual(len(set(manifest['case_ids'])), 21)
        self.assertTrue(all(x.startswith('/brush-thumbnail-native-popup/') for x in manifest['case_ids']))
        original = json.loads((ROOT / 'original20-cases.reference.json').read_text())['case_ids']
        self.assertEqual(manifest['case_ids'][:-1], original)
        self.assertEqual(manifest['case_ids'][-1], gate.LOWER_CASE)



class WireLsanRedControl(unittest.TestCase):
    def setUp(self):
        self.wire = module('wire_lsan_red')

    def sample(self):
        # Source-derived synthetic oracle fixture, NOT an observed native result.
        blocks=[]
        for line,size,count in ((1139,11,1),(1155,20,2),(1159,20,2),(1161,16,1)):
            blocks.append('Direct leak of %d byte(s) in %d object(s) allocated from:\n'
                '    #0 0x100 in malloc asan_malloc_linux.cpp:69\n'
                '    #1 0x200 in _gimp_wire_read_string ../wire-red-source/libgimpbase/gimpwire.c:455\n'
                '    #2 0x300 in _gp_param_def_read ../wire-red-source/libgimpbase/gimpprotocol.c:%d\n'
                '    #3 0x400 in wire_roundtrip ../wire-red-source/libgimpbase/test-choice-lifecycle.c:233\n' % (size,count,line))
        return {'name':'gimp:libgimpbase / choice-lifecycle','result':'FAIL','returncode':1,'duration':0.2,
                'stdout':'TAP version 14\nok 1 /choice-lifecycle/wire-roundtrip\n1..1\n',
                'stderr':'==test-choice-lifecycle==42==ERROR: LeakSanitizer: detected memory leaks\n\n'+'\n'.join(blocks)+'\nSUMMARY: AddressSanitizer: 67 byte(s) leaked in 6 allocation(s).\n',
                'command':['/owned/wire-red-build/libgimpbase/test-choice-lifecycle','--tap','-p',self.wire.CASE]}

    def test_strict_expected_red_is_negative_only_not_candidate_pass(self):
        report=self.wire.check([self.sample()])
        self.assertTrue(report['control_passed'],report['errors'])
        self.assertTrue(report['negative_control']);self.assertFalse(report['acceptance_eligible']);self.assertNotIn('passed',report)
        self.assertEqual(report['attributed_leak_bytes'],67);self.assertEqual(report['attributed_leak_allocations'],6)
        self.assertNotIn('command',report['tests'][0])

    def test_missing_duplicate_or_wrong_registration_fails(self):
        for rows in ([],[self.sample(),self.sample()],[dict(self.sample(),name='other')]):
            self.assertFalse(self.wire.check(rows)['control_passed'])

    def test_pass_timeout_abort_and_other_exit_codes_are_not_expected_lsan_red(self):
        for result,status in (('OK',0),('FAIL',-6),('FAIL',134),('TIMEOUT',-15),('FAIL',23),('FAIL',0)):
            self.assertFalse(self.wire.check([dict(self.sample(),result=result,returncode=status)])['control_passed'])

    def test_functional_case_must_finish_exactly_without_skip_or_extra_results(self):
        for stdout in ('','not ok 1 '+self.wire.CASE+'\n1..1\n','ok 1 '+self.wire.CASE+' # SKIP no\n1..1\n','ok 1 '+self.wire.CASE+'\n1..2\n','ok 1 '+self.wire.CASE+'\n1..1\nok 2 /extra # ordinary comment\n'):
            self.assertFalse(self.wire.check([dict(self.sample(),stdout=stdout)])['control_passed'])

    def test_selector_prefix_extra_skip_or_wrong_executable_rejected(self):
        row=self.sample()
        for args in ([row['command'][0],'-p','/choice-lifecycle'],row['command']+['-p',self.wire.CASE],row['command']+['-s',self.wire.CASE],['/wrong/other','-p',self.wire.CASE]):
            self.assertFalse(self.wire.check([dict(row,command=args)])['control_passed'])

    def test_stdout_sanitizer_diagnostic_cannot_hide_behind_expected_stderr(self):
        row=self.sample()
        for diagnostic in ('ERROR: AddressSanitizer: heap-use-after-free', '==other==42==ERROR: LeakSanitizer: detected memory leaks', 'SUMMARY: AddressSanitizer: 1 byte(s) leaked in 1 allocation(s).'):
            self.assertFalse(self.wire.check([dict(row,stdout=row['stdout']+diagnostic+'\n')])['control_passed'])

    def test_concatenated_selector_cannot_hide_behind_exact_leaf(self):
        row=self.sample()
        for selector in ('-p/other','-p=/choice-lifecycle'):
            self.assertFalse(self.wire.check([dict(row,command=row['command']+[selector])])['control_passed'])

    def test_plain_assertion_setup_or_unsupported_lsan_ptrace_fails(self):
        for stderr in ('Bail out! assertion failed','ERROR: setup failed','LeakSanitizer has encountered a fatal error\nLSan does not work under ptrace'):
            self.assertFalse(self.wire.check([dict(self.sample(),stderr=stderr)])['control_passed'])

    def test_unrelated_asan_ubsan_or_second_process_leak_fails(self):
        row=self.sample()
        for text in ('ERROR: AddressSanitizer: heap-use-after-free','source.c:1:2: runtime error: overflow','==child==88==ERROR: LeakSanitizer: detected memory leaks','AddressSanitizer:DEADLYSIGNAL'):
            self.assertFalse(self.wire.check([dict(row,stderr=row['stderr']+text)])['control_passed'])

    def test_every_allocation_block_must_have_exact_old_decoder_and_test_frames(self):
        row=self.sample()
        for old,new in (('_gp_param_def_read','unrelated_function'),('gimpprotocol.c:1139','gimpprotocol.c:999'),('gimpwire.c:455','other.c:455'),('wire_roundtrip','other_test'),('Direct leak of','Indirect leak of')):
            self.assertFalse(self.wire.check([dict(row,stderr=row['stderr'].replace(old,new,1))])['control_passed'])

    def test_allocations_must_reconcile_with_real_summary(self):
        row=self.sample()
        self.assertFalse(self.wire.check([dict(row,stderr=row['stderr'].replace('67 byte(s) leaked','68 byte(s) leaked'))])['control_passed'])
        self.assertFalse(self.wire.check([dict(row,stderr=row['stderr'].replace('6 allocation(s)','7 allocation(s)'))])['control_passed'])

    def test_immutable_c5_and_reviewed_test_constants(self):
        self.assertEqual(self.wire.OLD_COMMIT,'da070df3717723e75f0757113c2da2768949d676')
        self.assertEqual(self.wire.OLD_TREE,'544a91ca352614a58f024d1e3cd813ae0226df31')
        self.assertEqual(self.wire.TEST_SHA,'46807f0fe5b7c64572da7be56855b2a41e23eb089c8fb990793b27bdd28ac8d3')
        self.assertEqual(self.wire.OLD_FILES['libgimpbase/gimpprotocol.c'],'42b00612efd048ec5e0c1ae278ad1c6244d16bf018308c8685852017a93e07bc')
        self.assertEqual(self.wire.OLD_FILES['libgimpbase/gimpwire.c'],'3ddd68f2d5d8c4315a32026e0d13954b45cc1d97a1193d7cfd568af1e926a366')

    def test_overlay_verification_rejects_extra_changes_untracked_or_hash_drift(self):
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp);(source/self.wire.TEST).parent.mkdir(parents=True)
            (source/self.wire.TEST).write_bytes(b'test');(source/'production.c').write_bytes(b'old')
            receipt={'old_commit':self.wire.OLD_COMMIT,'old_tree':self.wire.OLD_TREE,'test_overlay_sha256':self.wire.sha(b'test'),'production_imported':False,'test_overlay_patch_sha256':self.wire.sha(b'patch')}
            answers={('rev-parse','HEAD'):self.wire.OLD_COMMIT.encode(),('rev-parse','HEAD^{tree}'):self.wire.OLD_TREE.encode(),('diff','--name-only','HEAD'):self.wire.TEST.encode(),('diff','--cached','--name-only'):b'',('ls-files','--others','--exclude-standard'):b'',('diff','--submodule=short','--','gimp-data'):b'',('diff','--binary','HEAD','--',self.wire.TEST):b'patch'}
            with mock.patch.object(self.wire,'OLD_FILES',{'production.c':self.wire.sha(b'old')}),mock.patch.object(self.wire,'TEST_SHA',self.wire.sha(b'test')),mock.patch.object(self.wire,'git',side_effect=lambda root,*args:answers[args]):
                self.wire.verify(source,receipt)
                for key,value in ((('diff','--name-only','HEAD'),(self.wire.TEST+'\nproduction.c').encode()),(('ls-files','--others','--exclude-standard'),b'unknown'),(('diff','--cached','--name-only'),b'production.c')):
                    original=answers[key];answers[key]=value
                    with self.assertRaises(ValueError):self.wire.verify(source,receipt)
                    answers[key]=original
                (source/'production.c').write_bytes(b'new')
                with self.assertRaises(ValueError):self.wire.verify(source,receipt)


if __name__ == '__main__':
    unittest.main(verbosity=2)
