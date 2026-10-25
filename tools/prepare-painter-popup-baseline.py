#!/usr/bin/env python3
"""Overlay only the frozen task-307 harness on the exact task-306 baseline.

This never imports candidate production code or revised inherited assertions.
The separate candidate-only overlay-paint-gate source and registration are not
part of this control; only their exact entry-file extension is recognized.
Run before configuring a fresh baseline worktree; retain the report externally.
Use --verify REPORT after compilation/testing to reject any further source drift.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


BASELINE = 'd4ce384f708090facbf6b08edc38b4ddcf84cff0'
TEST_SHA256 = '31ff252f755a9f152cb7f4ea7da400f62fa69d8a3b0ab8f83a9495809a4b05bf'
NEW_TEST = 'app/tests/test-common-layer-popup.cpp'
MAIN = 'app/tests/test-painter-layer-ui.c'
MESON = 'app/tests/meson.build'
ROUTES = 'migration/inventory/additional-c-header-routes.json'
TRACKED = {MAIN, MESON, ROUTES}


def git(repository, *args):
    return subprocess.check_output(['git', '-C', str(repository), *args])


def sha(data):
    return hashlib.sha256(data).hexdigest()


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Baseline registration anchor changed or is ambiguous: ' + old[:80])
    return text.replace(old, new, 1)


def inject_main(text):
    text = replace_once(text,
        'void gimp_test_filter_layer_popup_register (Gimp *application);',
        'void gimp_test_filter_layer_popup_register (Gimp *application);\n'
        'void gimp_test_common_layer_popup_register (Gimp *application);')
    text = replace_once(text,
        '  const gboolean filter_popup=g_getenv("GIMP_TEST_FILTER_LAYER_POPUP")!=NULL;',
        '  const gboolean filter_popup=g_getenv("GIMP_TEST_FILTER_LAYER_POPUP")!=NULL;\n'
        '  const gboolean common_popup=g_getenv("GIMP_TEST_COMMON_LAYER_POPUP")!=NULL;')
    text = replace_once(text,
        '  if(filter_popup){gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);}',
        '  if(filter_popup || common_popup){gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);}')
    text = replace_once(text,
        '  if(filter_popup) gimp_test_filter_layer_popup_register(gimp);',
        '  if(common_popup) gimp_test_common_layer_popup_register(gimp);\n'
        '  else if(filter_popup) gimp_test_filter_layer_popup_register(gimp);')
    for call in ('g_application_run', 'g_application_quit'):
        text = replace_once(text,
            'if(colorify_oncanvas || filter_popup){' + call,
            'if(colorify_oncanvas || filter_popup || common_popup){' + call)
    return text


def inject_meson(text):
    text = replace_once(text,
        "'test-filter-argument-patch.cpp', 'test-filter-layer-popup.cpp'] : [],",
        "'test-filter-argument-patch.cpp', 'test-filter-layer-popup.cpp', 'test-common-layer-popup.cpp'] : [],")
    block = """    test('painter-common-layer-popup', test_exe,
      env: ['GIMP_TEST_COMMON_LAYER_POPUP=1',
            'GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas',
            'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
            'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
            'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
            'UI_TEST=yes'],
      suite: 'app', depends: gimp_test_icon_dependencies,
      timeout: 180, is_parallel: false)
"""
    anchor = "  if test_name == 'painter-layer-ui'\n    test('painter-filter-layer-popup', test_exe,"
    return replace_once(text, anchor,
                        "  if test_name == 'painter-layer-ui'\n" + block +
                        "    test('painter-filter-layer-popup', test_exe,")



def candidate_main(common):
    """Recognize reviewed gate-only registration, never import it to baseline."""
    text = replace_once(common,
        'void gimp_test_common_layer_popup_register (Gimp *application);',
        'void gimp_test_common_layer_popup_register (Gimp *application);\n'
        'void gimp_test_overlay_paint_gate_register (Gimp *application);')
    text = replace_once(text,
        '  const gboolean common_popup=g_getenv("GIMP_TEST_COMMON_LAYER_POPUP")!=NULL;',
        '  const gboolean common_popup=g_getenv("GIMP_TEST_COMMON_LAYER_POPUP")!=NULL;\n'
        '  gboolean overlay_gate=g_getenv("GIMP_TEST_OVERLAY_PAINT_GATE")!=NULL;')
    text = replace_once(text,
        '  if(filter_popup || common_popup){gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);}',
        '  if(filter_popup || common_popup || overlay_gate){gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);}')
    text = replace_once(text,
        '  if(common_popup) gimp_test_common_layer_popup_register(gimp);',
        '  if(overlay_gate) gimp_test_overlay_paint_gate_register(gimp);\n'
        '  else if(common_popup) gimp_test_common_layer_popup_register(gimp);')
    for call in ('g_application_run', 'g_application_quit'):
        text = replace_once(text,
            'if(colorify_oncanvas || filter_popup || common_popup){' + call,
            'if(colorify_oncanvas || filter_popup || common_popup || overlay_gate){' + call)
    return text


def candidate_meson(common):
    """Exact candidate gate source/selector extension; baseline omits both."""
    text = replace_once(common,
        "'test-filter-layer-popup.cpp', 'test-common-layer-popup.cpp'] : [],",
        "'test-filter-layer-popup.cpp', 'test-common-layer-popup.cpp', 'test-overlay-paint-gate.cpp'] : [],")
    block = """    test('painter-overlay-paint-gate', test_exe,
      env: ['GIMP_TEST_OVERLAY_PAINT_GATE=1',
            'GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas',
            'GIMP_TESTING_ABS_TOP_SRCDIR=' + meson.project_source_root(),
            'GIMP_TESTING_ABS_TOP_BUILDDIR=' + meson.project_build_root(),
            'GIMP_TESTING_PLUGINDIRS=' + meson.project_build_root() / 'plug-ins/common',
            'UI_TEST=yes'],
      suite: 'app', depends: gimp_test_icon_dependencies,
      timeout: 180, is_parallel: false)
"""
    anchor = "  if test_name == 'painter-layer-ui'\n    test('painter-common-layer-popup', test_exe,"
    return replace_once(text, anchor,
        "  if test_name == 'painter-layer-ui'\n" + block +
        "    test('painter-common-layer-popup', test_exe,")


def check_routes(before, after):
    if set(before) != {'format', 'routes'} or set(after) != set(before):
        raise ValueError('Unexpected header inventory format')
    if before['format'] != after['format'] or after['routes'][:-2] != before['routes']:
        raise ValueError('Baseline header routes were changed beyond the two prerequisites')
    additions = after['routes'][-2:]
    expected = [
        {'path': 'app/tools/gimppaintertogglepopup.h', 'kind': 'new',
         'context': 'standalone', 'prerequisites': []},
        {'path': 'app/widgets/gimpblobeditor.h', 'kind': 'changed-baseline',
         'baseline_blob': '46a26506f8c69de6f2183d968eb4f153bb701a35',
         'context': 'core', 'prerequisites': []},
    ]
    for actual, wanted in zip(additions, expected):
        if set(actual) != set(wanted) | {'review'} or not actual.get('review'):
            raise ValueError('Missing review or unexpected header prerequisite fields')
        if {k: v for k, v in actual.items() if k != 'review'} != wanted:
            raise ValueError('Wrong header prerequisite route')


def overlay_bytes(baseline, candidate):
    """Return only approved bytes, rejecting extra edits in harness entry files."""
    result = {}
    for path, transform, candidate_transform in (
            (MAIN, inject_main, candidate_main), (MESON, inject_meson, candidate_meson)):
        before = git(baseline, 'show', BASELINE + ':' + path)
        result[path] = transform(before.decode()).encode()
        # Reconstruct baseline entries from306. Candidate-only gate registration
        # is recognized by exact bytes, not stripped permissively or copied.
        expected_candidate = candidate_transform(result[path].decode()).encode()
        if expected_candidate != (candidate / path).read_bytes():
            raise ValueError('Candidate entry has changes beyond reviewed harness registrations: ' + path)
    before = git(baseline, 'show', BASELINE + ':' + ROUTES)
    result[ROUTES] = (candidate / ROUTES).read_bytes()
    check_routes(json.loads(before), json.loads(result[ROUTES]))
    result[NEW_TEST] = (candidate / NEW_TEST).read_bytes()
    if sha(result[NEW_TEST]) != TEST_SHA256:
        raise ValueError('Frozen task-307 test source changed; review/update its expected identity')
    return result


def verify_worktree(baseline, report):
    if git(baseline, 'rev-parse', 'HEAD').decode().strip() != BASELINE:
        raise ValueError('Control is not the exact task-306 baseline commit')
    if report['baseline_commit'] != BASELINE or set(report['files']) != TRACKED | {NEW_TEST}:
        raise ValueError('Wrong baseline overlay report')
    tracked = set(git(baseline, 'diff', '--name-only', 'HEAD', '--').decode().splitlines())
    untracked = set(git(baseline, 'ls-files', '--others', '--exclude-standard').decode().splitlines())
    if tracked != TRACKED or untracked != {NEW_TEST}:
        raise ValueError(f'Unreviewed baseline changes: tracked={sorted(tracked)}, untracked={sorted(untracked)}')
    for path, item in report['files'].items():
        file = baseline / path
        if file.is_symlink() or not file.is_file() or sha(file.read_bytes()) != item['sha256']:
            raise ValueError('Baseline overlay content drift: ' + path)
    return True


def prepare(baseline, candidate, output):
    if git(baseline, 'rev-parse', 'HEAD').decode().strip() != BASELINE:
        raise ValueError('Baseline must be exact task-306 commit ' + BASELINE)
    if git(baseline, 'status', '--porcelain=v1', '--untracked-files=all').strip():
        raise ValueError('Baseline worktree must be clean before harness overlay')
    entries = overlay_bytes(baseline, candidate)
    report = {'baseline_commit': BASELINE,
              'candidate_commit': git(candidate, 'rev-parse', 'HEAD').decode().strip(),
              'scope': 'Baseline production plus two reviewed compile-route prerequisites and frozen task-307 harness only',
              'native_execution': 'not established by preparing this overlay', 'files': {}}
    for path, data in entries.items():
        target = baseline / path
        if target.is_symlink():
            raise ValueError('Refusing to replace symlink: ' + path)
        target.write_bytes(data)
        report['files'][path] = {'sha256': sha(data), 'size_bytes': len(data)}
    verify_worktree(baseline, report)
    # Full public source patch is review evidence, not an imported test result.
    patch = git(baseline, 'diff', '--binary', 'HEAD', '--', *sorted(TRACKED))
    output.with_suffix('.patch').write_bytes(patch)
    report['tracked_patch_sha256'] = sha(patch)
    output.write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--verify', type=Path)
    args = parser.parse_args()
    if args.verify:
        verify_worktree(args.baseline.resolve(), json.loads(args.verify.read_text()))
        print('Baseline production and reviewed harness overlay are unchanged')
    else:
        if not args.candidate or not args.output:
            parser.error('--candidate and --output are required without --verify')
        prepare(args.baseline.resolve(), args.candidate.resolve(), args.output.resolve())
        print('Prepared baseline control; native REDs remain unverified')


if __name__ == '__main__':
    main()
