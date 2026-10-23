#!/usr/bin/env python3
"""Run bounded Phase E native selections in an actual GTK display namespace.

Hold the shared build/test lock. These linked-native tests are in-build tests;
the separate runtime runner and actual-app walkthrough establish relocation.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
UI = [
    'filter_create_cancel_and_validate', 'filter_progress_widgets',
    'filter_progress_running_cancel', 'filter_progress_render_reentry',
    'filter_response_close_reentry', 'filter_creation_close_reentry',
    'isolated_editor_create_reedit', 'isolated_editor_preserve_and_undo',
    'isolated_editor_stale_definition', 'isolated_editor_saved_enums',
    'isolated_editor_every_field', 'isolated_editor_invalid_input',
    'isolated_editor_precision_ranges', 'isolated_editor_unknown_shapes',
    'isolated_editor_lifetimes', 'isolated_editor_reentry',
    'isolated_editor_running_cache', 'isolated_editor_save_reopen',
    'filter_schema_registry_invalidation', 'filter_schema_unavailable_creation',
    'filter_schema_incompatible_metadata', 'filter_schema_provider_identity',
    'filter_schema_reordered_keys', 'filter_schema_bounded_preview',
    'filter_schema_materialization_budget', 'filter_schema_untouched_nonfinite',
    'filter_schema_blinds_context_types', 'filter_schema_reference_tail_provenance',
    'filter_schema_metadata_snapshot', 'filter_schema_manager_lifetime',
    'filter_schema_argument_patch', 'filter_schema_undo_provider_reentry',
    'filter_schema_persistence_routes', 'filter_schema_persistence_special_bits',
    'filter_schema_persistence_large_tails', 'filter_schema_persistence_reference_identity',
    'filter_schema_persistence_failures', 'filter_schema_persistence_nested_null',
    'filter_schema_persistence_unknown_opaque', 'filter_schema_persistence_active_save',
    'filter_schema_persistence_reference_lease', 'filter_schema_persistence_double_cache',
]
CORE = [
    'progress_real_workers', 'progress_worker_cancel_replace_close',
    'convolution_native_failure_keeps_cache', 'convolution_cancel_replace_close',
    'small_tiles_replace_running_definition', 'retinex_replace_running_definition',
    'blinds_replace_running_definition', 'explicit_cancel_preserves_completed_cache',
    'definition_edit_stops_after_undo_close', 'definition_edit_preserves_reentered_install',
    'definition_notifications_stop_after_close', 'retired_definition_payload_reentry',
    'definition_revision_separates_cache_updates', 'definition_undo_redo',
    'gaussian_alias_and_negative_fixtures', 'genuine_point_filter_transfer',
    'native_precision_point_and_gaussian_transfer', 'saved_definition_is_separate',
    'checked_guard_resize_reprepares_request',
    'checked_guard_precision_reprepares_request',
    'checked_guard_nested_loop_preserves_wakeup',
]
XCF = [
    '/painter-xcf-roundtrip/filter_cache_and_arguments',
    '/painter-xcf-roundtrip/typed_nested_expired_arguments',
    '/painter-xcf-roundtrip/replace_uninterpreted_arguments',
    '/painter-xcf-roundtrip/external_references_remain_unresolved',
    '/painter-xcf-multipart/native-owner-duplicate-edit',
    '/painter-xcf-multipart/native-callback-argument-leases',
]
SOURCES = [
    'app/core/gimpfilterlayer.cpp', 'app/core/gimpfilterlayer.h',
    'app/core/gimpfilterlayer-arguments.hpp', 'app/core/gimpfilterparametereditor.cpp',
    'app/core/gimpfilterprocedure-arguments.cpp', 'app/dialogs/painter-layer-dialog.cpp',
    'app/xcf/painter-xcf-arguments.cpp', 'app/tests/test-painter-layer-ui.c',
    'app/tests/test-filter-editor-metadata.cpp', 'app/tests/test-filter-argument-patch.cpp',
    'app/tests/test-filter-schema-editors.inc', 'app/tests/test-isolated-filter-editors.inc',
    'app/tests/test-filter-schema-persistence.inc', 'app/tests/test-filter-schema-persistence-failures.inc',
    'app/tests/test-filter-allocation-failures.cpp',
    'migration/tests/run_filter_acceptance_native.py',
]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--target', choices=['ui', 'core', 'xcf', 'faults'], required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--case', action='append', default=[])
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    if args.sanitize and args.target not in ('ui', 'faults') and not args.binary:
        parser.error('A separate instrumented binary must be named for this target')
    name = {'ui': 'painter-layer-ui', 'core': 'gimp-filter-layer',
            'xcf': 'painter-xcf-roundtrip', 'faults': 'painter-layer-ui'}[args.target]
    if args.case:
        selected = [case if case.startswith('/') else '/'+name+'/'+case for case in args.case]
    elif args.target == 'faults':
        selected = ['/painter-layer-ui/filter_fault_descriptors',
                    '/painter-layer-ui/filter_fault_gtk_changed_array'] + [
            '/painter-layer-ui/filter_fault_route_'+route for route in
            ('blinds', 'small_tiles', 'retinex', 'convolution')] + [
            '/painter-layer-ui/filter_fault_patch_'+family+'_'+kind
            for family in ('cpp', 'try') for kind in
            ('scalar', 'string', 'strv', 'double_array', 'int32_array', 'raw_array')]
    else:
        selected = XCF if args.target == 'xcf' else ['/'+name+'/'+case for case in (UI if args.target == 'ui' else CORE)]
    registry = json.loads((build/'meson-info/intro-tests.json').read_text())
    test = next(item for item in registry if item['name'] == name)
    env = os.environ.copy()
    previous = env.get('LD_LIBRARY_PATH', '')
    env.update(test.get('env', {}))
    if 'LD_LIBRARY_PATH' in test.get('env', {}):
        env['LD_LIBRARY_PATH'] += ':'+previous
    env.update(GIMP3_CACHEDIR=str(build/'filter-spill-cache'), GSETTINGS_BACKEND='memory')
    if args.sanitize:
        env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    binary = args.binary.resolve() if args.binary else build/'app/tests'/(name+('-asan' if args.sanitize else ''))
    if args.target == 'faults' and not args.binary:
        parser.error('Private fault executable required via --binary')
    command = ['dbus-run-session', '--']
    if args.target == 'core':
        command += ['python3', '-B', str(ROOT/'migration/tests/run_filter_owner_context_fixture_test.py'), '--']
    command += [str(binary), '--verbose']
    for case in selected:
        command += ['-p', case]
    source_names = SOURCES + ({'core': ['app/tests/test-gimp-filter-layer.c'],
                              'xcf': ['app/tests/test-painter-xcf-roundtrip.c']}.get(args.target, []))
    before = {name: sha(ROOT/name) for name in source_names if (ROOT/name).is_file()}
    binary_hash = sha(binary)
    start = time.monotonic()
    completed = subprocess.run(command, cwd=build, env=env, capture_output=True, text=True, timeout=600)
    elapsed = time.monotonic()-start
    log = out/'native.log'
    log.write_text(completed.stdout+completed.stderr)
    actual = re.findall(r'^ok \d+ (\S+)$', completed.stdout, re.M)
    skipped = re.findall(r'^ok .*#\s*SKIP.*$', completed.stdout, re.M)
    diagnostics = [line for line in log.read_text().splitlines() if
                   any(marker in line for marker in ('ERROR: AddressSanitizer', 'runtime error:', 'AddressSanitizer:DEADLYSIGNAL'))]
    after = {name: sha(ROOT/name) for name in before}
    receipt = dict(scope='Bounded in-build native GTK acceptance; not relocated app or whole-port coverage',
                   command=command, exit_code=completed.returncode, selected=selected, executed=actual,
                   skipped=skipped, sanitizer_diagnostics=diagnostics, elapsed_seconds=elapsed,
                   max_rss_kib=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
                   binary_sha256=binary_hash, binary_unchanged=sha(binary) == binary_hash,
                   source_sha256=before, sources_changed_during_run=[name for name in before if before[name] != after[name]],
                   log_sha256=sha(log), pass_all=completed.returncode == 0 and sorted(actual) == sorted(selected)
                   and not skipped and not diagnostics and before == after and sha(binary) == binary_hash)
    (out/'report.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(json.dumps(receipt, indent=2))
    return 0 if receipt['pass_all'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
