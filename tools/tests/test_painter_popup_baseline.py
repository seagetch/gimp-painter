#!/usr/bin/env python3
"""Pure negative controls for the task-306 baseline harness overlay."""
from __future__ import annotations
import copy
import importlib.util
import json
from pathlib import Path
import unittest
import tempfile
from unittest import mock

spec = importlib.util.spec_from_file_location('popup_baseline', Path(__file__).parents[1] / 'prepare-painter-popup-baseline.py')
baseline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(baseline)


MAIN = '''void gimp_test_filter_layer_popup_register (Gimp *application);
  const gboolean filter_popup=g_getenv("GIMP_TEST_FILTER_LAYER_POPUP")!=NULL;
  if(filter_popup){gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);}
  if(filter_popup) gimp_test_filter_layer_popup_register(gimp);
  inherited_assertions_stay_identical ();
  if(colorify_oncanvas || filter_popup){g_application_run(gimp->app,0,NULL);}
  if(colorify_oncanvas || filter_popup){g_application_quit(G_APPLICATION(gimp->app));}
'''
MESON = '''test_name == 'painter-layer-ui' ? ['test-filter-editor-metadata.cpp', 'test-filter-argument-patch.cpp', 'test-filter-layer-popup.cpp'] : [],
  if test_name == 'painter-layer-ui'
    # All 63 cases took 83.69s under Xvfb; keep the full suite within its budget.
    test_timeout = 180
  endif
  if test_name == 'painter-layer-ui'
    test('painter-filter-layer-popup', test_exe,
      env: ['GIMP_TEST_FILTER_LAYER_POPUP=1'], suite: 'app')
  endif
'''
ADDITIONS = [
    {'path': 'app/tools/gimppaintertogglepopup.h', 'kind': 'new',
     'context': 'standalone', 'prerequisites': [], 'review': 'Reviewed C linkage'},
    {'path': 'app/widgets/gimpblobeditor.h', 'kind': 'changed-baseline',
     'baseline_blob': '46a26506f8c69de6f2183d968eb4f153bb701a35',
     'context': 'core', 'prerequisites': [], 'review': 'Reviewed extension'},
]


class BaselineOverlayTests(unittest.TestCase):
    def test_main_injection_preserves_inherited_assertions(self):
        changed = baseline.inject_main(MAIN)
        self.assertIn('  inherited_assertions_stay_identical ();', changed)
        self.assertIn('if(common_popup) gimp_test_common_layer_popup_register(gimp);', changed)
        self.assertEqual(changed.count('void gimp_test_common_layer_popup_register'), 1)

    def test_main_rejects_already_injected_or_missing_anchor(self):
        with self.assertRaises(ValueError):
            baseline.inject_main(baseline.inject_main(MAIN))
        with self.assertRaises(ValueError):
            baseline.inject_main(MAIN.replace('g_application_quit', 'different_function'))

    def test_meson_injection_preserves_inherited_registration(self):
        changed = baseline.inject_meson(MESON)
        self.assertIn("    test_timeout = 180\n", changed)
        self.assertIn("test('painter-filter-layer-popup', test_exe,", changed)
        self.assertEqual(changed.count("test('painter-common-layer-popup'"), 1)

    def test_meson_rejects_ambiguous_anchor(self):
        with self.assertRaises(ValueError):
            baseline.inject_meson(MESON + MESON)

    def test_candidate_main_gate_registration_is_exact_and_not_baseline(self):
        common = baseline.inject_main(MAIN)
        changed = baseline.candidate_main(common)
        self.assertNotIn('overlay_gate', common)
        self.assertNotIn('overlay_paint_gate', common)
        self.assertIn('  inherited_assertions_stay_identical ();', changed)
        self.assertEqual(changed.count('void gimp_test_overlay_paint_gate_register'), 1)
        self.assertIn('if(overlay_gate) gimp_test_overlay_paint_gate_register(gimp);', changed)
        for call in ('g_application_run', 'g_application_quit'):
            self.assertIn('common_popup || overlay_gate){' + call, changed)

    def test_candidate_main_rejects_duplicate_or_missing_gate_anchor(self):
        common = baseline.inject_main(MAIN)
        with self.assertRaises(ValueError):
            baseline.candidate_main(baseline.candidate_main(common))
        with self.assertRaises(ValueError):
            baseline.candidate_main(common.replace('g_application_quit', 'wrong_call'))

    def test_candidate_meson_keeps_gate_out_of_baseline(self):
        common = baseline.inject_meson(MESON)
        changed = baseline.candidate_meson(common)
        self.assertNotIn('overlay-paint-gate', common)
        self.assertNotIn('OVERLAY_PAINT_GATE', common)
        self.assertEqual(changed.count("test('painter-overlay-paint-gate'"), 1)
        self.assertEqual(changed.count("'test-overlay-paint-gate.cpp'"), 1)
        self.assertIn("test('painter-common-layer-popup'", changed)
        self.assertIn("test('painter-filter-layer-popup'", changed)

    def test_candidate_meson_rejects_duplicate_or_missing_gate_anchor(self):
        common = baseline.inject_meson(MESON)
        with self.assertRaises(ValueError):
            baseline.candidate_meson(common + common)
        with self.assertRaises(ValueError):
            baseline.candidate_meson(common.replace("test('painter-common-layer-popup'", "test('changed-common-popup'"))

    def run_overlay_fixture(self, mutate=None, test_bytes=b'frozen neutral common TU'):
        original = {
            baseline.MAIN: MAIN.encode(), baseline.MESON: MESON.encode(),
            baseline.ROUTES: json.dumps({'format': 1, 'routes': []}).encode(),
        }
        def git_show(repository, *args):
            self.assertEqual(args[0], 'show')
            self.assertTrue(args[1].startswith(baseline.BASELINE + ':'))
            return original[args[1].split(':', 1)[1]]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = {
                baseline.MAIN: baseline.candidate_main(baseline.inject_main(MAIN)).encode(),
                baseline.MESON: baseline.candidate_meson(baseline.inject_meson(MESON)).encode(),
                baseline.ROUTES: json.dumps({'format': 1, 'routes': ADDITIONS}).encode(),
                baseline.NEW_TEST: test_bytes,
            }
            if mutate:
                mutate(candidate)
            for path, data in candidate.items():
                file = root / path
                file.parent.mkdir(parents=True, exist_ok=True)
                file.write_bytes(data)
            with mock.patch.object(baseline, 'git', side_effect=git_show), \
                 mock.patch.object(baseline, 'TEST_SHA256', baseline.sha(test_bytes)):
                return baseline.overlay_bytes(root, root)

    def test_overlay_exact_allowlist_excludes_gate_tu_calls_and_production(self):
        result = self.run_overlay_fixture()
        self.assertEqual(set(result), {baseline.MAIN, baseline.MESON, baseline.ROUTES, baseline.NEW_TEST})
        self.assertEqual(result[baseline.MAIN], baseline.inject_main(MAIN).encode())
        self.assertEqual(result[baseline.MESON], baseline.inject_meson(MESON).encode())
        for path in (baseline.MAIN, baseline.MESON):
            self.assertNotIn(b'overlay_gate', result[path])
            self.assertNotIn(b'overlay_paint_gate', result[path])
            self.assertNotIn(b'overlay-paint-gate', result[path])
            self.assertNotIn(b'OVERLAY_PAINT_GATE', result[path])
        self.assertEqual(baseline.TRACKED, {baseline.MAIN, baseline.MESON, baseline.ROUTES})

    def test_overlay_rejects_unreviewed_main_or_meson_edits(self):
        for path in (baseline.MAIN, baseline.MESON):
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.run_overlay_fixture(lambda files: files.__setitem__(path, files[path] + b'\nunreviewed();\n'))

    def test_overlay_rejects_missing_candidate_gate_registration(self):
        with self.assertRaises(ValueError):
            self.run_overlay_fixture(lambda files: files.__setitem__(baseline.MAIN, baseline.inject_main(MAIN).encode()))

    def test_overlay_rejects_mutated_common_tu(self):
        with self.assertRaisesRegex(ValueError, 'Frozen task-307 test source changed'):
            self.run_overlay_fixture(lambda files: files.__setitem__(baseline.NEW_TEST, b'changed common TU'))

    def test_exact_prerequisites_allowed(self):
        before = {'format': 1, 'routes': [{'path': 'inherited.h'}]}
        after = {'format': 1, 'routes': before['routes'] + copy.deepcopy(ADDITIONS)}
        baseline.check_routes(before, after)

    def test_prerequisites_reject_inherited_changes(self):
        before = {'format': 1, 'routes': [{'path': 'inherited.h'}]}
        after = {'format': 1, 'routes': [{'path': 'modified.h'}] + copy.deepcopy(ADDITIONS)}
        with self.assertRaises(ValueError):
            baseline.check_routes(before, after)

    def test_prerequisites_reject_wrong_historical_blob(self):
        before = {'format': 1, 'routes': []}
        after = {'format': 1, 'routes': copy.deepcopy(ADDITIONS)}
        after['routes'][1]['baseline_blob'] = '0' * 40
        with self.assertRaises(ValueError):
            baseline.check_routes(before, after)

    def test_prerequisites_require_review_and_no_extra_routes(self):
        before = {'format': 1, 'routes': []}
        after = {'format': 1, 'routes': copy.deepcopy(ADDITIONS)}
        del after['routes'][0]['review']
        with self.assertRaises(ValueError):
            baseline.check_routes(before, after)
        after = {'format': 1, 'routes': copy.deepcopy(ADDITIONS) + [{'path': 'extra.h'}]}
        with self.assertRaises(ValueError):
            baseline.check_routes(before, after)


if __name__ == '__main__':
    unittest.main()
