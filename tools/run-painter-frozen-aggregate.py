#!/usr/bin/env python3
"""Produce a strict, package-consumable Linux aggregate from committed sources.

Run inside the authenticated portrait-Xvfb environment. No test filters, command
wrappers, generated-Meson edits, baseline exemptions, or reconstructed seals.
See migration/contracts/frozen-linux-aggregate.md.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import fcntl
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import platform
import re
import signal
import subprocess
import sys
import tempfile

SCOPE = 'all_registered_meson_tests_frozen_linux_normal_build'
EXECUTABLES = ('app/gimp-3.0', 'app/gimp-console-3.0')
SHARED_LOCK = Path('/workspace/shared/gimp-painter-build.lock')
RUNNER = 'tools/run-painter-frozen-aggregate.py'
HELPER = 'tools/package-linux-runtime.py'

# Mutable user/runtime state belongs to this invocation, never the caller's
# home or a previous run. Explicit GIMP directories must already exist before
# Filter spill admission asks the filesystem for their available capacity.
PRIVATE_DIRECTORIES = {
    'HOME': 'home',
    'XDG_CONFIG_HOME': 'config',
    'XDG_CACHE_HOME': 'cache',
    'XDG_DATA_HOME': 'data',
    'XDG_STATE_HOME': 'state',
    'XDG_RUNTIME_DIR': 'runtime',
    'TMPDIR': 'tmp',
    'GIMP3_DIRECTORY': 'gimp-profile',
    'GIMP3_TEMPDIR': 'gimp-temp',
    'GIMP3_CACHEDIR': 'gimp-cache',
}

# These are expected *negative-test output*, not exemptions for failing tests.
# They remain visible in the sanitized per-target logs and gate.
NEGATIVE_WARNINGS = {
    'painter-compact-options': (
        r'There are no selected layers or channels to (?:fill|stroke to)\.',
        r"A selected layer's pixels are locked\.",
    ),
    'painter-xcf-adversarial': (
        r'This XCF drawable is incomplete\. Complete tiles and the original source have been retained; Save is disabled to avoid data loss\.',
        r'This XCF file is corrupt!  I have loaded as much of it as I can, but it is incomplete\.',
    ),
    'painter-xcf-roundtrip': (
        r'Painter XCF metadata was retained but could not be fully restored: (?:unknown extension version|Unsupported or malformed saved Filter argument; original definition is retained|Invalid arguments|Duplicate Painter metadata key; original capsule is retained and cannot be safely rewritten|unknown layer kind|Missing layer kind|Painter metadata conflicts with its native owner or semantic schema; original capsule remains inert|Saved Filter arguments exceed the mutable materialization limit; original definition is retained)',
        r'XCF Warning: version 0:0 of filter "gegl:brightness-contrast" does not have the future-value property\. The property was ignored and the filter may not render properly\.',
        r'XCF Warning: the "unavailable effect" \(xxxxxxxxxxxxxxxxxxxxxxxx\) filter is not installed\. It was discarded\.',
        r'XCF Warning: argument "format" of filter gegl:color has unsupported type gpointer\. It was discarded\.',
    ),
    'painter-fill-brush-ui': (
        r'Pending Fill Brush strokes were canceled because their target changed\.',
        r'Invalid Fill Brush coordinates\.',
    ),
    'painter-smudge-ui': (
        r'Pending Painter Smudge strokes were canceled because their target changed\.',
        r'Invalid Painter Smudge coordinates\.',
    ),
    'painter-paper-ui': (
        r'Pending (?:Fill Brush|Painter Smudge) strokes were canceled because their target changed\.',
    ),
    'painter-save-continuation': (
        r'Pending Fill Brush strokes were canceled because their target changed\.',
    ),
}
NEGATIVE_ERRORS = {
    'painter-xcf-adversarial': (
        r'not enough tiles found in level',
        r'encountered garbage after reading level: (?:138|178)',
    ),
    'selection-float': (
        r"(?:Execution|Calling) error for procedure 'gimp-selection-float':",
    ),
}
CRASH = re.compile(
    r'\b(?:segmentation fault|segfault|bus error|core dumped|fatal error|'
    r'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|ThreadSanitizer)\b|'
    r'Traceback \(most recent call last\)|\bSIG(?:SEGV|BUS|ABRT|ILL|FPE)\b', re.I)
FAILURE = re.compile(
    r'^\s*(?:not ok\b|Bail out!)|\b(?:CRITICAL|FATAL)\b|'
    r'\bassertion\b.*\bfailed\b|\bruntime (?:check failed|error:)', re.I)
WARNING = re.compile(r'\b[\w-]+-WARNING\b|\bWARNING\s*(?:\*\*|:)|\*\*\s*:?\s*WARNING\b', re.I)
TAP_SKIP = re.compile(r'^\s*(?:(?:not )?ok\b[^\n]*|\d+\.\.\d+\s*)#\s*(?:SKIP|TODO)\b', re.I)
SECRET_KEY = re.compile(r'password|passwd|secret|token|api[_-]?key|access[_-]?key|credential|private[_-]?key|cookie|authorization|auth[_-]?key|(?:^|_)(?:auth|authentication)(?:$|_)', re.I)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def git(repository, *arguments):
    return subprocess.check_output(['git', '-C', str(repository), *arguments],
                                   stderr=subprocess.PIPE).decode().strip()


class Sanitizer:
    """No env objects are published; redact credentials if echoed in diagnostics."""
    def __init__(self, environment=None):
        self.secrets = set()
        self.add_environment(os.environ if environment is None else environment)

    def add_environment(self, environment):
        if isinstance(environment, dict):
            self.secrets.update(value for key, value in environment.items()
                                if SECRET_KEY.search(key) and not re.search(r'_(?:CAPABILITIES|ENABLED?)$', key, re.I)
                                and isinstance(value, str) and value)

    def text(self, value):
        value = str(value)
        for secret in sorted(self.secrets, key=len, reverse=True):
            value = value.replace(secret, '[REDACTED]')
        lines = []
        for line in value.splitlines(keepends=True):
            # Meson's text logs and error summaries can contain inherited or
            # per-test shell environment assignments. They are not evidence.
            if (re.match(r'^\s*(?:Inherited environment:|Environment:)', line, re.I) or
                    re.match(r'^\s*(?:export\s+)?[A-Za-z_][A-Za-z_0-9]*=', line)):
                lines.append('[environment line omitted]\n')
                continue
            line = re.sub(r'(?i)(Authorization\s*:\s*(?:Bearer|Basic)\s+)\S+', r'\1[REDACTED]', line)
            line = re.sub(r'(?i)((?:password|passwd|secret|token|api[_-]?key)\s*[=:]\s*)[^\s,;]+',
                          r'\1[REDACTED]', line)
            line = re.sub(r'(?i)(--(?:password|passwd|secret|token|api[_-]?key)\s+)\S+',
                          r'\1[REDACTED]', line)
            line = re.sub(r'(https?://)[^/\s:@]+:[^/\s@]+@', r'\1[REDACTED]@', line)
            lines.append(line)
        return ''.join(lines)

    def object(self, value):
        if isinstance(value, dict):
            return {key: self.object(item) for key, item in value.items()
                    if key.lower() not in {'env', 'environment', 'inherited_environment'}}
        if isinstance(value, list):
            result = [self.object(item) for item in value]
            for index in range(1, len(value)):
                if isinstance(value[index - 1], str) and re.fullmatch(
                        r'--(?:password|passwd|secret|token|api[_-]?key)', value[index - 1], re.I):
                    result[index] = '[REDACTED]'
            return result
        return self.text(value) if isinstance(value, str) else value


def write_json(path, value, sanitizer):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(sanitizer.object(value), indent=2, ensure_ascii=False) + '\n')
    temporary.replace(path)


def load_committed_helper(repository, expected_commit):
    """Import the actual committed helper, never a copied/reimplemented validator."""
    verified = {}
    for name, path in ((HELPER, repository / HELPER), (RUNNER, Path(__file__).resolve())):
        committed = subprocess.check_output(['git', '-C', str(repository), 'show',
                                             f'{expected_commit}:{name}'], stderr=subprocess.PIPE)
        if path.read_bytes() != committed or (repository / name).read_bytes() != committed:
            raise RuntimeError(f'Runner/helper must be the exact committed file: {name}')
        verified[name] = committed
    spec = importlib.util.spec_from_file_location('frozen_aggregate_package_helper', repository / HELPER)
    module = importlib.util.module_from_spec(spec)
    # Bypass timestamp-based .pyc reads as well as writes: an ignored stale cache
    # must never execute different code from the committed bytes just verified.
    exec(compile(verified[HELPER], str(repository / HELPER), 'exec'), module.__dict__)
    return module


def git_blob_id(path, algorithm):
    """Hash working-tree bytes as a Git blob, independently of index stat flags."""
    if path.is_symlink():
        data = os.fsencode(os.readlink(path))
        return hashlib.new(algorithm, f'blob {len(data)}\0'.encode() + data).hexdigest()
    digest = hashlib.new(algorithm)
    digest.update(f'blob {path.stat().st_size}\0'.encode())
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def source_state(repository, helper):
    files, submodules = {}, {}
    mismatches = []
    algorithm = git(repository, 'rev-parse', '--show-object-format')
    flags = subprocess.check_output(['git', '-C', str(repository), 'ls-files', '-v', '-z']).decode().split('\0')
    hidden = [entry[2:] for entry in flags if entry and (entry[0].islower() or entry[0] == 'S')]
    entries = subprocess.check_output(['git', '-C', str(repository), 'ls-files', '--stage', '-z'])
    for entry in entries.decode().split('\0'):
        if not entry:
            continue
        metadata, name = entry.split('\t', 1)
        mode, pinned, stage = metadata.split()
        if stage != '0':
            raise RuntimeError('Unmerged tracked source')
        if mode == '160000':
            child = source_state(repository / name, helper)
            child['pinned_commit'] = pinned
            child['clean'] = child['clean'] and child['commit'] == pinned
            submodules[name] = child
        else:
            path = repository / name
            files[name] = helper.path_fingerprint(path)
            correct_type = path.is_symlink() if mode == '120000' else path.is_file() and not path.is_symlink()
            correct_mode = correct_type and (mode == '120000' or
                            bool(path.stat().st_mode & 0o111) == (mode == '100755'))
            if not correct_mode or git_blob_id(path, algorithm) != pinned:
                mismatches.append(name)
    status = git(repository, 'status', '--porcelain', '--untracked-files=all')
    return {'commit': git(repository, 'rev-parse', 'HEAD'),
            'tree': git(repository, 'rev-parse', 'HEAD^{tree}'),
            'clean': not status and not hidden and not mismatches and all(child['clean'] for child in submodules.values()),
            'status': status, 'hidden_index_entries': hidden, 'committed_content_mismatches': mismatches,
            'tracked_files': files, 'submodules': submodules}


def require_source(state, commit, tree):
    if state['commit'] != commit or state['tree'] != tree or not state['clean']:
        raise RuntimeError('Aggregate requires the expected commit/tree and clean source including pinned submodules')


def require_normal_build(build, repository):
    info = json.loads((build / 'meson-info/meson-info.json').read_text())
    machines = json.loads((build / 'meson-info/intro-machines.json').read_text())
    options = {row['name']: row['value'] for row in
               json.loads((build / 'meson-info/intro-buildoptions.json').read_text())}
    if (platform.system() != 'Linux' or machines['host']['system'] != 'linux' or
            machines['host'] != machines['build'] or
            Path(info['directories']['source']).resolve() != repository or
            Path(info['directories']['build']).resolve() != build or
            options.get('backend') != 'ninja' or options.get('b_sanitize') != 'none' or
            options.get('b_coverage') is not False):
        raise RuntimeError('Expected the normal native Linux Ninja build, without sanitizers or coverage')
    # Do not publish compiler flags or environment payloads.
    return {key: options.get(key) for key in ('backend', 'buildtype', 'b_sanitize', 'b_coverage')}


def elf_inventory(build, helper):
    result = {}
    for path in sorted(build.rglob('*')):
        if path.is_file():
            with path.open('rb') as stream:
                is_elf = stream.read(4) == b'\x7fELF'
            if is_elf:
                result[str(path.relative_to(build))] = helper.path_fingerprint(path)
    return result


def elf_changes(before, after, build):
    changed = sorted(name for name, fingerprint in before.items() if after.get(name) != fingerprint)
    added = sorted(after.keys() - before.keys())
    verified, rejected = [], []
    for name in added:
        path = build / name
        parts = Path(name).parts
        private = build / parts[0]
        target = path.resolve()
        relative = str(target.relative_to(build)) if target.is_relative_to(build) else None
        valid = (len(parts) > 1 and parts[0].startswith('.GIMP3-build-config-') and
                 private.is_dir() and not private.is_symlink() and
                 private.stat().st_uid == os.getuid() and private.stat().st_mode & 0o077 == 0 and
                 path.is_symlink() and relative in before and
                 'symlink' not in before[relative] and after[name].get('content') == before[relative])
        (verified if valid else rejected).append(name)
    return changed, verified, rejected


def pretty_name(registered, registry):
    suites = {suite for row in registry for suite in row['suite']}
    if len(suites) > 1 and registered['suite']:
        project = registered['suite'][0].split(':', 1)[0]
        names = '+'.join(suite.partition(':')[2] for suite in registered['suite'])
        return project + (':' if names else '') + names + ' / ' + registered['name']
    return registered['name']


def diagnostics(name, output):
    skipped, crashes, failures, nonfatal = [], [], [], []
    for line in output.splitlines():
        known_warning = False
        if TAP_SKIP.search(line):
            skipped.append(line)
        if CRASH.search(line):
            crashes.append(line)
        if FAILURE.search(line):
            failures.append(line)
        if 'GIMP-Warning:' in line:
            prefix, warning = line.split('GIMP-Warning:', 1)
            warning = warning.strip()
            if (not WARNING.search(prefix) and
                    (re.fullmatch(r'Welcome to GIMP \d+\.\d+\.\d+!', warning) or
                     any(re.fullmatch(pattern, warning) for pattern in NEGATIVE_WARNINGS.get(name, ())))):
                nonfatal.append(line)
                known_warning = True
            else:
                failures.append(line)
        if WARNING.search(line) and not known_warning and line not in failures:
            failures.append(line)
        if re.search(r'\bGIMP-Error:', line):
            message = line.split('GIMP-Error:', 1)[1].strip()
            if any(re.fullmatch(pattern, message) for pattern in NEGATIVE_ERRORS.get(name, ())):
                nonfatal.append(line)
            else:
                failures.append(line)
    return skipped, crashes, failures, nonfatal


def registry_archive(registry):
    fields = ('name', 'cmd', 'suite', 'workdir', 'timeout', 'is_parallel', 'priority', 'protocol', 'depends')
    return [{'registry_index': index, **{key: row[key] for key in fields if key in row}}
            for index, row in enumerate(registry, 1)]


def validate_registry(registry):
    if not isinstance(registry, list) or not registry:
        raise RuntimeError('Meson test registry is empty or invalid')
    for row in registry:
        if (not isinstance(row, dict) or not isinstance(row.get('name'), str) or not row['name'] or
                not isinstance(row.get('cmd'), list) or not row['cmd'] or
                any(not isinstance(argument, str) for argument in row['cmd']) or
                not isinstance(row.get('suite'), list) or
                any(not isinstance(suite, str) for suite in row['suite'])):
            raise RuntimeError('Meson registry entry has invalid name, command or suites')


def read_results(path, sanitizer):
    results, errors = [], []
    if not path.is_file():
        return [], ['Meson did not produce its JSON results log']
    for number, line in enumerate(path.read_text(errors='replace').splitlines(), 1):
        try:
            row = json.loads(line)
            if not isinstance(row, dict):
                raise ValueError('result must be an object')
            sanitizer.add_environment(row.get('env'))
            results.append({key: row[key] for key in
                            ('name', 'command', 'result', 'returncode', 'starttime', 'duration', 'stdout', 'stderr')
                            if key in row})
        except (ValueError, TypeError):
            errors.append(f'Invalid Meson JSON result at line {number}')
    return results, errors


def coverage_gate(registry, results):
    """Match observed commands AND qualified names, never list positions alone."""
    remaining = set(range(len(results)))
    targets, suites = [], defaultdict(lambda: {'registered': 0, 'outcomes': Counter()})
    for index, registered in enumerate(registry, 1):
        matches = [number for number in remaining if
                   results[number].get('name') == pretty_name(registered, registry) and
                   results[number].get('command') == registered['cmd']]
        row = results[matches[0]] if len(matches) == 1 else {}
        if row:
            remaining.remove(matches[0])
        output = row.get('stdout', '') + '\n' + row.get('stderr', '')
        skipped, crashes, failures, nonfatal = diagnostics(registered['name'], output)
        if not row:
            failures.append('Missing or ambiguous observed Meson result for this registered command/name')
        passed = (row.get('result') == 'OK' and type(row.get('returncode')) is int and
                  row['returncode'] == 0 and not skipped and not crashes and not failures)
        target = {'registry_index': index, 'name': registered['name'], 'suites': registered['suite'],
                  'registered_command': registered['cmd'], 'actual_command': row.get('command'),
                  'meson_name': row.get('name'), 'result': row.get('result', 'MISSING'),
                  'returncode': row.get('returncode'), 'classification': 'passed' if passed else 'failed',
                  'tap_skipped_subtests': skipped, 'runtime_crash_diagnostics': crashes,
                  'failure_evidence': failures, 'nonfatal_warnings': nonfatal}
        targets.append(target)
        for suite in registered['suite']:
            suites[suite]['registered'] += 1
            suites[suite]['outcomes'][target['result']] += 1
    unmatched = [results[number] for number in sorted(remaining)]
    complete = bool(registry) and len(results) == len(registry) and not unmatched and all(
        target['actual_command'] is not None for target in targets)
    return {'targets': targets, 'registered_tests': len(registry), 'recorded_results': len(results),
            'coverage_complete': complete, 'unmatched_results': unmatched,
            'result_counts': dict(Counter(row.get('result', 'MISSING') for row in results)),
            'suite_counts': dict(suites)}


def isolated_child_environment(private, inherited=None):
    """Prepare fresh writable state while retaining dependency/display inputs."""
    environment = dict(os.environ if inherited is None else inherited)
    private = Path(private).resolve()
    directories = []
    for variable, name in PRIVATE_DIRECTORIES.items():
        directory = private / name
        # Refuse preexisting state, including symlinks. The caller owns the
        # fresh temporary parent and removes all state when the run finishes.
        directory.mkdir(mode=0o700)
        environment[variable] = str(directory)
        directories.append(directory)
    font_cache = Path(environment['XDG_CACHE_HOME']) / 'fontconfig'
    font_cache.mkdir(mode=0o700)
    directories.append(font_cache)
    # Test creation and writing, not os.access() or permission bits alone.
    # These probes are removed before any build or test starts.
    for directory in directories:
        with tempfile.TemporaryFile(dir=directory) as probe:
            probe.write(b'aggregate writable-directory preflight\n')
            probe.flush()
    environment['GSETTINGS_BACKEND'] = 'memory'
    environment['PYTHONDONTWRITEBYTECODE'] = '1'
    return environment


def run_command(command, log_path, environment=None):
    # Python test helpers import modules from the committed source tree. Keep
    # their caches out of that tree without ignoring or excusing dirty files.
    environment = dict(os.environ if environment is None else environment)
    environment['PYTHONDONTWRITEBYTECODE'] = '1'
    with log_path.open('w') as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=True, env=environment)
        try:
            return process.wait()
        except BaseException:
            # Let Meson cancel its independently grouped test processes before
            # taking after-facts. Only signal this invocation's process group.
            try:
                # Meson's SIGINT handler can cancel only the current test and
                # continue scheduling. SIGTERM cancels the complete aggregate.
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
            raise


def execute(args, output, sanitizer):
    try:
        with args.lock.open('a') as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            return execute_locked(args, output, sanitizer)
    except (OSError, KeyboardInterrupt) as error:
        write_json(output / 'aggregate-gate.json', {
            'schema_version': 1, 'scope': SCOPE, 'all_passed': False,
            'errors': [f'Could not acquire/retain shared build lock: {type(error).__name__}: {error}'],
            'packager_validation': 'rejected'}, sanitizer)
        return 1


def execute_locked(args, output, sanitizer):
    repository, build = args.repository.resolve(), args.build.resolve()
    gate = {'schema_version': 1, 'scope': SCOPE, 'all_passed': False,
            'baseline_exemptions_applied': False, 'expected_source_commit': args.expected_commit,
            'expected_source_tree': args.expected_tree, 'meson_exit_status': None,
            'build_freshness': {}, 'errors': [], 'started_at': datetime.now(timezone.utc).isoformat()}
    helper = None
    before_source = before_elf = registry = None
    try:
        # Keep the lock through default build, both seals, test run, verification,
        # and evidence writes. Never acquire it separately around individual steps.
        # Evidence is validated outside source/build; use its parent explicitly
        # so an inherited TMPDIR inside either tree cannot dirty frozen inputs.
        # Keep raw logs in a private sibling, never inside the evidence directory.
        with tempfile.TemporaryDirectory(prefix='gimp-aggregate-private-', dir=output.parent) as private:
            private = Path(private)
            helper = load_committed_helper(repository, args.expected_commit)
            initial = source_state(repository, helper)
            gate.update(source_commit=initial['commit'], source_tree=initial['tree'])
            write_json(output / 'source-before-build.json', initial, sanitizer)
            require_source(initial, args.expected_commit, args.expected_tree)
            gate['normal_build'] = require_normal_build(build, repository)
            if not os.environ.get('DISPLAY') or not os.environ.get('XAUTHORITY'):
                raise RuntimeError('Run inside the authenticated portrait-Xvfb wrapper (DISPLAY and XAUTHORITY required)')
            gate['provenance'] = {name: {'sha256': sha(repository / name)} for name in (RUNNER, HELPER)}
            child_environment = isolated_child_environment(private)
            gate['runtime_isolation'] = {
                'private_directory_variables': list(PRIVATE_DIRECTORIES),
                'fontconfig_user_cache_created': True,
                'directory_write_preflight': 'passed',
                'gsettings_backend': 'memory',
            }
            build_command = ['ninja', '-C', str(build)]
            build_log = private / 'build.log'
            build_status = None
            gate['default_build'] = {'command': build_command, 'exit_status': None}
            try:
                build_status = run_command(build_command, build_log, child_environment)
            finally:
                gate['default_build']['exit_status'] = build_status
                if build_log.is_file():
                    (output / 'default-build.log').write_text(sanitizer.text(build_log.read_text(errors='replace')))
            if build_status != 0:
                raise RuntimeError('Full default Ninja build failed; no aggregate was run')
            before_source = source_state(repository, helper)
            require_source(before_source, args.expected_commit, args.expected_tree)
            if before_source != initial:
                raise RuntimeError('Source state changed during the full default build')
            require_normal_build(build, repository)
            registry_path = build / 'meson-info/intro-tests.json'
            registry = json.loads(registry_path.read_text())
            validate_registry(registry)
            for row in registry:
                sanitizer.add_environment(row.get('env'))
            gate['registry_sha256'] = sha(registry_path)
            write_json(output / 'registered-tests.json', registry_archive(registry), sanitizer)
            before_elf = elf_inventory(build, helper)
            if not all(name in before_elf for name in EXECUTABLES):
                raise RuntimeError('Both normal GIMP executables must be present in the observed ELF inventory')
            gate['build_inputs'] = {name: {'sha256': sha(build / name)} for name in EXECUTABLES}
            # Capture immediately before invocation, after the completed default
            # build. These are observed facts; never copy an after-seal into before.
            gate['build_freshness']['before'] = helper.capture_freshness_snapshot(build, repository)
            logbase = private / 'meson'
            command = ['meson', 'test', '-C', str(build), '--no-rebuild', '--num-processes', '1',
                       '--print-errorlogs', '--logbase', str(logbase)]
            gate['meson_command'] = command
            try:
                gate['meson_exit_status'] = run_command(command, private / 'meson-console.log', child_environment)
            finally:
                # Even a failed/interrupted Meson invocation gets independently
                # observed after-facts and any complete result rows it produced.
                for key, capture in (
                        ('after', lambda: helper.capture_freshness_snapshot(build, repository)),
                        ('source_after', lambda: source_state(repository, helper)),
                        ('elf_after', lambda: elf_inventory(build, helper))):
                    try:
                        value = capture()
                        if key == 'after':
                            gate['build_freshness'][key] = value
                        else:
                            gate[key] = value
                            filename = 'source-integrity.json' if key == 'source_after' else 'elf-integrity.json'
                            initial_value = before_source if key == 'source_after' else before_elf
                            write_json(output / filename, {'before': initial_value, 'after': value}, sanitizer)
                    except Exception as error:
                        gate['errors'].append(f'Could not capture {key}: {error}')
                results, parse_errors = read_results(logbase.with_suffix('.json'), sanitizer)
                gate['errors'].extend(parse_errors)
                gate.update(coverage_gate(registry, results))
                write_json(output / 'test-results.json', results, sanitizer)
                console = private / 'meson-console.log'
                if console.is_file():
                    (output / 'meson-console.log').write_text(sanitizer.text(console.read_text(errors='replace')))
            after_source = gate.pop('source_after', {})
            after_elf = gate.pop('elf_after', {})
            changed, aliases, rejected = elf_changes(before_elf, after_elf, build)
            tracked_before = before_source['tracked_files']
            tracked_after = after_source.get('tracked_files', {})
            gate['changed_tracked_files'] = sorted(name for name in tracked_before.keys() | tracked_after.keys()
                                                   if tracked_before.get(name) != tracked_after.get(name))
            gate['changed_preexisting_elf_files'] = changed
            gate['new_verified_private_profile_elf_aliases'] = aliases
            gate['new_unverified_elf_entries'] = rejected
            gate['integrity_checks'] = {
                'expected_source_commit': before_source['commit'] == args.expected_commit,
                'expected_source_tree': before_source['tree'] == args.expected_tree,
                'commit_unchanged': before_source['commit'] == after_source.get('commit'),
                'tree_unchanged': before_source['tree'] == after_source.get('tree'),
                'tracked_tree_clean_before_and_after': before_source['clean'] and after_source.get('clean') is True,
                'tracked_files_unchanged': not gate['changed_tracked_files'],
                'pinned_submodules_unchanged': before_source['submodules'] == after_source.get('submodules'),
                'preexisting_elf_files_unchanged': not changed,
                'all_new_elf_entries_are_verified_private_test_profile_aliases': not rejected,
                'test_registry_unchanged': gate['registry_sha256'] == sha(registry_path),
            }
            write_json(output / 'source-integrity.json', {'before': before_source, 'after': after_source}, sanitizer)
            write_json(output / 'elf-integrity.json', {'before': before_elf, 'after': after_elf}, sanitizer)
            gate['all_passed'] = (not gate['errors'] and gate['coverage_complete'] and
                                  gate['meson_exit_status'] == 0 and
                                  all(target['classification'] == 'passed' for target in gate['targets']) and
                                  all(value is True for value in gate['integrity_checks'].values()))
            # Validate the *published representation*, not an unsanitized object
            # that differs from the bytes the packager will consume.
            published = sanitizer.object(gate)
            if published['build_freshness'] != gate['build_freshness']:
                raise RuntimeError('Sanitization would alter sealed freshness metadata; cannot accept the gate')
            helper.require_aggregate_gate(published, args.expected_commit, build)
            gate['packager_validation'] = 'accepted'
            gate['finished_at'] = datetime.now(timezone.utc).isoformat()
            write_json(output / 'aggregate-gate.json', gate, sanitizer)
            return 0
    except (Exception, KeyboardInterrupt) as error:
        gate['all_passed'] = False
        gate['errors'].append(f'{type(error).__name__}: {error}')
        gate['packager_validation'] = 'rejected'
        gate['finished_at'] = datetime.now(timezone.utc).isoformat()
        write_json(output / 'aggregate-gate.json', gate, sanitizer)
        return 1


def main(argv=None):
    sys.dont_write_bytecode = True
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory outside source and build')
    parser.add_argument('--expected-commit', required=True, help='Full committed source object ID')
    parser.add_argument('--expected-tree', required=True, help='Full committed source tree object ID')
    parser.add_argument('--lock', type=Path, default=SHARED_LOCK)
    args = parser.parse_args(argv)
    repository, build, output = args.repository.resolve(), args.build.resolve(), args.output.resolve()
    if any(not re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}', value)
           for value in (args.expected_commit, args.expected_tree)):
        parser.error('Expected commit/tree must be full hexadecimal object IDs')
    if output.is_relative_to(repository) or output.is_relative_to(build):
        parser.error('Evidence directory must be outside the source and build trees')
    # Never overwrite a previous passing or failing gate.
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    def interrupted(number, frame):
        raise KeyboardInterrupt(f'Received signal {number}')
    previous = {number: signal.signal(number, interrupted) for number in (signal.SIGTERM, signal.SIGHUP)}
    try:
        status = execute(args, output, Sanitizer())
    finally:
        for number, handler in previous.items():
            signal.signal(number, handler)
    print(f'Frozen aggregate {"accepted" if status == 0 else "rejected"}: {output / "aggregate-gate.json"}')
    return status


if __name__ == '__main__':
    sys.exit(main())
