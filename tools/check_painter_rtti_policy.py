#!/usr/bin/env python3
"""Bounded original 04.011 verification against real, existing Meson outputs.

Inventory all production app C++ commands. Recompile every core/display C++
unit with a final -fno-rtti into NEW isolated objects; never invoke Ninja or
source-writing generators. Run small language/common-bridge controls. Reuse
04.010 foundation runtime evidence only after verifying its exact identities.
Native GNU-style compilation only; production flags are never changed.
"""
import argparse
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
import time
import unittest

from check_painter_exception_flags import (CPP_SUFFIXES, arguments, digest,
                                          production_target, replay_arguments,
                                          source_path, target_for)

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / 'migration/tests/rtti-policy'
CORE_TARGETS = {'appcore', 'appdisplay'}
RTTI_FLAGS = {'-frtti', '-fno-rtti', '/GR', '/GR-'}


def rtti_flags(command):
    return [arg for arg in command if arg in RTTI_FLAGS]


def rtti_disabled(command):
    flags = rtti_flags(command)
    return bool(flags and flags[-1] in ('-fno-rtti', '/GR-'))


def safe_arguments(entry):
    """Accept the actual native compile form; reject extra side-effect modes."""
    command = arguments(entry)
    if any(arg.startswith(('@', '-save-temps', '-fprofile', '-fdump'))
           or arg in ('--coverage', '-ftest-coverage', '-fstack-usage') for arg in command):
        raise ValueError('Unsupported response file or side-effect compiler mode')
    if command.count(entry['file']) != 1 or '-c' not in command or '-o' not in command:
        raise ValueError('Expected one source and an explicit compile/output action')
    # The shared helper preserves all ordinary flags and their order, removing
    # only the original source, output and dependency-output actions.
    return replay_arguments(entry)


def run(command, cwd, log, env=None):
    try:
        result = subprocess.run(command, cwd=cwd, text=True, capture_output=True, timeout=300, env=env)
        output, returncode = result.stdout + result.stderr, result.returncode
    except subprocess.TimeoutExpired as error:
        output = 'Compiler/runtime command exceeded 300 seconds.\n'
        for stream in (error.stdout, error.stderr):
            if stream:
                output += stream.decode(errors='replace') if isinstance(stream, bytes) else stream
        returncode = 124
    log.write_text(output)
    return {'command': command, 'directory': str(cwd), 'returncode': returncode,
            'log': str(log), 'log_sha256': digest(log)}


def succeeded(result):
    return result.get('returncode') == 0


def reuse_foundation(args, entries, targets):
    """Do not rerun 39 cases: establish that their source/flags/binary still match."""
    prior = args.prior_evidence_dir
    report_path = prior / 'repository/migration/tests/exception-policy/foundation-native.json'
    report = json.loads(report_path.read_text())
    result = next(item for item in report['results'] if item['configuration'] == args.configuration)
    old_database = prior / 'current-inputs' / args.configuration / 'compile_commands.json'
    source_manifest = prior / 'native/source-hashes-before-build.json'
    sources = json.loads(source_manifest.read_text())
    checks = {'prior_status_pass': report['status'] == 'PASS',
              'compile_database_identical': digest(old_database) == digest(args.metadata_dir / 'compile_commands.json')}
    verified_sources = {name: sha for name, sha in sources.items() if name.startswith('app/painter/')}
    checks['common_source_tree_identical'] = bool(verified_sources) and all(
        digest(ROOT / name) == sha for name, sha in verified_sources.items())
    binary = args.metadata_dir / 'app/painter/painter-foundation'
    checks['binary_identical'] = digest(binary) == result['binary_sha256']
    for key, filename in [('c_object_sha256', 'tests_test-c-api.c.o'),
                          ('cpp_object_sha256', 'tests_test-foundation.cpp.o')]:
        obj = args.metadata_dir / 'app/painter/painter-foundation.p' / filename
        checks[key + '_identical'] = digest(obj) == result[key]
    log = prior / 'repository/migration/tests/exception-policy' / (args.configuration + '-foundation.log')
    checks['runtime_log_identical'] = digest(log) == result['stdout_sha256']
    common_commands = [entry for entry in entries
                       if target_for(entry, targets)['name'] in ('apppainter', 'painter-foundation')]
    cpp_commands = [entry for entry in common_commands if source_path(entry).suffix in CPP_SUFFIXES]
    checks['all_common_cpp_commands_disable_rtti'] = bool(cpp_commands) and all(
        rtti_disabled(arguments(entry)) for entry in cpp_commands)
    checks['prior_returncode_and_case_count'] = result['returncode'] == 0 and result['test_cases'] == 39
    return {'status': 'PASS' if all(checks.values()) else 'FAIL',
            'runtime_rerun': False, 'checks': checks, 'prior_result': result,
            'prior_report': str(report_path), 'prior_report_sha256': digest(report_path),
            'prior_database_sha256': digest(old_database),
            'prior_source_manifest_sha256': digest(source_manifest),
            'common_source_sha256': verified_sources,
            'common_cpp_commands': len(cpp_commands),
            'common_compile_vectors': [arguments(entry) for entry in common_commands]}


def fresh_foundation(args, entries, targets):
    """Use only after a genuine rebuild; a new binary cannot reuse old results."""
    common_commands = [entry for entry in entries
                       if target_for(entry, targets)['name'] in ('apppainter', 'painter-foundation')]
    cpp_commands = [entry for entry in common_commands if source_path(entry).suffix in CPP_SUFFIXES]
    binary = args.metadata_dir / 'app/painter/painter-foundation'
    env = dict(os.environ, G_DEBUG='fatal-warnings')
    result = run([str(binary), '--tap'], args.metadata_dir,
                 args.work_dir / 'foundation.log', env=env)
    output = Path(result['log']).read_text()
    tests = [line for line in output.splitlines() if line.startswith('ok ')]
    checks = {'runtime_passed': succeeded(result), 'all_39_cases_passed': len(tests) == 39,
              'all_common_cpp_commands_disable_rtti': bool(cpp_commands) and all(
                  rtti_disabled(arguments(entry)) for entry in cpp_commands)}
    return {'status': 'PASS' if all(checks.values()) else 'FAIL', 'runtime_rerun': True,
            'checks': checks, 'runtime': result, 'binary_sha256': digest(binary),
            'test_cases': len(tests), 'common_cpp_commands': len(cpp_commands),
            'common_compile_vectors': [arguments(entry) for entry in common_commands],
            'common_source_sha256': {str(path.relative_to(ROOT)): digest(path)
                for path in sorted((ROOT / 'app/painter').rglob('*')) if path.is_file()
                and path.suffix in ('.cpp', '.cc', '.c', '.h', '.hpp')},
            'reason': 'The original native build was lost; this is a rebuilt binary, not reused 04.010 evidence.'}


def controls(args, template, common_archive):
    scratch = args.work_dir / 'controls'
    scratch.mkdir()
    base = safe_arguments(template)
    results = []
    # Separate expressions are necessary: a downcast and polymorphic typeid
    # each must fail for RTTI reasons, while an upcast may compile without RTTI.
    for name in ('dynamic-cast', 'typeid', 'virtual-dispatch', 'virtual-base-upcast'):
        source = FIXTURES / (name + '.cpp')
        case = {'name': name, 'source_sha256': digest(source), 'runs': []}
        for mode in ('disabled', 'enabled'):
            executable = scratch / (name + '-' + mode)
            flag = '-fno-rtti' if mode == 'disabled' else '-frtti'
            result = run(base + [flag, str(source), '-o', str(executable), '-fdiagnostics-color=never'],
                         template['directory'], scratch / (executable.name + '.log'))
            result['mode'] = mode
            requires_rtti = name in ('dynamic-cast', 'typeid')
            if mode == 'disabled' and requires_rtti:
                diagnostic = Path(result['log']).read_text()
                result['expected_rejection'] = not succeeded(result) and 'rtti' in diagnostic.lower() and (
                    ('dynamic_cast' in diagnostic) if name == 'dynamic-cast' else ('typeid' in diagnostic))
                result['passed'] = result['expected_rejection']
            else:
                if succeeded(result):
                    result['runtime'] = run([str(executable)], scratch, scratch / (executable.name + '-run.log'))
                result['passed'] = succeeded(result) and succeeded(result.get('runtime', {}))
            case['runs'].append(result)
        results.append(case)
    # Link the production archive, not a reimplementation of the typed store.
    source = FIXTURES / 'typed-bridge.cpp'
    obj = scratch / 'typed-bridge.o'
    executable = scratch / 'typed-bridge'
    result = run(base + ['-fno-rtti', '-c', str(source), '-o', str(obj), '-fdiagnostics-color=never'],
                 template['directory'], scratch / 'typed-bridge-compile.log')
    libs = subprocess.run(['pkg-config', '--libs', 'gobject-2.0'], check=True,
                          capture_output=True, text=True).stdout
    if succeeded(result):
        result['link'] = run([base[0], '-fno-rtti', str(obj), str(common_archive), '-pthread'] + shlex.split(libs)
                             + ['-o', str(executable)], template['directory'], scratch / 'typed-bridge-link.log')
        if succeeded(result['link']):
            result['runtime'] = run([str(executable)], scratch, scratch / 'typed-bridge-run.log')
    result['passed'] = succeeded(result) and succeeded(result.get('link', {})) and succeeded(result.get('runtime', {}))
    results.append({'name': 'typed-bridge', 'source_sha256': digest(source),
                    'archive_sha256': digest(common_archive), 'runs': [result]})
    return {'status': 'PASS' if all(run['passed'] for case in results for run in case['runs']) else 'FAIL',
            'cases': results}


def check(args):
    args.metadata_dir = args.metadata_dir.resolve()
    args.work_dir = args.work_dir.resolve()
    if args.work_dir.is_relative_to(args.metadata_dir) or args.metadata_dir.is_relative_to(args.work_dir):
        raise ValueError('Work directory must be separate from the native build directory')
    # Require a new work directory, preventing overwrite of a past result or
    # an accidental output alias to an original production object.
    args.work_dir.mkdir(parents=True, exist_ok=False)
    database = args.metadata_dir / 'compile_commands.json'
    targets_file = args.metadata_dir / 'meson-info/intro-targets.json'
    compiler_file = args.metadata_dir / 'meson-info/intro-compilers.json'
    targets = json.loads(targets_file.read_text())
    compiler = json.loads(compiler_file.read_text())['host']['cpp']
    if compiler['id'] not in ('gcc', 'clang', 'apple-clang'):
        raise ValueError('Only native GNU-style drivers are supported')
    entries = json.loads(database.read_text())
    report = {'task': 'original 04.011 bounded native RTTI independence', 'status': 'RUNNING',
              'configuration': args.configuration,
              'recorded_at_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'compiler': compiler,
              'inputs': {str(path): digest(path) for path in (database, targets_file, compiler_file,
                  Path(__file__), ROOT / 'tools/check_painter_exception_flags.py')},
              'production_units': [], 'excluded_targets': {}, 'replays': []}
    replay = []
    excluded = Counter()
    for entry in entries:
        source = source_path(entry)
        if source.suffix not in CPP_SUFFIXES:
            continue
        if not source.is_relative_to(ROOT / 'app') and not source.is_relative_to(args.metadata_dir / 'app'):
            continue
        target = target_for(entry, targets)
        if not production_target(target):
            excluded[target['name']] += 1
            continue
        do_replay = target['name'] in CORE_TARGETS
        if do_replay:
            replay.append(entry)
        report['production_units'].append({
            'source': str(source.relative_to(ROOT)), 'source_sha256': digest(source),
            'target': target['name'], 'original_output': entry['output'],
            'command': arguments(entry), 'rtti_flags': rtti_flags(arguments(entry)),
            'painter_cpp': source.suffix == '.cpp', 'replayed': do_replay})
    report['excluded_targets'] = dict(sorted(excluded.items()))
    declared_sources = {str(Path(source).resolve()) for target in targets if target['name'] in CORE_TARGETS
                        for group in target['target_sources'] for source in group.get('sources', [])
                        if Path(source).suffix in CPP_SUFFIXES}
    report['core_display_scope_complete'] = declared_sources == {str(source_path(entry)) for entry in replay}
    original_objects = {str((Path(entry['directory']) / entry['output']).resolve()):
                        digest(Path(entry['directory']) / entry['output'])
                        if (Path(entry['directory']) / entry['output']).exists() else None for entry in replay}
    report['original_object_sha256_before'] = original_objects
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    report['foundation'] = fresh_foundation(args, entries, targets) if args.run_foundation else reuse_foundation(args, entries, targets)
    common = next(target for target in targets if target['name'] == 'apppainter')
    template = next(entry for entry in entries if source_path(entry).suffix in CPP_SUFFIXES
                    and target_for(entry, targets)['name'] == 'painter-foundation')
    report['controls'] = controls(args, template, Path(common['filename'][0]))
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    replay_dir = args.work_dir / 'objects'
    replay_dir.mkdir()

    def compile_one(pair):
        index, entry = pair
        source = source_path(entry)
        obj = replay_dir / (str(index) + '-' + source.name + '.o')
        log = replay_dir / (obj.name + '.log')
        command = safe_arguments(entry) + ['-fno-rtti', '-fdiagnostics-color=never', '-c', str(source), '-o', str(obj)]
        result = run(command, entry['directory'], log)
        result.update({'source': str(source.relative_to(ROOT)), 'original_output': entry['output'],
                       'source_sha256': digest(source), 'object': str(obj)})
        if succeeded(result):
            result['object_sha256'] = digest(obj)
        print(json.dumps({'replay': result['source'], 'returncode': result['returncode']}), flush=True)
        return result

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        report['replays'] = list(pool.map(compile_one, enumerate(replay)))
    report['original_objects_unchanged'] = all((digest(path) if Path(path).exists() else None) == sha for path, sha in original_objects.items())
    report['metadata_unchanged'] = all(digest(path) == sha for path, sha in report['inputs'].items())
    report['production_sources_unchanged'] = all(digest(ROOT / unit['source']) == unit['source_sha256']
                                                for unit in report['production_units'])
    units = report['production_units']
    painter = [unit for unit in units if unit['painter_cpp']]
    gaps = [unit['source'] for unit in painter if not unit['replayed'] and not rtti_disabled(unit['command'])]
    report['summary'] = {'production_cpp_commands': len(units), 'painter_cpp_commands': len(painter),
        'painter_with_existing_no_rtti': sum(rtti_disabled(unit['command']) for unit in painter),
        'core_display_replayed': len(replay),
        'painter_replayed': sum(unit['painter_cpp'] and unit['replayed'] for unit in units),
        'upstream_replayed': sum(not unit['painter_cpp'] and unit['replayed'] for unit in units),
        'successful_full_object_compiles': sum(succeeded(item) for item in report['replays']),
        'painter_flags_outside_replay_gaps': gaps,
        'foundation_cases_verified': 39 if report['foundation']['status'] == 'PASS' else 0,
        'foundation_runtime_rerun': report['foundation']['runtime_rerun'],
        'control_cases': len(report['controls']['cases'])}
    checks = [bool(replay), bool(painter), not gaps, report['original_objects_unchanged'],
              report['metadata_unchanged'], report['production_sources_unchanged'],
              report['controls']['status'] == 'PASS', report['foundation']['status'] == 'PASS',
              report['core_display_scope_complete'],
              all(succeeded(item) for item in report['replays'])]
    report['status'] = 'PASS' if all(checks) else 'FAIL'
    report['limitations'] = [
        'Only the listed native Linux/GNU-style commands are compiled; no cross-platform claim.',
        'Only core/display production C++ units are freshly recompiled; other targets are fully inventoried, not rebuilt.',
        'Foundation runtime reuse or fresh execution is explicitly identified; reused results require exact recorded identities.',
        'The typed-bridge control links the real existing common archive and exercises GType rejection, typed slots and C close.',
        'No production flags, original objects, build generators, GUI, profile or network listener are changed or run.',
        'Compiler exception matching may emit typeinfo even with -fno-rtti; absence of every typeinfo symbol is not required.',
        'An unambiguous dynamic_cast upcast can work without RTTI; negative controls specifically require runtime RTTI.',
        'Focused sanitizer RTTI/vptr rebuild closure is a separate policy and is unchanged.',
        'No whole-application relink, legacy adapter runtime acceptance or every-feature behavior is claimed.']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'status': report['status'], **report['summary']}, indent=2))
    return 0 if report['status'] == 'PASS' else 1


class SafetyTests(unittest.TestCase):
    def test_replay_removes_original_writes_and_retains_flag_order(self):
        entry = {'file': 'some unit.cpp', 'arguments': ['c++', '-Irelative', '-frtti', '-fexceptions',
                 '-fno-rtti', '-MD', '-MQ', 'old.o', '-MF', 'old.d', '-o', 'old.o', '-c', 'some unit.cpp']}
        self.assertEqual(safe_arguments(entry), ['c++', '-Irelative', '-frtti', '-fexceptions', '-fno-rtti'])
        self.assertTrue(rtti_disabled(safe_arguments(entry)))
        entry['arguments'].append('-frtti')
        self.assertFalse(rtti_disabled(safe_arguments(entry)))

    def test_replay_rejects_unbounded_output_modes(self):
        entry = {'file': 'unit.cpp', 'command': 'c++ -c unit.cpp -o unit.o -save-temps'}
        with self.assertRaises(ValueError):
            safe_arguments(entry)

    def test_repeated_production_source_in_fixture_is_excluded(self):
        self.assertTrue(production_target({'defined_in': '/repo/app/core/meson.build', 'type': 'static library'}))
        self.assertFalse(production_target({'defined_in': '/repo/app/core/meson.build', 'type': 'executable', 'installed': False}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--metadata-dir', type=Path)
    parser.add_argument('--configuration', choices=('default', 'http'))
    foundation = parser.add_mutually_exclusive_group()
    foundation.add_argument('--prior-evidence-dir', type=Path,
                            help='Root of extracted exception-policy/evidence.tar.gz (native/, current-inputs/, repository/)')
    foundation.add_argument('--run-foundation', action='store_true')
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--jobs', type=int, choices=(1, 2, 3, 4), default=2)
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(SafetyTests)
        return 0 if unittest.TextTestRunner().run(suite).wasSuccessful() else 1
    if not all((args.metadata_dir, args.configuration, args.work_dir, args.output)) or not (args.prior_evidence_dir or args.run_foundation):
        parser.error('paths, --configuration and either --prior-evidence-dir or --run-foundation are required')
    return check(args)


if __name__ == '__main__':
    sys.exit(main())
