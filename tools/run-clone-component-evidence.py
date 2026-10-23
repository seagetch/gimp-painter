#!/usr/bin/env python3
"""Run the bounded Clone normal/ASan/UBSan evidence gate under one build lock.

Requires a configured fresh modern build and authenticated portrait TCP Xvfb.
No source modifications, legacy-runtime work, aggregate/package acceptance or
LSan coverage are implied. Each attempt keeps private raw logs beside its public
receipt; neither previous failures nor original normal objects are overwritten.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path
import signal
import sys

sys.dont_write_bytecode = True
from clone_evidence import (ROOT, SCOPE, TARGETS, filtered_environment, PUBLIC_FILES,
    aggregate, canonical, digest, public_json, public_meson_text, validate_receipt)

RUNNER = 'tools/run-clone-component-evidence.py'
SANITIZER = 'migration/tests/run_clone_layer_sanitizers.py'


def save(path, value):
    path.write_bytes(canonical(value))


def inherited_lock(fd, path):
    expected, observed = path.stat(), os.fstat(fd)
    if (expected.st_dev, expected.st_ino) != (observed.st_dev, observed.st_ino):
        raise RuntimeError('Inherited descriptor is not the shared build lock')
    with path.open('r') as probe:
        try:
            fcntl.flock(probe, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError:
            pass
        else:
            fcntl.flock(probe, fcntl.LOCK_UN)
            raise RuntimeError('Caller does not hold the exclusive build lock')
    fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)


def run_locked(args, output, private):
    repository, build = args.repository.resolve(), args.build.resolve()
    receipt = {'schema': 1, 'scope': SCOPE, 'status': 'failed', 'errors': [],
               'source_commit': args.expected_commit, 'source_tree': args.expected_tree,
               'source_directory': str(repository),
               'build_directory': str(build), 'commands': [], 'files': {},
               'started_utc': datetime.now(timezone.utc).isoformat(),
               'limitation': 'Focused eight-source ASan/UBSan only; LSan disabled. This is not full aggregate or platform/release acceptance.'}
    source, elf, freshness, transforms = {}, {}, {}, {}
    helper = None
    locale_directory = None
    locales = {}

    def command(argv, name, environment):
        record = {'argv': argv, 'exit_code': None}
        receipt['commands'].append(record)
        raw = private / name
        try:
            record['exit_code'] = aggregate.run_command(argv, raw, environment)
        finally:
            if raw.exists():
                (output / name).write_bytes(raw.read_bytes())
                record['raw_log_sha256'] = digest(raw.read_bytes())
                record['public_log_sha256'] = digest((output / name).read_bytes())
        if record['exit_code'] != 0:
            raise RuntimeError('Failed command (exit ' + str(record['exit_code']) + '): ' + name)

    try:
        helper = aggregate.load_committed_helper(repository, args.expected_commit)
        source['before_build'] = aggregate.source_state(repository, helper)
        aggregate.require_source(source['before_build'], args.expected_commit, args.expected_tree)
        receipt['normal_build'] = aggregate.require_normal_build(build, repository)
        receipt['provenance'] = {name: aggregate.sha(repository / name) for name in
            (RUNNER, SANITIZER, 'tools/clone_evidence.py', aggregate.RUNNER, aggregate.HELPER)}
        if not os.environ.get('DISPLAY') or not os.environ.get('XAUTHORITY'):
            raise RuntimeError('Authenticated portrait Xvfb DISPLAY and XAUTHORITY are required')
        inherited = filtered_environment(os.environ)
        save(output / 'environment.json', inherited)
        environment = aggregate.isolated_child_environment(private, inherited)
        baseline = {'receipt': json.loads((args.baseline / 'receipt.json').read_text()),
                    'receipt_sha256': aggregate.sha(args.baseline / 'receipt.json'), 'logs': {}}
        for path in sorted(args.baseline.iterdir()):
            if path.name != 'receipt.json':
                raw = path.read_bytes()
                baseline['logs'][path.name] = {'sha256': digest(raw), 'text': raw.decode()}
        save(output / 'baseline.json', baseline)
        if baseline != json.loads((repository / 'migration/tests/clone-component-baseline.json').read_text()):
            raise RuntimeError('Baseline provenance differs from the committed original failed attempts')
        command(['ninja', '-C', str(build), '-j4'], 'default-build.log', environment)
        source['before_tests'] = aggregate.source_state(repository, helper)
        aggregate.require_source(source['before_tests'], args.expected_commit, args.expected_tree)
        if source['before_build'] != source['before_tests']:
            raise RuntimeError('Sources changed during full default build')
        aggregate.require_normal_build(build, repository)
        locale_directory = private / 'locale'
        locales['before'] = aggregate.stage_build_locales(build, locale_directory)
        if locales['before']['catalog_count'] != 361:
            raise RuntimeError('Expected all 361 configured global catalogs')
        environment['GIMP3_LOCALEDIR'] = str(locale_directory)
        raw_registry = (build / 'meson-info/intro-tests.json').read_bytes()
        registry = json.loads(raw_registry)
        aggregate.validate_registry(registry)
        receipt['raw_registry_sha256'] = digest(raw_registry)
        published, transforms['registered-tests.json'] = public_json(raw_registry)
        (output / 'registered-tests.json').write_bytes(published)
        compile_raw = (build / 'compile_commands.json').read_bytes()
        receipt['compile_database_sha256'] = digest(compile_raw)
        published, transforms['compile-database.json'] = public_json(compile_raw)
        (output / 'compile-database.json').write_bytes(published)
        elf['before'] = aggregate.elf_inventory(build, helper)
        freshness['before'] = helper.capture_freshness_snapshot(build, repository)
        logbase = private / 'meson'
        receipt['raw_logbase'] = str(logbase)
        try:
            command(['meson', 'test', '-C', str(build), '--no-rebuild', '--num-processes', '1',
                     '--print-errorlogs', '--logbase', str(logbase), *TARGETS],
                    'meson-console.log', environment)
        finally:
            if logbase.with_suffix('.json').exists():
                published, transforms['normal-results.json'] = public_json(logbase.with_suffix('.json').read_bytes(), True)
                (output / 'normal-results.json').write_bytes(published)
            if logbase.with_suffix('.txt').exists():
                published, transforms['testlog.txt'] = public_meson_text(logbase.with_suffix('.txt').read_bytes())
                (output / 'testlog.txt').write_bytes(published)
        sanitizer_directory = private / 'instrumented'
        report = private / 'sanitizers.json'
        try:
            command([sys.executable, '-B', str(repository / SANITIZER), str(build),
                     '--report', str(report), '--output-dir', str(sanitizer_directory)],
                    'sanitizer-console.log', environment)
        finally:
            if report.exists():
                raw = report.read_bytes()
                published, transforms['sanitizers.json'] = public_json(raw)
                (output / 'sanitizers.json').write_bytes(published)
                san = json.loads(raw)
                actual = {p.name: helper.path_fingerprint(p) for p in sanitizer_directory.iterdir()} if sanitizer_directory.exists() else {}
                save(output / 'sanitizer-artifacts.json', {'directory': str(sanitizer_directory),
                    'files': actual, 'before_run': san.get('artifacts_before_run'),
                    'after_run': san.get('artifacts_after_run')})
    except (Exception, KeyboardInterrupt) as error:
        receipt['errors'].append(f'{type(error).__name__}: {error}')
    finally:
        if helper is not None:
            for name, capture, destination in (
                    ('source', lambda: aggregate.source_state(repository, helper), source),
                    ('elf', lambda: aggregate.elf_inventory(build, helper), elf),
                    ('freshness', lambda: helper.capture_freshness_snapshot(build, repository), freshness)):
                try:
                    destination['after'] = capture()
                except Exception as error:
                    receipt['errors'].append('After seal ' + name + ': ' + str(error))
            if locale_directory is not None and 'before' in locales:
                try:
                    aggregate.verify_staged_locales(locale_directory, locales['before'])
                    if any(aggregate.sha(build / row['source']) != row['sha256'] for row in locales['before']['catalogs'].values()):
                        raise RuntimeError('Original build catalogs changed')
                    # Independent after byte read; retain the original declared metadata.
                    locales['after'] = json.loads(json.dumps(locales['before']))
                    for name, row in locales['after']['catalogs'].items():
                        row['sha256'] = aggregate.sha(locale_directory / name)
                        row['size_bytes'] = (locale_directory / name).stat().st_size
                except Exception as error:
                    receipt['errors'].append('Catalog after seal: ' + str(error))
        for name, value in (('source-integrity.json', source), ('elf-integrity.json', elf),
                            ('freshness.json', freshness), ('transforms.json', transforms), ('locales.json', locales)):
            save(output / name, value)
        if 'compile_database_sha256' in receipt and aggregate.sha(build / 'compile_commands.json') != receipt['compile_database_sha256']:
            receipt['errors'].append('Full compile database changed during run')
        receipt['finished_utc'] = datetime.now(timezone.utc).isoformat()
        receipt['files'] = {path.name: digest(path.read_bytes()) for path in output.iterdir() if path.is_file() and path.name != 'receipt.json'}
        if not receipt['errors'] and set(receipt['files']) == PUBLIC_FILES:
            receipt['status'] = 'passed'
        save(output / 'receipt.json', receipt)
        if receipt['status'] == 'passed':
            errors = validate_receipt(output, repository)['errors']
            if errors:
                receipt['errors'].extend(errors)
                receipt['status'] = 'failed'
                save(output / 'receipt.json', receipt)
    print('Clone evidence ' + receipt['status'] + ': ' + str(output / 'receipt.json'))
    if receipt['errors']:
        print('\n'.join(receipt['errors']), file=sys.stderr)
    return 0 if receipt['status'] == 'passed' else 1


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=ROOT)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--expected-tree', required=True)
    parser.add_argument('--lock-fd', type=int, help='Already-held inherited exclusive shared lock')
    args = parser.parse_args(argv)
    repository, build, output = args.repository.resolve(), args.build.resolve(), args.output.resolve()
    private = output.with_name(output.name + '-private')
    if any(path.is_relative_to(repository) or path.is_relative_to(build) for path in (output, private)):
        parser.error('Evidence and sanitizer outputs must be outside source and normal build')
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    private.mkdir(exist_ok=False, mode=0o700)
    def interrupted(number, frame):
        raise KeyboardInterrupt('Received signal ' + str(number))
    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGHUP, interrupted)
    if args.lock_fd is not None:
        inherited_lock(args.lock_fd, aggregate.SHARED_LOCK)
        return run_locked(args, output, private)
    with aggregate.SHARED_LOCK.open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        return run_locked(args, output, private)


if __name__ == '__main__':
    sys.exit(main())
