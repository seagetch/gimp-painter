#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run exact Value Invert native/XCF paths against a completed ordinary build.

Invoke inside authenticated Xvfb, verified owner-fixture wrapper and a locking
shell. Default coverage includes fresh genuine captures. --synthetic-only is an
explicit incomplete diagnostic gate and never claims genuine acceptance.
No source/build mutation or build setup is performed by this runner.
"""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

from run_sharpen_context_sanitizers import (build_library_path, reseal, seals,
                                           tap_result, verify_lock)
ROOT = Path(__file__).resolve().parents[2]
NATIVE = ('integer_native_route', 'invalid_definitions_keep_cache',
          'gray_unsupported', 'tiny_selection_routes', 'late_context_routes',
          'component_masks', 'sealed_context_routes', 'outside_selection_no_merge',
          'cancel_replace_lower_close', 'partial_import_lifecycle',
          'native_profile_bytes', 'admission_preserves_cache',
          'real_routes_unchanged', 'projection_input_bounds')
XCF = ('synthetic_capsule_roundtrip', 'malformed_capsules', 'rejected_types_roundtrip', 'active_save_close_reopen')
FIXTURES = ('GIMP_TEST_VALUE_INVERT_FIXTURE_DIR',
            'GIMP_TEST_VALUE_INVERT_PROJECTION_FIXTURE_DIR')


def selected_paths(synthetic_only=False):
    names = {'gimp-filter-layer': NATIVE, 'painter-xcf-open': XCF}
    if not synthetic_only:
        names['gimp-filter-layer'] += ('actual_old_live', 'actual_old_projection_history')
        names['painter-xcf-open'] += ('old_writer_roundtrip',)
    return [(target, '/' + target + '/value_invert_' + name)
            for target, cases in names.items() for name in cases]


def passed_result(exit_code, stdout, stderr, path):
    tap = tap_result(stdout, path)
    diagnostics = bool(re.search(r'CRITICAL|WARNING|AddressSanitizer|runtime error:|Bail out!|^not ok',
                                 stdout + '\n' + stderr, re.M))
    return exit_code == 0 and tap['exact_positive_result'] and not diagnostics, tap


def fixture_snapshot(root):
    root = Path(root)
    if not root.is_absolute() or root.is_symlink() or not root.is_dir():
        raise RuntimeError('Fixture root must be an absolute regular directory')
    paths = list(root.rglob('*'))
    if any(p.is_symlink() or not (p.is_file() or p.is_dir()) for p in paths):
        raise RuntimeError('Fixture tree contains a link or special file')
    result = seals(p for p in paths if p.is_file())
    if not result:
        raise RuntimeError('Fixture tree is empty')
    return result


def verify_fixture_roots(environment):
    # Re-materialize hash-pinned artifacts rather than trusting an inherited
    # environment variable or nonempty directory. Keep exact matching live
    # fixture seals for the after-run comparison. No fixture content is invented.
    sys.path.insert(0, str(ROOT/'tools'))
    from check_value_invert_evidence import verify as verify_base
    from check_value_invert_projection_evidence import verify as verify_history
    summaries, before = {}, {}
    with tempfile.TemporaryDirectory(prefix='value-invert-normal-verify-') as temporary:
        for index, (variable, verify) in enumerate(zip(FIXTURES, (verify_base, verify_history))):
            if not environment.get(variable):
                raise RuntimeError('Missing verified fixture input: '+variable)
            actual_root = Path(environment[variable])
            canonical_root = Path(temporary)/str(index)
            summaries[variable] = verify(extract=canonical_root)
            actual = fixture_snapshot(actual_root)
            canonical = fixture_snapshot(canonical_root)
            relative = lambda values, root: {str(Path(name).relative_to(root)): value for name, value in values.items()}
            if relative(actual, actual_root) != relative(canonical, canonical_root):
                raise RuntimeError('Live fixture tree differs from sealed corpus: '+variable)
            before.update(actual)
    return summaries, before


def execute(command, build, environment, timeout):
    start = time.monotonic()
    process = subprocess.Popen(command, cwd=build, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, start_new_session=True)
    status = 'finished'
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        stdout, stderr = process.communicate()
        status = 'timeout'
    return dict(stdout=stdout, stderr=stderr, exit_code=process.returncode,
                seconds=time.monotonic()-start, process_status=status)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--lock-fd', type=int, required=True)
    parser.add_argument('--lock-file', type=Path,
                        default=Path('/workspace/shared/gimp-painter-build.lock'))
    parser.add_argument('--synthetic-only', action='store_true')
    parser.add_argument('--test-timeout', type=int, default=300)
    args = parser.parse_args(argv)
    args.build, args.report = args.build.resolve(), args.report.resolve()
    if args.report.exists() or args.report.suffix != '.json':
        parser.error('Use a fresh dedicated .json report path')
    if not 1 <= args.test_timeout <= 3600:
        parser.error('Test timeout must be 1 through 3600 seconds')
    selected = selected_paths(args.synthetic_only)
    report = dict(report_format='value-invert-context-normal-v1', status='running',
                  source=str(ROOT), build=str(args.build),
                  scope='Explicit bounded native owner/XCF paths; no full aggregate or installed UI claim',
                  synthetic_only=args.synthetic_only, genuine_acceptance=False,
                  tests=[dict(target=t, path=p, process_status='not_run', passed=False)
                         for t, p in selected])
    before = {}
    def save():
        args.report.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.report.with_suffix('.tmp')
        temporary.write_text(json.dumps(report, indent=2)+'\n')
        temporary.replace(args.report)
    save()
    try:
        report['lock'] = verify_lock(args.lock_fd, args.lock_file)
        environment = dict(os.environ)
        environment['LD_LIBRARY_PATH'] = build_library_path(args.build, environment.get('LD_LIBRARY_PATH', ''))
        registry = json.loads((args.build/'meson-info/intro-tests.json').read_text())
        records = {t: [row for row in registry if row.get('name') == t] for t, _ in selected}
        if any(len(rows) != 1 for rows in records.values()):
            raise RuntimeError('Missing or ambiguous native registry identity')
        names = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others',
                                         '--exclude-standard'], cwd=ROOT).decode().split('\0')
        before['source'] = seals(ROOT/name for name in names if name and (ROOT/name).is_file())
        artifacts = {args.build/'compile_commands.json', args.build/'build.ninja',
                     args.build/'meson-info/intro-tests.json'}
        for target in records:
            artifacts.add(args.build/'app/tests'/target)
        # Include build-local shared libraries, not just the executable that loads
        # them. This is a focused runtime seal, not every dependency or object.
        artifacts.update(p.resolve() for p in args.build.rglob('*.so*') if p.is_file())
        before['build'] = seals(artifacts)
        before['fixtures'] = {}
        if not args.synthetic_only:
            report['fixture_verification'], before['fixtures'] = verify_fixture_roots(environment)
        report['before'] = before
        save()
        for row in report['tests']:
            verify_lock(args.lock_fd, args.lock_file)
            env = environment.copy()
            env.update({k: v for k, v in records[row['target']][0]['env'].items()
                        if k != 'LD_LIBRARY_PATH'})
            command = [str(args.build/'app/tests'/row['target']), '--tap', '-p', row['path']]
            row.update(command=command, process_status='running'); save()
            row.update(execute(command, args.build, env, args.test_timeout))
            row['passed'], row['tap'] = passed_result(row['exit_code'], row['stdout'], row['stderr'], row['path'])
            row['passed'] &= row['process_status'] == 'finished'
            save(); print(('PASS ' if row['passed'] else 'FAIL ')+row['path'], flush=True)
        report['status'] = 'passed' if all(r['passed'] for r in report['tests']) else 'failed'
    except BaseException as error:
        report.update(status='failed', error=str(error), exception=type(error).__name__)
        print(str(error), file=sys.stderr)
    finally:
        report['seals'] = {name: reseal(values) for name, values in before.items()}
        if any(item['changed'] or item['read_errors'] for item in report['seals'].values()):
            report['status'] = 'failed'
        report['genuine_acceptance'] = report['status'] == 'passed' and not args.synthetic_only and bool(report.get('fixture_verification'))
        save()
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
