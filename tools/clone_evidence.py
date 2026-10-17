#!/usr/bin/env python3
"""Closed, source-bound receipt contract for the scoped Clone regression run.

Pure validation is not native acceptance. Historical integration reports remain
immutable; a current receipt supplements them only after a fresh native run.
"""
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import subprocess

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
SCOPE = 'clone-component-visibility-normal-and-focused-sanitizers-v1'
TARGETS = ('gimp-clone-layer', 'painter-layer-tree', 'painter-xcf-roundtrip',
           'painter-layer-ui', 'update-geometry', 'projection-update-bounds', 'core')
INSTRUMENTED = frozenset({'app/core/gimpitem.c', 'app/core/gimpclonelayer.cpp',
    'app/core/gimpgrouplayer.c', 'app/tests/test-gimp-clone-layer.c',
    'app/tests/test-gimp-clone-layout.cpp', 'app/painter/binding-store.cpp',
    'app/painter/gimp-painter-binding.cpp', 'app/painter/gimp-painter-error.cpp'})
NEW_TESTS = tuple('/gimp-clone-layer/component_visibility_' + suffix for suffix in
    ('rgb', 'rgba', 'gray', 'gray_alpha', 'show_mask', 'projection_boundary'))
ENV_ALLOWLIST = frozenset({'PATH', 'LD_LIBRARY_PATH', 'PKG_CONFIG_PATH', 'GI_TYPELIB_PATH',
    'XDG_DATA_DIRS', 'GEGL_PATH', 'BABL_PATH', 'GIO_EXTRA_MODULES', 'DISPLAY', 'XAUTHORITY',
    'LANG', 'LC_ALL', 'LC_CTYPE', 'TZ', 'CC', 'CXX', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS',
    'LDFLAGS', 'PYTHONPATH', 'CPATH', 'LIBRARY_PATH', 'GETTEXTDATADIR', 'GETTEXTDATADIRS',
    'GIMP_DEPS_ROOT'})
PUBLIC_FILES = frozenset({'source-integrity.json', 'elf-integrity.json', 'freshness.json',
    'registered-tests.json', 'normal-results.json', 'sanitizers.json',
    'sanitizer-artifacts.json', 'locales.json', 'environment.json', 'default-build.log',
    'meson-console.log', 'testlog.txt', 'sanitizer-console.log', 'transforms.json',
    'baseline.json', 'compile-database.json'})
ENV_FIELDS = frozenset({'env', 'environment', 'inherited_environment'})


def load_module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    exec(compile(Path(path).read_bytes(), str(path), 'exec'), module.__dict__)
    return module


aggregate = load_module(ROOT / 'tools/run-painter-frozen-aggregate.py', 'clone_aggregate')
package = load_module(ROOT / 'tools/package-linux-runtime.py', 'clone_package')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return (json.dumps(value, indent=2, ensure_ascii=False) + '\n').encode()


def filtered_environment(inherited):
    """Retain only declared dependency/display inputs for isolated children."""
    return {key: value for key, value in inherited.items() if key in ENV_ALLOWLIST}


def strip_environment(value):
    if isinstance(value, dict):
        return {key: strip_environment(item) for key, item in value.items()
                if key.lower() not in ENV_FIELDS}
    if isinstance(value, list):
        return [strip_environment(item) for item in value]
    return value


def public_json(raw, jsonl=False):
    value = [json.loads(line) for line in raw.splitlines() if line.strip()] if jsonl else json.loads(raw)
    published = canonical(strip_environment(value))
    return published, {'transform': 'recursive-remove-env-fields-v1',
        'raw_sha256': digest(raw), 'public_sha256': digest(published),
        'public_semantics_sha256': digest(canonical(strip_environment(value))),
        'jsonl_input': jsonl}


def public_meson_text(raw):
    # Environment values may contain newlines, fake assignments and blank lines.
    # Remove the COMPLETE pre-test span, never a physical-line approximation.
    boundary = re.search(rb'^={3,} [1-9][0-9]*/[1-9][0-9]* ={3,}\r?$', raw, re.M)
    if boundary is None:
        raise ValueError('Meson text log has no first-test boundary')
    public = raw[boundary.start():]
    return public, {'transform': 'remove-complete-pre-test-span-v1',
        'raw_sha256': digest(raw), 'public_sha256': digest(public),
        'removed_start': 0, 'removed_end': boundary.start(),
        'removed_sha256': digest(raw[:boundary.start()]),
        'raw_bytes': len(raw), 'public_bytes': len(public),
        'unchanged_test_suffix_sha256': digest(public)}


def exact_tap(output, required=()):
    plans = re.findall(r'^1\.\.(\d+)\s*$', output, re.M)
    rows = re.findall(r'^ok (\d+) (/[\w./-]+)\s*$', output, re.M)
    if (len(plans) != 1 or int(plans[0]) <= 0 or len(rows) != int(plans[0]) or
            [int(number) for number, _ in rows] != list(range(1, len(rows) + 1)) or
            len({name for _, name in rows}) != len(rows)):
        return False
    if re.search(r'^\s*(?:not ok\b|Bail out!|(?:ok\b[^\n]*|\d+\.\.\d+[^\n]*)#\s*(?:SKIP|TODO))', output, re.M | re.I):
        return False
    return set(required).issubset({name for _, name in rows})


def clone_tests(root):
    names = []
    for name, macro in (('test-gimp-clone-layer.c', 'ADD'), ('test-gimp-clone-layout.cpp', 'ADD_HANDLE')):
        text = (Path(root) / 'app/tests' / name).read_text()
        names.extend('/gimp-clone-layer/' + test for test in
                     re.findall(r'\b' + macro + r'\s*\(\s*(\w+)\s*\)\s*;', text))
    if not set(NEW_TESTS).issubset(names) or len(names) != len(set(names)):
        raise ValueError('Missing or duplicate current Clone source test registrations')
    return names


def selected_gate(registry, results, required_clone):
    aggregate.validate_registry(registry)
    errors, matched = [], set()
    counts = Counter(row['name'] for row in registry)
    if any(counts[name] != 1 for name in TARGETS):
        errors.append('Missing or duplicate selected registry target')
    if len({aggregate.pretty_name(row, registry) for row in registry}) != len(registry):
        errors.append('Duplicate qualified registry names')
    for registered in registry:
        if registered['name'] not in TARGETS:
            continue
        indexes = [index for index, row in enumerate(results)
                   if row.get('name') == aggregate.pretty_name(registered, registry)
                   and row.get('command') == registered['cmd']]
        if len(indexes) != 1:
            errors.append('Missing/duplicate exact registered result: ' + registered['name'])
            continue
        index = indexes[0]
        matched.add(index)
        row = results[index]
        output = row.get('stdout', '') + '\n' + row.get('stderr', '')
        skipped, crashes, failures, _ = aggregate.diagnostics(registered['name'], output)
        required = required_clone if registered['name'] == 'gimp-clone-layer' else ()
        if (row.get('result') != 'OK' or type(row.get('returncode')) is not int or
                row['returncode'] != 0 or skipped or crashes or failures or
                not exact_tap(row.get('stdout', ''), required)):
            errors.append('Failed/skip/diagnostic/nonpositive TAP: ' + registered['name'])
        if registered['name'] == 'gimp-clone-layer' and len(re.findall(r'^ok \d+ ', row['stdout'], re.M)) != len(required_clone):
            errors.append('Clone TAP does not exactly cover current registrations')
    if matched != set(range(len(results))) or len(results) != len(TARGETS):
        errors.append('Unknown, duplicate or missing observed results')
    return errors



def source_commit_gate(root, state):
    errors = []
    if (state.get('clean') is not True or state.get('status') or
            state.get('hidden_index_entries') or state.get('committed_content_mismatches')):
        errors.append('Source snapshot is not independently clean')
    if aggregate.git(root, 'rev-parse', state['commit'] + '^{tree}') != state['tree']:
        errors.append('Source commit/tree mismatch')
    tree = aggregate.git(root, 'ls-tree', '-r', state['commit']).splitlines()
    files, modules = set(), {}
    for line in tree:
        metadata, name = line.split('\t', 1)
        mode, kind, oid = metadata.split()
        if kind == 'commit':
            modules[name] = oid
        else:
            files.add(name)
    if files != set(state['tracked_files']) or set(modules) != set(state['submodules']):
        errors.append('Source snapshot omits/adds committed files or submodules')
    for name, oid in modules.items():
        child = state['submodules'][name]
        if child['commit'] != oid or child['pinned_commit'] != oid:
            errors.append('Unpinned source submodule: ' + name)
        errors.extend(source_commit_gate(root / name, child))
        algorithm = aggregate.git(root / name, 'rev-parse', '--show-object-format')
        child_entries = {}
        for line in aggregate.git(root / name, 'ls-tree', '-r', child['commit']).splitlines():
            metadata, filename = line.split('\t', 1)
            mode, kind, blob = metadata.split()
            if kind != 'commit':
                child_entries[filename] = blob
        for filename, seal in child['tracked_files'].items():
            path = root / name / filename
            if package.path_fingerprint(path) != seal:
                errors.append('Current submodule file fingerprint changed: ' + name + '/' + filename)
            if (path.exists() or path.is_symlink()) and aggregate.git_blob_id(path, algorithm) != child_entries.get(filename):
                errors.append('Current submodule file is not the pinned committed blob: ' + name + '/' + filename)
        if aggregate.git(root / name, 'rev-parse', 'HEAD') != oid or aggregate.git(root / name, 'status', '--porcelain', '--untracked-files=all'):
            errors.append('Current pinned submodule differs: ' + name)
    return errors


def validate_receipt(directory, root=ROOT, expected_sha=None):
    """Recompute acceptance from sealed artifacts; never trust an all_passed flag."""
    directory, root = Path(directory), Path(root)
    errors = []
    result = {'errors': errors, 'sources_sha256': {}, 'normal_stdout': '', 'sanitizer_stdout': ''}
    try:
        receipt_path = directory / 'receipt.json'
        receipt_bytes = receipt_path.read_bytes()
        if expected_sha is not None and digest(receipt_bytes) != expected_sha:
            raise ValueError('Current receipt hash mismatch')
        receipt = json.loads(receipt_bytes)
        if receipt.get('schema') != 1 or receipt.get('scope') != SCOPE or receipt.get('status') != 'passed':
            raise ValueError('Missing, failed or wrong-scope current receipt')
        if set(receipt['files']) != PUBLIC_FILES or {p.name for p in directory.iterdir()} != PUBLIC_FILES | {'receipt.json'}:
            raise ValueError('Missing or unknown extra public outputs')
        for name in PUBLIC_FILES:
            path = directory / name
            if path.is_symlink() or not path.is_file() or digest(path.read_bytes()) != receipt['files'][name]:
                raise ValueError('Changed/missing sealed artifact: ' + name)
        def read(name):
            return json.loads((directory / name).read_text())
        execution_root = Path(receipt['source_directory'])
        if not execution_root.is_absolute() or '..' in execution_root.parts:
            raise ValueError('Invalid recorded execution source directory')
        source = read('source-integrity.json')
        before = source['before_build']
        if before != source['before_tests'] or before != source['after']:
            errors.append('Sources changed during build/run')
        aggregate.require_source(before, receipt['source_commit'], receipt['source_tree'])
        errors.extend(source_commit_gate(root, before))
        # Post-run evidence and task/contract narration can be published later;
        # no native claim is transferred to those later document edits.
        documentary = {'tasks.md', 'migration/progress.tsv', 'migration/contracts/clone-layer-port.md',
                       'migration/acceptance/clone.json'}
        committed_entries = {}
        for entry in aggregate.git(root, 'ls-tree', '-r', receipt['source_commit']).splitlines():
            metadata, name = entry.split('\t', 1)
            mode, kind, oid = metadata.split()
            if kind != 'commit':
                committed_entries[name] = (mode, oid)
        algorithm = aggregate.git(root, 'rev-parse', '--show-object-format')
        current_paths = set(aggregate.git(root, 'ls-files').splitlines())
        allowed_new = {p.relative_to(root).as_posix() for p in directory.iterdir()} if directory.is_relative_to(root) else set()
        if current_paths - set(before['tracked_files']) - set(before['submodules']) - allowed_new - documentary:
            errors.append('Current checkout has unsealed additional tracked sources')
        for name, seal in before['tracked_files'].items():
            if name in documentary:
                continue
            path = root / name
            if package.path_fingerprint(path) != seal:
                errors.append('Current source fingerprint differs from sealed run: ' + name)
            if name in committed_entries and (path.exists() or path.is_symlink()):
                mode, oid = committed_entries[name]
                if aggregate.git_blob_id(path, algorithm) != oid:
                    errors.append('Current input is not the original committed blob: ' + name)
                if mode != '120000' and oct(path.stat().st_mode & 0o777) != seal.get('mode'):
                    errors.append('Source mode differs from sealed run: ' + name)
            if 'sha256' in seal:
                if not path.is_file() or digest(path.read_bytes()) != seal['sha256']:
                    errors.append('Current source differs from sealed run: ' + name)
                result['sources_sha256'][name] = seal['sha256']
        freshness = read('freshness.json')
        if not freshness['before'] or freshness['before'] != freshness['after']:
            errors.append('Build freshness changed/missing')
        if freshness['before']['source_commit'] != receipt['source_commit']:
            errors.append('Freshness source commit mismatch')
        elf = read('elf-integrity.json')
        if not all(name in elf['before'] for name in aggregate.EXECUTABLES):
            errors.append('Missing full-build application ELF seals')
        for name, seal in elf['before'].items():
            if elf['after'].get(name) != seal:
                errors.append('Original normal-build ELF changed: ' + name)
        # Private profile aliases are the only allowed normal-build additions.
        for name in elf['after'].keys() - elf['before'].keys():
            seal = elf['after'][name]
            if (not name.startswith('.GIMP3-build-config-') or 'symlink' not in seal or
                    seal.get('content') not in elf['before'].values()):
                errors.append('Unknown added normal-build ELF: ' + name)
        registry = read('registered-tests.json')
        if strip_environment(registry) != registry:
            errors.append('Published registry contains environment fields')
        if freshness['before']['build_metadata']['meson-info/intro-tests.json']['sha256'] != receipt['raw_registry_sha256']:
            errors.append('Full registry is not bound to freshness')
        transforms = read('transforms.json')
        if set(transforms) != {'registered-tests.json', 'normal-results.json', 'sanitizers.json', 'testlog.txt', 'compile-database.json'}:
            errors.append('Missing transformation identities')
        for name, transform in transforms.items():
            if transform['public_sha256'] != receipt['files'][name]:
                errors.append('Transformation/public hash mismatch: ' + name)
            if name == 'testlog.txt':
                if (transform['transform'] != 'remove-complete-pre-test-span-v1' or
                        transform['removed_start'] != 0 or transform['removed_end'] < 0 or
                        transform['raw_bytes'] - transform['removed_end'] != transform['public_bytes'] or
                        transform['unchanged_test_suffix_sha256'] != receipt['files'][name]):
                    errors.append('Invalid complete-prefix text transformation')
            elif (transform['transform'] != 'recursive-remove-env-fields-v1' or
                  transform['public_semantics_sha256'] != receipt['files'][name]):
                errors.append('Invalid exact JSON environment removal')
        if transforms['registered-tests.json']['raw_sha256'] != receipt['raw_registry_sha256']:
            errors.append('Registry transformation is not source-bound')
        if transforms['compile-database.json']['raw_sha256'] != receipt['compile_database_sha256']:
            errors.append('Compile database transformation is not bound')
        required = clone_tests(root)
        normal = read('normal-results.json')
        errors.extend(selected_gate(registry, normal, required))
        for row in normal:
            if row.get('name') == aggregate.pretty_name(next(r for r in registry if r['name'] == 'gimp-clone-layer'), registry):
                result['normal_stdout'] = row['stdout']
        san = read('sanitizers.json')
        output = san.get('stdout', '') + '\n' + san.get('stderr', '')
        skipped, crashes, failures, _ = aggregate.diagnostics('gimp-clone-layer', output)
        if (san.get('sanitizers') != ['address', 'undefined'] or san.get('leak_detection') is not False or
                set(san['sources']) != INSTRUMENTED or len(san['sources']) != len(INSTRUMENTED) or
                san.get('exit_code') != 0 or skipped or crashes or failures or
                not exact_tap(san.get('stdout', ''), required) or
                len(re.findall(r'^ok \d+ ', san.get('stdout', ''), re.M)) != len(required)):
            errors.append('Focused sanitizer scope/execution/TAP is incomplete')
        if (len(san['executions']) != len(INSTRUMENTED) + 5 or
                san['commands'] != [row['command'] for row in san['executions']] or
                any(type(row['exit_code']) is not int or row['exit_code'] != 0 for row in san['executions']) or
                san['executions'][-1]['command'] != [san['executable']]):
            errors.append('Missing/failed sanitizer compile/link/run command receipts')
        artifacts = read('sanitizer-artifacts.json')
        expected = {Path(name).name + '.o' for name in INSTRUMENTED} | {'gimp-clone-layer-asan'}
        if set(artifacts['files']) != expected or artifacts['before_run'] != artifacts['after_run']:
            errors.append('Sanitizer output inventory or executable run seal changed')
        if (artifacts['files'] != artifacts['before_run'] or artifacts['files'] != artifacts['after_run'] or
                Path(artifacts['directory']).is_relative_to(Path(receipt['build_directory'])) or
                Path(artifacts['directory']).is_relative_to(execution_root) or
                san['executable'] != str(Path(artifacts['directory']) / 'gimp-clone-layer-asan')):
            errors.append('Sanitizer artifact identity/isolation mismatch')
        sanitizer_runner = load_module(root / 'migration/tests/run_clone_layer_sanitizers.py', 'clone_san_validation')
        entries, original_sources, replacements, extras = [], [], {}, []
        for entry in read('compile-database.json'):
            path = (Path(entry['directory']) / entry['file']).resolve()
            if path.is_relative_to(execution_root) and path.relative_to(execution_root).as_posix() in INSTRUMENTED:
                entries.append(entry)
                original_sources.append(path.relative_to(execution_root).as_posix())
        if original_sources != san['sources'] or len(entries) != len(INSTRUMENTED):
            errors.append('Instrumented inputs differ from full compile database')
        import shlex
        for entry, execution in zip(entries, san['executions']):
            path = (Path(entry['directory']) / entry['file']).resolve()
            obj = Path(artifacts['directory']) / (path.name + '.o')
            if execution['command'] != sanitizer_runner.compile_command(entry, obj):
                errors.append('Sanitizer compile command identity changed')
            original = list(entry.get('arguments') or shlex.split(entry['command']))
            if path.relative_to(execution_root).as_posix().startswith('app/tests/'):
                replacements[entry.get('output') or original[original.index('-o') + 1]] = str(obj)
            else:
                extras.append(str(obj))
        link_read = san['executions'][len(INSTRUMENTED)]
        if link_read['command'] != ['ninja', '-t', 'commands', 'app/tests/gimp-clone-layer']:
            errors.append('Sanitizer link discovery identity changed')
        link = sanitizer_runner.link_command(link_read['stdout'], Path(receipt['build_directory']),
                                             Path(san['executable']), replacements, extras, True)
        if san.get('origin_relocation') != 'expand-against-original-executable-directory':
            errors.append('Missing preserved original executable loader resolution')
        if link != san['executions'][len(INSTRUMENTED) + 1]['command']:
            errors.append('Sanitizer link command identity changed')
        normal_loader, sanitizer_loader = san['executions'][-3:-1]
        if (normal_loader['command'] != ['ldd', str(Path(receipt['build_directory']) / 'app/tests/gimp-clone-layer')] or
                sanitizer_loader['command'] != ['ldd', san['executable']]):
            errors.append('Missing exact normal/sanitizer loader checks')
        normal_libraries = sanitizer_runner.gimp_library_paths(normal_loader['stdout'])
        san_libraries = sanitizer_runner.gimp_library_paths(sanitizer_loader['stdout'])
        if normal_libraries != san_libraries or set(normal_libraries) != set(san['library_resolution']):
            errors.append('Normal/sanitizer libgimp loader resolutions disagree')
        for name, location in normal_libraries.items():
            path = Path(location)
            if not path.is_relative_to(Path(receipt['build_directory'])):
                errors.append('libgimp resolves outside the sealed current build: ' + name)
                continue
            fingerprint = elf['before'].get(path.relative_to(Path(receipt['build_directory'])).as_posix(), {})
            if san['library_resolution'][name] != {'path': location, 'sha256': fingerprint.get('sha256')}:
                errors.append('Loader resolved library is not the sealed normal-build ELF: ' + name)
        for execution in san['executions'][:len(INSTRUMENTED)]:
            argv = execution['command']
            if ('-fsanitize=address,undefined' not in argv or '-fno-omit-frame-pointer' not in argv or
                    Path(argv[argv.index('-o') + 1]).parent != Path(artifacts['directory'])):
                errors.append('Uninstrumented or non-isolated sanitizer compilation')
        locales = read('locales.json')
        if (locales['before'] != locales['after'] or locales['before']['catalog_count'] != 361 or
                len(locales['before']['catalogs']) != 361):
            errors.append('Incomplete/changed staged catalogs')
        environment = read('environment.json')
        if set(environment) - ENV_ALLOWLIST:
            errors.append('Unapproved inherited environment was published')
        commands = receipt['commands']
        build = receipt['build_directory']
        expected_meson = ['meson', 'test', '-C', build, '--no-rebuild', '--num-processes', '1',
                          '--print-errorlogs', '--logbase', receipt['raw_logbase'], *TARGETS]
        if (len(commands) != 3 or commands[0]['argv'] != ['ninja', '-C', build, '-j4'] or
                commands[1]['argv'] != expected_meson or
                commands[2]['argv'][1:] != ['-B', str(execution_root / 'migration/tests/run_clone_layer_sanitizers.py'), build,
                    '--report', str(Path(receipt['raw_logbase']).parent / 'sanitizers.json'),
                    '--output-dir', artifacts['directory']] or
                any(type(row['exit_code']) is not int or row['exit_code'] != 0 for row in commands)):
            errors.append('Default build or exact selected Meson execution was not successful')
        baseline = read('baseline.json')
        if baseline != json.loads((root / 'migration/tests/clone-component-baseline.json').read_text()) or [r['exit_code'] for r in baseline['receipt']['commands']] != [1, 1, 0]:
            errors.append('Original failed baseline attempts were not retained')
        result['sanitizer_stdout'] = san.get('stdout', '')
        result['receipt'] = receipt
    except (OSError, ValueError, KeyError, TypeError, RuntimeError, StopIteration, IndexError, subprocess.CalledProcessError) as error:
        errors.append('Invalid current Clone receipt: ' + str(error))
    return result
