#!/usr/bin/env python3
"""Pure mocked controls. No native arithmetic/runtime acceptance is performed."""
from copy import deepcopy
import json
import shutil
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from clone_evidence import (ROOT, TARGETS, INSTRUMENTED, NEW_TESTS, PUBLIC_FILES, SCOPE,
    aggregate, canonical, digest, exact_tap, selected_gate, public_json,
    public_meson_text, validate_receipt, load_module, filtered_environment)

sanrunner = load_module(ROOT / 'migration/tests/run_clone_layer_sanitizers.py', 'clone_san_controls')


def tap(names):
    return 'TAP version 14\n1..' + str(len(names)) + '\n' + ''.join(
        f'ok {i} {name}\n' for i, name in enumerate(names, 1))


def registry():
    return [{'name': name, 'suite': ['gimp:app'], 'cmd': ['/build/' + name] +
             (['-m', 'slow'] if name == 'painter-xcf-roundtrip' else [])} for name in TARGETS] + [
                {'name': 'unselected', 'suite': ['gimp:painter'], 'cmd': ['/unselected']}]


def results(rows):
    return [{'name': aggregate.pretty_name(row, rows), 'command': row['cmd'], 'result': 'OK',
             'returncode': 0, 'stdout': tap(NEW_TESTS if row['name'] == 'gimp-clone-layer' else ['/test/pass']),
             'stderr': ''} for row in rows if row['name'] in TARGETS]


class PrimitiveControls(unittest.TestCase):
    def test_required_dependency_root_reaches_isolated_child_only(self):
        dependency_root = '/workspace/shared/gimp-portrait-deps/root'
        inherited = {'GIMP_DEPS_ROOT': dependency_root, 'PATH': '/usr/bin',
                     'PYTHONPATH': '/workspace/shared/gimp-portrait-deps/python',
                     'ARBITRARY_SETTING': 'must-not-propagate', 'API_TOKEN': 'private-value'}
        filtered = filtered_environment(inherited)
        with tempfile.TemporaryDirectory() as directory:
            child = aggregate.isolated_child_environment(Path(directory), filtered)
        self.assertEqual(child['GIMP_DEPS_ROOT'], dependency_root)
        self.assertEqual(child['PYTHONPATH'], inherited['PYTHONPATH'])
        self.assertNotIn('ARBITRARY_SETTING', child)
        self.assertNotIn('API_TOKEN', child)
        self.assertEqual(filtered, {key: inherited[key] for key in ('GIMP_DEPS_ROOT', 'PATH', 'PYTHONPATH')})

    def test_complete_positive_tap(self):
        self.assertTrue(exact_tap(tap(NEW_TESTS), NEW_TESTS))

    def test_tap_rejects_false_pass_skip_and_duplicates(self):
        good = tap(NEW_TESTS)
        for bad in ('', '1..0\n', good.replace('ok 1 ', 'not ok 1 '), good + 'Bail out!\n',
                    good.replace('ok 1 ', 'ok 2 '), good + '1..6\n',
                    good.replace(NEW_TESTS[0], NEW_TESTS[1]),
                    good.replace(NEW_TESTS[0], NEW_TESTS[0] + ' # SKIP optional'),
                    good.replace('1..6', '1..6 # TODO later')):
            with self.subTest(bad=bad):
                self.assertFalse(exact_tap(bad, NEW_TESTS))

    def test_full_registry_namespace_and_arguments(self):
        rows = registry()
        self.assertEqual(selected_gate(rows, results(rows), NEW_TESTS), [])
        wrong = results(rows)
        wrong[0]['name'] = 'gimp-clone-layer'
        self.assertTrue(selected_gate(rows, wrong, NEW_TESTS))
        wrong = results(rows)
        wrong[2]['command'] = wrong[2]['command'][:-2]
        self.assertTrue(selected_gate(rows, wrong, NEW_TESTS))

    def test_missing_duplicate_invented_registry_and_results(self):
        rows = registry()
        for altered in (rows[1:], rows + [rows[0]], [dict(r, name='invented') if i == 0 else r for i, r in enumerate(rows)]):
            self.assertTrue(selected_gate(altered, results(rows), NEW_TESTS))
        for altered in (results(rows)[1:], results(rows) + [results(rows)[0]],
                        results(rows) + [{'name': 'invented', 'command': ['/no']} ]):
            self.assertTrue(selected_gate(rows, altered, NEW_TESTS))

    def test_zero_exit_diagnostics_and_missing_cases_rejected(self):
        rows = registry()
        for diagnostic in ('GLib-CRITICAL bad state', 'GIMP-Warning: unexpected', 'GIMP-Error: unexpected',
                           'ERROR: AddressSanitizer: failure', 'runtime error: overflow', 'SIGABRT'):
            runs = results(rows)
            runs[0]['stderr'] += diagnostic
            self.assertTrue(selected_gate(rows, runs, NEW_TESTS))
        runs = results(rows)
        runs[0]['stdout'] = tap(NEW_TESTS[:-1])
        self.assertTrue(selected_gate(rows, runs, NEW_TESTS))

    def test_multiline_environment_complete_span_and_suffix_identity(self):
        prefix = b"Log of Meson test suite\n\nInherited environment: SECRET='first\nnot_an_assignment\n\nlast' OTHER=foo\n"
        suffix = b'==================================== 1/7 =====================================\ntest: gimp-clone-layer\nstdout:\nok 1 /pass\n'
        raw = prefix + suffix
        public, transform = public_meson_text(raw)
        self.assertEqual(public, suffix)
        self.assertEqual(transform['removed_end'], len(prefix))
        self.assertEqual(transform['raw_sha256'], digest(raw))
        self.assertEqual(transform['unchanged_test_suffix_sha256'], digest(suffix))
        self.assertNotIn(b'SECRET', public)
        with self.assertRaises(ValueError):
            public_meson_text(prefix)

    def test_json_removes_only_environment_fields_exactly(self):
        value = {'env': {'SECRET': 'a\nb'}, 'stdout': 'ok 1 /pass\n',
                 'nested': [{'environment': {'key': 'value'}, 'result': 'OK'}]}
        public, metadata = public_json(json.dumps(value).encode())
        self.assertEqual(json.loads(public), {'stdout': value['stdout'], 'nested': [{'result': 'OK'}]})
        self.assertEqual(metadata['public_semantics_sha256'], digest(public))

    def test_compile_output_isolated_original_arguments_preserved(self):
        entry = {'file': 'unit.c', 'command': 'cc -DKEEP=1 -MMD -MF old.d -MTold.o -c unit.c -o normal.o'}
        cmd = sanrunner.compile_command(entry, Path('/private/unit.o'))
        self.assertEqual(cmd, ['cc', '-DKEEP=1', '-c', 'unit.c', '-o', '/private/unit.o'] + sanrunner.FLAGS)
        self.assertEqual(entry['command'], 'cc -DKEEP=1 -MMD -MF old.d -MTold.o -c unit.c -o normal.o')
        self.assertEqual(sanrunner.output_paths(Path('/build'), Path('/private')),
                         (Path('/private'), Path('/private/gimp-clone-layer-asan')))
        self.assertEqual(sanrunner.output_paths(Path('/build'), None),
                         (Path('/build/clone-sanitizers'), Path('/build/app/tests/gimp-clone-layer-asan')))


    def test_relocated_link_preserves_original_origin_semantics(self):
        original = "c++ unit.o -o app/tests/gimp-clone-layer '-Wl,-rpath,$ORIGIN/../../libgimpbase:${ORIGIN}/../../libgimpcolor:$LIB'"
        relocated = sanrunner.link_command(original, Path('/build'), Path('/private/asan'), {}, [], True)
        self.assertIn('-Wl,-rpath,/build/app/tests/../../libgimpbase:/build/app/tests/../../libgimpcolor:$LIB', relocated)
        default = sanrunner.link_command(original, Path('/build'), Path('/build/app/tests/gimp-clone-layer-asan'), {}, [], False)
        self.assertIn('-Wl,-rpath,$ORIGIN/../../libgimpbase:${ORIGIN}/../../libgimpcolor:$LIB', default)
        self.assertEqual(relocated[relocated.index('-o') + 1], '/private/asan')

    def test_loader_requires_exact_current_build_libraries(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            lib = build / 'libgimpbase-3.0.so.0'; lib.write_bytes(b'mocked ELF seal')
            line = 'libgimpbase-3.0.so.0 => ' + str(lib) + ' (0x1234)\n'
            self.assertEqual(sanrunner.library_resolution(line, line, build)['libgimpbase-3.0.so.0']['sha256'], digest(lib.read_bytes()))
            for wrong in ('libgimpbase-3.0.so.0 => not found\n',
                          'libgimpbase-3.0.so.0 => /prefix/libgimpbase-3.0.so.0 (0x1234)\n'):
                with self.assertRaises(RuntimeError):
                    sanrunner.library_resolution(line, wrong, build)

    def test_failed_compile_preserves_exit_diagnostics_and_normal_object(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            build = base / 'build'; build.mkdir()
            normal = build / 'normal.o'; normal.write_bytes(b'normal sentinel')
            entries = [{'directory': str(build), 'file': str(ROOT / source),
                        'command': 'cc -c ' + str(ROOT / source) + ' -o ' + Path(source).name + '.o'}
                       for source in sorted(INSTRUMENTED)]
            (build / 'compile_commands.json').write_text(json.dumps(entries))
            report = base / 'report.json'
            discovery = subprocess.CompletedProcess([], 0, 'c++ -o app/tests/gimp-clone-layer ' +
                ' '.join(Path(source).name + '.o' for source in sorted(INSTRUMENTED)), '')
            failed = subprocess.CompletedProcess([], 7, 'failed compile output', 'deliberate failure')
            with patch.object(sanrunner.subprocess, 'run', side_effect=[discovery, failed]):
                with patch('sys.stdout'), patch('sys.stderr'):
                    status = sanrunner.main([str(build), '--report', str(report), '--output-dir', str(base / 'isolated')])
            data = json.loads(report.read_text())
            self.assertEqual(status, 7)
            self.assertEqual(data['executions'][1]['exit_code'], 7)
            self.assertEqual(data['executions'][1]['stderr'], 'deliberate failure')
            self.assertEqual(normal.read_bytes(), b'normal sentinel')
            self.assertIn(str(base / 'isolated' / sanrunner.object_name(sorted(INSTRUMENTED)[0])), data['executions'][1]['command'])



class ReceiptControls(unittest.TestCase):
    """Synthetic sealed objects exercise the validator; never run a native binary."""
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.base = Path(self.temporary.name)
        self.root, self.output = self.base / 'source', self.base / 'evidence'
        self.root.mkdir(); self.output.mkdir()
        self.rtti_source = 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'
        for source in set(INSTRUMENTED) | {self.rtti_source}:
            path = self.root / source
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('/* mocked control input */\n')
        (self.root / 'app/tests/test-gimp-clone-layer.c').write_text(''.join('ADD (' + n.rsplit('/', 1)[1] + ');\n' for n in NEW_TESTS))
        path = self.root / 'migration/tests/run_clone_layer_sanitizers.py'
        path.parent.mkdir(parents=True)
        path.write_bytes((ROOT / 'migration/tests/run_clone_layer_sanitizers.py').read_bytes())
        (path.parent / 'painter_sanitizer_scope.py').write_bytes((ROOT / 'migration/tests/painter_sanitizer_scope.py').read_bytes())
        baseline_path = self.root / 'migration/tests/clone-component-baseline.json'
        baseline_path.write_bytes((ROOT / 'migration/tests/clone-component-baseline.json').read_bytes())
        def git(*args):
            return subprocess.check_output(['git', '-C', str(self.root), *args], stderr=subprocess.DEVNULL).decode().strip()
        git('init'); git('add', '.')
        git('-c', 'user.name=Control', '-c', 'user.email=control@example.invalid', 'commit', '-m', 'Synthetic receipt control')
        self.commit, self.tree = git('rev-parse', 'HEAD'), git('rev-parse', 'HEAD^{tree}')
        files = {name: {'sha256': digest((self.root / name).read_bytes()), 'bytes': (self.root / name).stat().st_size, 'mode': '0o644'} for name in git('ls-files').splitlines()}
        source = {'commit': self.commit, 'tree': self.tree, 'clean': True, 'status': '', 'hidden_index_entries': [],
                  'committed_content_mismatches': [], 'tracked_files': files, 'submodules': {}}
        self.rows = registry()
        self.transforms = {}
        self.write('source-integrity.json', {'before_build': source, 'before_tests': source, 'after': source})
        elf = {name: {'sha256': 'a' * 64, 'bytes': 32, 'mode': '0o755'} for name in aggregate.EXECUTABLES}
        elf['libgimpbase/libgimpbase-3.0.so.0'] = {'sha256': 'a' * 64, 'bytes': 32, 'mode': '0o755'}
        self.write('elf-integrity.json', {'before': elf, 'after': elf})
        raw_registry = canonical(self.rows)
        self.transformed('registered-tests.json', raw_registry)
        self.transformed('normal-results.json', b'\n'.join(json.dumps(row).encode() for row in results(self.rows)), True)
        rawtext = b"Inherited environment: SECRET='multi\nline'\n==================================== 1/7 =====================================\n" + tap(NEW_TESTS).encode()
        public, self.transforms['testlog.txt'] = public_meson_text(rawtext)
        (self.output / 'testlog.txt').write_bytes(public)
        build, private = str(self.base / 'build'), self.base / 'private'
        instrumented = private / 'instrumented'
        entries, members, direct = [], [], []
        for source_path in sorted(set(INSTRUMENTED) | {self.rtti_source}):
            name = Path(source_path).name
            original_object = 'normal-' + name + '.o'
            flags = '-fno-rtti ' if source_path.endswith('.cpp') else ''
            entry = {'directory': build, 'file': str(self.root / source_path),
                     'command': 'cc ' + flags + '-c ' + str(self.root / source_path) + ' -o ' + original_object}
            entries.append(entry)
            elf[original_object] = {'sha256': 'a' * 64, 'bytes': 32, 'mode': '0o644'}
            if source_path.startswith('app/tests/'):
                direct.append(original_object)
            else:
                members.append(str(Path(build) / original_object))
        # A standalone duplicate of a production source must never displace its archive member.
        entries.append(dict(entries[-1], command=entries[-1]['command'].rsplit(' -o ', 1)[0] + ' -o standalone-copy.o'))
        self.write('elf-integrity.json', {'before': elf, 'after': elf})
        compile_raw = canonical(entries)
        self.transformed('compile-database.json', compile_raw)
        link_text = ' '.join(['c++', '-o', 'app/tests/gimp-clone-layer', *direct, 'libnormal.a'])
        plan = sanrunner.build_plan(self.root, Path(build), instrumented, entries, link_text, {'libnormal.a': members})
        exe = str(instrumented / 'gimp-clone-layer-asan')
        link = sanrunner.link_command(link_text, Path(build), Path(exe), plan['replacements'], plan['instrumented_anchors'], True)
        executions = [
            {'command': ['ninja', '-t', 'commands', 'app/tests/gimp-clone-layer'], 'exit_code': 0, 'stdout': link_text, 'stderr': ''},
            {'command': ['ar', 't', 'libnormal.a'], 'exit_code': 0, 'stdout': '\n'.join(members), 'stderr': ''}]
        for command in [job['command'] for job in plan['compilations']] + [archive['command'] for archive in plan['archives'] if archive['command']] + [link]:
            executions.append({'command': command, 'exit_code': 0, 'stdout': '', 'stderr': ''})
        library = build + '/libgimpbase/libgimpbase-3.0.so.0'
        loader_output = 'libgimpbase-3.0.so.0 => ' + library + ' (0x1234)\n'
        executions.append({'command': ['ldd', build + '/app/tests/gimp-clone-layer'], 'exit_code': 0, 'stdout': loader_output, 'stderr': ''})
        executions.append({'command': ['ldd', exe], 'exit_code': 0, 'stdout': loader_output, 'stderr': ''})
        executions.append({'command': [exe], 'exit_code': 0, 'stdout': tap(NEW_TESTS), 'stderr': ''})
        san = {'sanitizers': ['address', 'undefined'], 'leak_detection': False, 'sources': sorted(INSTRUMENTED),
               'exit_code': 0, 'stdout': tap(NEW_TESTS), 'stderr': '', 'commands': [r['command'] for r in executions],
               'executions': executions, 'executable': exe, 'origin_relocation': 'expand-against-original-executable-directory',
               'library_resolution': {'libgimpbase-3.0.so.0': {'path': library, 'sha256': 'a' * 64}}}
        san.update(build_plan=plan, rtti_compatibility_only_sources=plan['rtti_compatibility_only_sources'],
                   original_archive_sha256_before={'libnormal.a': 'c' * 64}, original_archive_sha256_after={'libnormal.a': 'c' * 64})
        self.transformed('sanitizers.json', canonical(san))
        outputs = {sanrunner.object_name(p): {'sha256': 'a' * 64, 'bytes': 32, 'mode': '0o644'} for p in set(INSTRUMENTED) | {self.rtti_source}}
        outputs['archive-libnormal.a'] = {'sha256': 'd' * 64, 'bytes': 32, 'mode': '0o644'}
        outputs['gimp-clone-layer-asan'] = {'sha256': 'a' * 64, 'bytes': 32, 'mode': '0o755'}
        self.write('sanitizer-artifacts.json', {'directory': str(instrumented), 'files': outputs, 'before_run': outputs, 'after_run': outputs})
        catalogs = {'catalog_count': 361, 'catalogs': {str(i): {'sha256': 'a' * 64} for i in range(361)}}
        self.write('locales.json', {'before': catalogs, 'after': catalogs})
        fresh = {'source_commit': self.commit, 'build_outputs': {'libnormal.a': {'sha256': 'c' * 64}}, 'build_metadata': {'meson-info/intro-tests.json': {'sha256': digest(raw_registry)}}}
        self.write('freshness.json', {'before': fresh, 'after': fresh})
        self.write('environment.json', {'DISPLAY': 'localhost:42'})
        self.write('baseline.json', json.loads(baseline_path.read_text()))
        for name in ('default-build.log', 'meson-console.log', 'sanitizer-console.log'):
            (self.output / name).write_text('mocked control only\n')
        self.write('transforms.json', self.transforms)
        self.receipt = {'schema': 1, 'scope': SCOPE, 'status': 'passed', 'source_commit': self.commit,
            'source_tree': self.tree, 'source_directory': str(self.root), 'build_directory': build, 'raw_registry_sha256': digest(raw_registry),
            'compile_database_sha256': digest(compile_raw), 'raw_logbase': str(private / 'meson'),
            'commands': [{'argv': ['ninja', '-C', build, '-j4'], 'exit_code': 0},
                         {'argv': ['meson', 'test', '-C', build, '--no-rebuild', '--num-processes', '1', '--print-errorlogs', '--logbase', str(private / 'meson'), *TARGETS], 'exit_code': 0},
                         {'argv': ['python3', '-B', str(path), build, '--report', str(private / 'sanitizers.json'), '--output-dir', str(instrumented)], 'exit_code': 0}]}
        self.seal()

    def tearDown(self):
        self.temporary.cleanup()

    def write(self, name, value):
        (self.output / name).write_bytes(canonical(value))

    def transformed(self, name, raw, jsonl=False):
        public, self.transforms[name] = public_json(raw, jsonl)
        (self.output / name).write_bytes(public)

    def seal(self):
        self.receipt['files'] = {name: digest((self.output / name).read_bytes()) for name in PUBLIC_FILES}
        self.write('receipt.json', self.receipt)

    def validate(self):
        return validate_receipt(self.output, self.root)['errors']

    def test_valid_mocked_receipt(self):
        self.assertEqual(self.validate(), [])

    def test_relocated_checkout_preserves_execution_command_identity(self):
        original = self.root
        relocated = self.base / 'relocated-checkout'
        shutil.move(str(original), str(relocated))
        self.root = relocated
        self.assertFalse(original.exists())
        self.assertEqual(self.receipt['source_directory'], str(original))
        self.assertEqual(self.validate(), [])

    def test_tampered_execution_source_directory_rejected(self):
        self.receipt['source_directory'] = str(self.base / 'invented-execution-root')
        self.seal()
        self.assertTrue(self.validate())

    def test_tampered_receipt_pin(self):
        pinned = digest((self.output / 'receipt.json').read_bytes())
        self.receipt['status'] = 'invented'; self.seal()
        self.assertTrue(validate_receipt(self.output, self.root, pinned)['errors'])

    def test_tampered_artifact_without_resealing(self):
        (self.output / 'normal-results.json').write_text('[]')
        self.assertTrue(self.validate())

    def test_unknown_extra_output(self):
        (self.output / 'unexpected.log').write_text('anything')
        self.assertTrue(self.validate())

    def test_stale_source(self):
        (self.root / 'app/core/gimpclonelayer.cpp').write_text('changed')
        self.assertTrue(self.validate())

    def test_changed_source_resealed_under_old_commit(self):
        path = self.root / 'app/core/gimpclonelayer.cpp'
        path.write_text('changed source relabelled as original checkpoint')
        data = json.loads((self.output / 'source-integrity.json').read_text())
        for state in data.values():
            state['tracked_files']['app/core/gimpclonelayer.cpp'].update(sha256=digest(path.read_bytes()), bytes=path.stat().st_size)
        self.write('source-integrity.json', data); self.seal()
        self.assertTrue(any('original committed blob' in e for e in self.validate()))

    def test_forged_source_mode(self):
        data = json.loads((self.output / 'source-integrity.json').read_text())
        for state in data.values():
            state['tracked_files']['app/core/gimpclonelayer.cpp']['mode'] = '0o755'
        self.write('source-integrity.json', data); self.seal()
        self.assertTrue(self.validate())

    def test_unsealed_source_inventory(self):
        data = json.loads((self.output / 'source-integrity.json').read_text())
        for state in data.values():
            state['tracked_files'].pop('app/core/gimpclonelayer.cpp')
        self.write('source-integrity.json', data); self.seal()
        self.assertTrue(self.validate())

    def test_false_success_diagnostics_even_after_reseal(self):
        data = results(self.rows); data[0]['stderr'] = 'GLib-CRITICAL unexpected'
        self.transformed('normal-results.json', canonical(data))
        self.write('transforms.json', self.transforms); self.seal()
        self.assertTrue(self.validate())

    def test_uninstrumented_sanitizer_command(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        index = next(i for i, argv in enumerate(data['commands']) if '-fsanitize=address,undefined' in argv)
        data['commands'][index].remove('-fsanitize=address,undefined')
        data['executions'][index]['command'] = data['commands'][index]
        self.transformed('sanitizers.json', canonical(data)); self.write('transforms.json', self.transforms); self.seal()
        self.assertTrue(self.validate())

    def test_instrumented_and_rtti_only_coverage_stay_distinct(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        plan = data['build_plan']
        self.assertEqual(plan['instrumented_sources'], sorted(INSTRUMENTED))
        self.assertEqual(plan['rtti_compatibility_only_sources'], [self.rtti_source])
        for job in plan['compilations']:
            if job['instrumented']:
                self.assertIn('-fsanitize=address,undefined', job['command'])
            else:
                self.assertNotIn('-fsanitize=address,undefined', job['command'])
            if job['source'].endswith('.cpp'):
                self.assertEqual(job['command'][-1], '-frtti')
        original_objects = {job['original_object'] for job in plan['compilations']}
        for archive in plan['archives']:
            self.assertFalse(original_objects.intersection(archive['replacement_members']))
        self.assertFalse(any('standalone-copy.o' in job['original_object'] for job in plan['compilations']))

    def test_focused_anchors_precede_archives_without_duplicate_copies(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        plan = data['build_plan']
        link = data['executions'][-4]['command']
        self.assertEqual(len(plan['instrumented_anchors']), 8)
        first_archive = min(link.index(row['replacement']) for row in plan['archives'] if row['replacement'])
        focused_originals = {job['original_object'] for job in plan['compilations'] if job['instrumented']}
        for anchor in plan['instrumented_anchors']:
            self.assertEqual(link.count(anchor), 1)
            self.assertLess(link.index(anchor), first_archive)
        for archive in plan['archives']:
            self.assertFalse(focused_originals.intersection(archive['replacement_members']))
            self.assertFalse(set(plan['instrumented_anchors']).intersection(archive['replacement_members']))

    def test_missing_or_ambiguous_original_compilations_rejected(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        entries = json.loads((self.output / 'compile-database.json').read_text())
        plan = data['build_plan']
        args = (Path(self.receipt['source_directory']), Path(self.receipt['build_directory']),
                Path(data['executable']).parent)
        link_text = data['executions'][0]['stdout']
        members = {item['argument']: item['members'] for item in plan['archives']}
        for altered in (entries[1:], entries + [entries[0]]):
            with self.assertRaises(RuntimeError):
                sanrunner.build_plan(*args, altered, link_text, members)
        duplicated = deepcopy(members)
        duplicated['libnormal.a'].append(duplicated['libnormal.a'][0])
        with self.assertRaises(RuntimeError):
            sanrunner.build_plan(*args, entries, link_text, duplicated)

    def test_every_original_archive_alias_is_replaced_without_forcing_objects(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        entries = json.loads((self.output / 'compile-database.json').read_text())
        membership = data['build_plan']['archives'][0]['members']
        text = data['executions'][0]['stdout'] + ' second-alias.a'
        plan = sanrunner.build_plan(Path(self.receipt['source_directory']), Path(self.receipt['build_directory']),
            Path(data['executable']).parent, entries, text,
            {'libnormal.a': membership, 'second-alias.a': membership})
        self.assertEqual(len([row for row in plan['archives'] if row['replacement']]), 2)
        link = sanrunner.link_command(text, Path(self.receipt['build_directory']), Path(data['executable']), plan['replacements'], plan['instrumented_anchors'], True)
        rtti_object = next(job['object'] for job in plan['compilations'] if not job['instrumented'])
        self.assertNotIn(rtti_object, link)
        self.assertNotIn('libnormal.a', link)
        self.assertNotIn('second-alias.a', link)

    def test_falsely_claimed_rtti_instrumentation_rejected(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        data['sources'].append(self.rtti_source)
        self.transformed('sanitizers.json', canonical(data)); self.write('transforms.json', self.transforms); self.seal()
        self.assertTrue(self.validate())

    def test_altered_private_archive_replacements_rejected(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        record = data['build_plan']['archives'][0]
        record['replacement_members'][0] = record['members'][0]
        self.transformed('sanitizers.json', canonical(data)); self.write('transforms.json', self.transforms); self.seal()
        self.assertTrue(self.validate())

    def test_original_archive_mutation_rejected(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        data['original_archive_sha256_after']['libnormal.a'] = 'd' * 64
        self.transformed('sanitizers.json', canonical(data)); self.write('transforms.json', self.transforms); self.seal()
        self.assertTrue(self.validate())

    def test_changed_build_and_catalog_seals(self):
        data = json.loads((self.output / 'freshness.json').read_text())
        data['after']['source_commit'] = 'b' * 40
        self.write('freshness.json', data); self.seal()
        self.assertTrue(self.validate())

    def test_changed_original_normal_objects(self):
        data = json.loads((self.output / 'elf-integrity.json').read_text())
        data['after'][aggregate.EXECUTABLES[0]]['sha256'] = 'b' * 64
        self.write('elf-integrity.json', data); self.seal()
        self.assertTrue(self.validate())

    def test_sanitizer_unknown_outputs_and_lsan_claim(self):
        data = json.loads((self.output / 'sanitizer-artifacts.json').read_text())
        data['files']['unexpected.o'] = {'sha256': 'b' * 64}
        self.write('sanitizer-artifacts.json', data); self.seal()
        self.assertTrue(self.validate())

    def test_false_skip_in_focused_sanitizers(self):
        data = json.loads((self.output / 'sanitizers.json').read_text())
        data['stdout'] = data['stdout'].replace(NEW_TESTS[0], NEW_TESTS[0] + ' # SKIP')
        self.transformed('sanitizers.json', canonical(data)); self.write('transforms.json', self.transforms); self.seal()
        self.assertTrue(self.validate())

    def test_baseline_failures_cannot_be_erased(self):
        data = json.loads((self.output / 'baseline.json').read_text())
        data['receipt']['commands'][0]['exit_code'] = 0
        self.write('baseline.json', data); self.seal()
        self.assertTrue(self.validate())

    def test_weakened_selected_arguments(self):
        self.receipt['commands'][1]['argv'].append('--suite=app'); self.seal()
        self.assertTrue(self.validate())

    def test_failed_build_cannot_be_relabelled(self):
        self.receipt['commands'][0]['exit_code'] = 1; self.seal()
        self.assertTrue(self.validate())


if __name__ == '__main__':
    unittest.main()
