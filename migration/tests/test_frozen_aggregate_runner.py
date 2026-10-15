#!/usr/bin/env python3
"""Strict aggregate runner fixtures; never build or run the native application."""
import argparse
import copy
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


runner = load('frozen_aggregate', REPO / 'tools/run-painter-frozen-aggregate.py')
package = load('package_fixture', REPO / 'tools/package-linux-runtime.py')


def registry():
    return [
        {'name': 'same-name', 'cmd': ['/fixture/first'], 'suite': ['gimp:app', 'gimp:fixture']},
        {'name': 'same-name', 'cmd': ['/fixture/second'], 'suite': ['gimp:painter']},
    ]


def results(rows):
    return [{'name': runner.pretty_name(row, rows), 'command': row['cmd'], 'result': 'OK',
             'returncode': 0, 'stdout': 'TAP version 14\n1..1\nok 1 /fixture\n', 'stderr': ''}
            for row in rows]


class CoverageTests(unittest.TestCase):
    def setUp(self):
        self.registry = registry()
        self.results = results(self.registry)

    def test_duplicate_names_multiple_suites_and_reordered_results(self):
        gate = runner.coverage_gate(self.registry, list(reversed(self.results)))
        self.assertTrue(gate['coverage_complete'])
        self.assertEqual(gate['targets'][0]['registry_index'], 1)
        self.assertEqual(gate['targets'][0]['actual_command'], ['/fixture/first'])
        self.assertEqual(gate['targets'][0]['suites'], ['gimp:app', 'gimp:fixture'])
        self.assertEqual(gate['suite_counts']['gimp:fixture'], {'registered': 1, 'outcomes': {'OK': 1}})
        self.assertTrue(all(row['classification'] == 'passed' for row in gate['targets']))

    def test_missing_duplicate_extra_and_wrong_command_fail_coverage(self):
        wrong_command = copy.deepcopy(self.results)
        wrong_command[0]['command'] = ['/wrapper', '/fixture/first']
        wrong_suite = copy.deepcopy(self.results)
        wrong_suite[0]['name'] = 'gimp:wrong / same-name'
        for result in (self.results[:1], self.results + self.results[:1], wrong_command, wrong_suite, []):
            with self.subTest(result=result):
                gate = runner.coverage_gate(self.registry, result)
                self.assertFalse(gate['coverage_complete'])

    def test_empty_registry_is_not_a_passing_aggregate(self):
        self.assertFalse(runner.coverage_gate([], [])['coverage_complete'])

    def test_registry_shape_is_validated_before_running_meson(self):
        runner.validate_registry(self.registry)
        for invalid in ([], {}, [{'name': 'fixture', 'cmd': 'shell string', 'suite': []}],
                        [{'name': 'fixture', 'cmd': ['test'], 'suite': 'wrong'}]):
            with self.subTest(invalid=invalid), self.assertRaises(RuntimeError):
                runner.validate_registry(invalid)

    def test_ambiguous_identical_entries_are_not_matched_by_position(self):
        self.registry[1] = copy.deepcopy(self.registry[0])
        gate = runner.coverage_gate(self.registry, results(self.registry))
        self.assertFalse(gate['coverage_complete'])
        self.assertEqual(len(gate['unmatched_results']), 2)

    def test_non_ok_and_nonzero_never_pass(self):
        for result, code in [('FAIL', 1), ('SKIP', 0), ('TIMEOUT', -9), ('EXPECTEDFAIL', 0),
                             ('OK', -11), ('OK', True), ('OK', '0')]:
            with self.subTest(result=result, code=code):
                self.results[0].update(result=result, returncode=code)
                gate = runner.coverage_gate(self.registry, self.results)
                self.assertTrue(gate['coverage_complete'])
                self.assertEqual(gate['targets'][0]['classification'], 'failed')

    def test_skipped_tap_with_meson_ok_still_fails(self):
        for output in ('ok 35 /native-large # SKIP needs slow mode', '1..0 # SKIP no display',
                       'not ok 2 /later # TODO implement', '  ok 1 # skip nested'):
            with self.subTest(output=output):
                self.results[0]['stdout'] = output
                target = runner.coverage_gate(self.registry, self.results)['targets'][0]
                self.assertEqual(target['classification'], 'failed')
                self.assertEqual(target['tap_skipped_subtests'], [output])

    def test_skipping_unknown_plugin_is_not_a_tap_skip(self):
        self.results[0]['stderr'] = "Skipping unknown file '/build/plug-in.dummy' in plug-in directory."
        self.assertEqual(runner.coverage_gate(self.registry, self.results)['targets'][0]['classification'], 'passed')

    def test_crashes_and_failure_diagnostics_cannot_hide_behind_ok(self):
        for output in ('script-fu: fatal error: Segmentation fault', 'Traceback (most recent call last):',
                       'AddressSanitizer: heap-use-after-free', '(host): GLib-GObject-CRITICAL **: invalid object',
                       'GEGL-WARNING: tile-backend-swap writer queue was not empty before freeing',
                       'assertion foo == bar failed', 'runtime check failed: (gap_list->start == 0)',
                       'runtime error: index out of bounds', 'not ok 12 /fixture', 'Bail out! broken',
                       'GIMP-Warning: Failed to write user data', 'GIMP-Error: startup failed',
                       'Gimp-Paint-WARNING **: unexpected state', '** WARNING **: fixture warning',
                       'New-Library-WARNING: unexpected state', 'WARNING: unexpected state'):
            with self.subTest(output=output):
                self.results[0]['stderr'] = output
                self.assertEqual(runner.coverage_gate(self.registry, self.results)['targets'][0]['classification'], 'failed')

    def test_negative_warnings_are_preserved_only_for_their_test(self):
        output = 'GIMP-Warning: Invalid Fill Brush coordinates.'
        skipped, crashes, failures, nonfatal = runner.diagnostics('painter-fill-brush-ui', output)
        self.assertEqual((skipped, crashes, failures), ([], [], []))
        self.assertEqual(nonfatal, [output])
        self.assertEqual(runner.diagnostics('unrelated-test', output)[2], [output])
        self.assertTrue(runner.diagnostics('painter-fill-brush-ui', 'GIMP-Warning: Unexpected problem')[2])

    def test_welcome_warning_does_not_blanket_allow_gimp_warnings(self):
        self.assertEqual(runner.diagnostics('color-parser', 'GIMP-Warning: Welcome to GIMP 3.0.9!')[2], [])
        self.assertTrue(runner.diagnostics('color-parser', 'GIMP-Warning: Welcome to GIMP 3.0.9!\nGIMP-Warning: broken')[2])

    def test_intentional_negative_errors_are_narrow_and_remain_visible(self):
        for name, output in (
                ('painter-xcf-adversarial', 'GIMP-Error: not enough tiles found in level'),
                ('selection-float', "GIMP-Error: Execution error for procedure 'gimp-selection-float':")):
            self.assertEqual(runner.diagnostics(name, output)[2], [])
            self.assertEqual(runner.diagnostics(name, output)[3], [output])
            self.assertEqual(runner.diagnostics('unrelated', output)[2], [output])
        self.assertTrue(runner.diagnostics('selection-float', 'GIMP-Error: Failed to save data:')[2])


class SanitizationTests(unittest.TestCase):
    def test_structured_env_objects_are_never_published(self):
        sanitizer = runner.Sanitizer({'API_TOKEN': 'very-secret-value'})
        raw = {'env': {'API_TOKEN': 'very-secret-value', 'HOME': '/private/home'},
               'nested': [{'environment': {'OTHER': 'value'}, 'stdout': 'echo very-secret-value'}]}
        output = sanitizer.object(raw)
        self.assertEqual(output, {'nested': [{'stdout': 'echo [REDACTED]'}]})
        self.assertNotIn('/private/home', json.dumps(output))

    def test_environment_dumps_and_credentials_are_removed_from_text(self):
        sanitizer = runner.Sanitizer({})
        raw = ('Inherited environment: {"HOME": "/private"}\nHOME=/private\n'
               'TOKEN=unknown /fixture\nexport OTHER=value\n'
               'Authorization: Bearer secret-credential\n'
               'failure token=secret-value\nhttps://name:password@host/path\n'
               'GEGL-WARNING: queue was not empty\n')
        output = sanitizer.text(raw)
        for secret in ('/private', 'unknown', 'secret-credential', 'secret-value', 'name:password'):
            self.assertNotIn(secret, output)
        self.assertIn('GEGL-WARNING: queue was not empty', output)

    def test_multiline_and_command_option_credentials_are_redacted(self):
        sanitizer = runner.Sanitizer({'PRIVATE_KEY': 'private\nmultiline\nkey'})
        self.assertEqual(sanitizer.text('failure private\nmultiline\nkey'), 'failure [REDACTED]')
        self.assertEqual(sanitizer.text('runner --token value-not-in-env'), 'runner --token [REDACTED]')
        self.assertEqual(sanitizer.object(['runner', '--token', 'value-not-in-env']),
                         ['runner', '--token', '[REDACTED]'])

    def test_short_passwords_and_access_keys_are_redacted_without_treating_flags_as_secrets(self):
        sanitizer = runner.Sanitizer({'PASSWORD': 'abc123', 'AWS_ACCESS_KEY_ID': 'ABCDEFGH0123456789',
                                     'ENABLE_BROWSER_CREDENTIAL_CAPABILITIES': 'true'})
        self.assertEqual(sanitizer.text('echo abc123 / echo ABCDEFGH0123456789 / true'),
                         'echo [REDACTED] / echo [REDACTED] / true')

    def test_registry_archive_uses_allowlist_and_raw_hash_is_separate(self):
        rows = registry()
        rows[0]['env'] = {'SECRET_TOKEN': 'never archive', 'HOME': '/private'}
        rows[0]['unexpected'] = {'private': 'do not copy unknown fields'}
        archived = runner.registry_archive(rows)
        self.assertEqual(archived[0]['registry_index'], 1)
        self.assertNotIn('env', archived[0])
        self.assertNotIn('unexpected', archived[0])
        self.assertNotEqual(hashlib.sha256(json.dumps(rows).encode()).hexdigest(),
                            hashlib.sha256(json.dumps(archived).encode()).hexdigest())

    def test_result_parser_preserves_partial_failure_and_sanitizes_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'meson.json'
            row = results(registry())[0]
            row.update(env={'TOKEN': 'secret-from-meson'}, stdout='observed secret-from-meson', unknown='not evidence')
            path.write_text(json.dumps(row) + '\n{broken json\n')
            sanitizer = runner.Sanitizer({})
            observed, errors = runner.read_results(path, sanitizer)
            self.assertEqual(len(observed), 1)
            self.assertEqual(len(errors), 1)
            self.assertNotIn('env', observed[0])
            self.assertNotIn('unknown', observed[0])
            self.assertNotIn('secret-from-meson', json.dumps(sanitizer.object(observed)))

    def test_missing_log_is_explicit_failure(self):
        observed, errors = runner.read_results(Path('/missing/aggregate-fixture'), runner.Sanitizer({}))
        self.assertEqual(observed, [])
        self.assertTrue(errors)


class IntegrityTests(unittest.TestCase):
    def test_child_python_helpers_do_not_dirty_source_and_keep_other_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'fixture_module.py').write_text('VALUE = "fixture import passed"\n')
            script = root / 'child.py'
            script.write_text('import os, fixture_module\n'
                              'print(fixture_module.VALUE)\n'
                              'print(os.environ["FROZEN_FIXTURE_KEEP"])\n'
                              'print(os.environ["PYTHONDONTWRITEBYTECODE"])\n')
            with mock.patch.dict(os.environ, {'FROZEN_FIXTURE_KEEP': 'retained', 'PYTHONDONTWRITEBYTECODE': ''}):
                status = runner.run_command([sys.executable, str(script)], root / 'log')
                self.assertEqual(os.environ['PYTHONDONTWRITEBYTECODE'], '')
            self.assertEqual(status, 0)
            self.assertEqual((root / 'log').read_text(), 'fixture import passed\nretained\n1\n')
            self.assertFalse((root / '__pycache__').exists())

    def test_committed_helper_import_cannot_execute_stale_ignored_bytecode(self):
        import py_compile
        with tempfile.TemporaryDirectory() as directory:
            repository = Path(directory)
            (repository / 'tools').mkdir()
            helper = repository / runner.HELPER
            helper.write_bytes(b'VALUE = 1\n')
            before = helper.stat()
            py_compile.compile(str(helper), doraise=True)
            helper.write_bytes(b'VALUE = 2\n')
            os.utime(helper, ns=(before.st_atime_ns, before.st_mtime_ns))
            (repository / runner.RUNNER).write_bytes(Path(runner.__file__).read_bytes())
            def committed(command, **kwargs):
                name = command[-1].split(':', 1)[1]
                return (repository / name).read_bytes()
            with mock.patch.object(runner.subprocess, 'check_output', side_effect=committed):
                module = runner.load_committed_helper(repository, 'commit')
            self.assertEqual(module.VALUE, 2)

    def test_modified_packaging_helper_is_rejected_before_import(self):
        with tempfile.TemporaryDirectory() as directory:
            repository = Path(directory)
            (repository / 'tools').mkdir()
            (repository / runner.HELPER).write_bytes(b'local modifications')
            with mock.patch.object(runner.subprocess, 'check_output', return_value=b'committed helper'):
                with self.assertRaisesRegex(RuntimeError, 'exact committed file'):
                    runner.load_committed_helper(repository, 'commit')

    def test_normal_build_rejects_sanitizers_coverage_foreign_platform_and_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            build, repository = root / 'build', root / 'source'
            metadata = build / 'meson-info'
            metadata.mkdir(parents=True)
            machine = {'system': 'linux', 'cpu': 'x86_64'}
            info = {'directories': {'source': str(repository), 'build': str(build)}}
            machines = {'host': machine, 'build': machine}
            options = {'backend': 'ninja', 'b_sanitize': 'none', 'b_coverage': False}
            def write():
                (metadata / 'meson-info.json').write_text(json.dumps(info))
                (metadata / 'intro-machines.json').write_text(json.dumps(machines))
                (metadata / 'intro-buildoptions.json').write_text(json.dumps(
                    [{'name': name, 'value': value} for name, value in options.items()]))
            write()
            with mock.patch.object(runner.platform, 'system', return_value='Linux'):
                self.assertEqual(runner.require_normal_build(build, repository)['b_sanitize'], 'none')
                for field, value in [('backend', 'vs'), ('b_sanitize', 'address'), ('b_coverage', True)]:
                    old = options[field]
                    options[field] = value
                    write()
                    with self.assertRaises(RuntimeError):
                        runner.require_normal_build(build, repository)
                    options[field] = old
                write()
                with self.assertRaises(RuntimeError):
                    runner.require_normal_build(build, root / 'wrong-source')
                machines['host'] = {'system': 'windows', 'cpu': 'x86_64'}
                write()
                with self.assertRaises(RuntimeError):
                    runner.require_normal_build(build, repository)

    def test_command_interruption_signals_only_its_own_group_and_waits(self):
        with tempfile.TemporaryDirectory() as directory:
            process = mock.Mock(pid=12345)
            process.wait.side_effect = [KeyboardInterrupt(), 0]
            with mock.patch.object(runner.subprocess, 'Popen', return_value=process) as launch, \
                    mock.patch.object(runner.os, 'killpg') as kill:
                with self.assertRaises(KeyboardInterrupt):
                    runner.run_command(['fixture'], Path(directory) / 'log')
            self.assertTrue(launch.call_args.kwargs['start_new_session'])
            kill.assert_called_once_with(12345, runner.signal.SIGTERM)
            self.assertEqual(process.wait.call_args_list[-1], mock.call(timeout=30))

    def test_changed_deleted_new_copied_and_private_alias_elfs(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            binary = build / 'original'
            binary.write_bytes(b'\x7fELFfixture')
            binary.chmod(0o755)
            before = runner.elf_inventory(build, package)
            private = build / '.GIMP3-build-config-fixture'
            private.mkdir(mode=0o700)
            (private / 'alias').symlink_to(binary)
            after = runner.elf_inventory(build, package)
            self.assertEqual(runner.elf_changes(before, after, build),
                             ([], ['.GIMP3-build-config-fixture/alias'], []))
            (build / 'copied').write_bytes(binary.read_bytes())
            after = runner.elf_inventory(build, package)
            self.assertIn('copied', runner.elf_changes(before, after, build)[2])
            private.chmod(0o755)
            self.assertIn('.GIMP3-build-config-fixture/alias', runner.elf_changes(before, after, build)[2])
            binary.write_bytes(b'\x7fELFchanged')
            self.assertEqual(runner.elf_changes(before, runner.elf_inventory(build, package), build)[0], ['original'])
            binary.unlink()
            self.assertEqual(runner.elf_changes(before, {}, build)[0], ['original'])

    def test_alias_outside_private_profile_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            (build / 'original').write_bytes(b'\x7fELFfixture')
            before = runner.elf_inventory(build, package)
            (build / 'alias').symlink_to('original')
            self.assertEqual(runner.elf_changes(before, runner.elf_inventory(build, package), build)[2], ['alias'])

    def test_expected_commit_tree_and_clean_source_are_mandatory(self):
        original = {'commit': 'commit', 'tree': 'tree', 'clean': True}
        runner.require_source(original, 'commit', 'tree')
        for key, value in [('commit', 'wrong'), ('tree', 'wrong'), ('clean', False)]:
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                runner.require_source({**original, key: value}, 'commit', 'tree')

    def test_source_state_detects_untracked_and_changed_files(self):
        with tempfile.TemporaryDirectory() as directory:
            repository = Path(directory)
            subprocess.run(['git', 'init', '--quiet', str(repository)], check=True)
            (repository / 'tracked.c').write_text('source')
            subprocess.run(['git', '-C', str(repository), 'add', '.'], check=True)
            subprocess.run(['git', '-C', str(repository), '-c', 'user.name=Fixture',
                            '-c', 'user.email=fixture@example.invalid', 'commit', '-qm', 'fixture'], check=True)
            before = runner.source_state(repository, package)
            self.assertTrue(before['clean'])
            # Git status alone can hide locally modified build inputs.
            subprocess.run(['git', '-C', str(repository), 'update-index', '--assume-unchanged', 'tracked.c'], check=True)
            (repository / 'tracked.c').write_text('hidden')
            hidden = runner.source_state(repository, package)
            self.assertEqual(hidden['status'], '')
            self.assertFalse(hidden['clean'])
            self.assertEqual(hidden['hidden_index_entries'], ['tracked.c'])
            self.assertEqual(hidden['committed_content_mismatches'], ['tracked.c'])
            (repository / 'tracked.c').write_text('source')
            subprocess.run(['git', '-C', str(repository), 'update-index', '--no-assume-unchanged', 'tracked.c'], check=True)
            subprocess.run(['git', '-C', str(repository), 'config', 'core.filemode', 'false'], check=True)
            (repository / 'tracked.c').chmod(0o755)
            mode_changed = runner.source_state(repository, package)
            self.assertEqual(mode_changed['status'], '')
            self.assertFalse(mode_changed['clean'])
            self.assertEqual(mode_changed['committed_content_mismatches'], ['tracked.c'])
            (repository / 'tracked.c').chmod(0o644)
            (repository / 'untracked.txt').write_text('not committed')
            self.assertFalse(runner.source_state(repository, package)['clean'])
            (repository / 'untracked.txt').unlink()
            (repository / 'tracked.c').write_text('changed')
            after = runner.source_state(repository, package)
            self.assertFalse(after['clean'])
            self.assertNotEqual(before['tracked_files'], after['tracked_files'])


class ExecutionTests(unittest.TestCase):
    """Synthetic subprocess/metadata fixtures exercise the real orchestration."""
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.repo, self.build, self.output = (root / name for name in ('source', 'build', 'evidence'))
        for path in (self.repo, self.build, self.output):
            path.mkdir()
        (self.build / 'app').mkdir()
        (self.build / 'meson-info').mkdir()
        (self.repo / 'tools').mkdir()
        for name in (runner.RUNNER, runner.HELPER):
            (self.repo / name).write_text('committed fixture')
        for name in runner.EXECUTABLES:
            (self.build / name).write_bytes(b'\x7fELFfixture')
        self.registry = registry()
        self.registry_path = self.build / 'meson-info/intro-tests.json'
        self.registry_path.write_text(json.dumps(self.registry))
        self.source = {'commit': 'c' * 40, 'tree': 'a' * 40, 'clean': True, 'status': '',
                       'tracked_files': {'fixture.c': {'sha256': 'source'}}, 'submodules': {}}
        self.snapshot = {'format': 1, 'build_directory': str(self.build), 'ninja_targets_sha256': 'targets',
                         'ninja_commands_sha256': 'commands', 'build_outputs': {}, 'build_metadata': {},
                         'install_inputs': {}, 'source_commit': self.source['commit'],
                         'tracked_source_files': {}, 'source_inventory': {}, 'gimp_data': {}}
        self.args = argparse.Namespace(repository=self.repo, build=self.build, output=self.output,
                                       expected_commit=self.source['commit'], expected_tree=self.source['tree'],
                                       lock=root / 'build.lock')
        self.build_exit = self.meson_exit = 0
        self.observed = results(self.registry)
        self.calls = []

    def fake_run(self, command, log_path):
        self.calls.append(command)
        # A second nonblocking exclusive lock must fail throughout both calls.
        with self.args.lock.open('a') as lock:
            import fcntl
            with self.assertRaises(BlockingIOError):
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        log_path.write_text('Inherited environment: SECRET=private\nfixture output\n')
        if command[0] == 'ninja':
            self.assertEqual(command, ['ninja', '-C', str(self.build)])
            return self.build_exit
        self.assertEqual(command[:8], ['meson', 'test', '-C', str(self.build), '--no-rebuild', '--num-processes', '1', '--print-errorlogs'])
        self.assertEqual(command[8], '--logbase')
        Path(command[9] + '.json').write_text(''.join(json.dumps(row) + '\n' for row in self.observed))
        return self.meson_exit

    def execute(self, snapshots=None, sources=None):
        with mock.patch.object(runner, 'load_committed_helper', return_value=package), \
                mock.patch.object(runner, 'require_normal_build', return_value={'b_sanitize': 'none'}), \
                mock.patch.object(runner, 'source_state', side_effect=sources or [copy.deepcopy(self.source) for _ in range(3)]), \
                mock.patch.object(package, 'capture_freshness_snapshot', side_effect=snapshots or [copy.deepcopy(self.snapshot), copy.deepcopy(self.snapshot)]) as capture, \
                mock.patch.object(runner, 'run_command', side_effect=self.fake_run), \
                mock.patch.dict(os.environ, {'DISPLAY': ':91', 'XAUTHORITY': '/private/authority'}):
            status = runner.execute(self.args, self.output, runner.Sanitizer({}))
        gate = json.loads((self.output / 'aggregate-gate.json').read_text())
        return status, gate, capture.call_count

    def test_success_is_accepted_by_real_packager_and_uses_two_genuine_seals(self):
        status, gate, count = self.execute()
        self.assertEqual(status, 0, gate['errors'])
        self.assertEqual(count, 2)
        self.assertEqual(len(self.calls), 2)
        self.assertEqual(gate['packager_validation'], 'accepted')
        self.assertEqual(gate['registry_sha256'], package.sha(self.registry_path))
        self.assertEqual(package.require_aggregate_gate(gate, self.source['commit'], self.build), self.snapshot)
        self.assertNotIn('private', (self.output / 'meson-console.log').read_text())
        self.assertFalse(Path(gate['meson_command'][-1]).parent.exists())

    def test_default_build_failure_keeps_failing_gate_without_invented_before_facts(self):
        self.build_exit = 1
        status, gate, count = self.execute()
        self.assertEqual(status, 1)
        self.assertFalse(gate['all_passed'])
        self.assertEqual(gate['build_freshness'], {})
        self.assertEqual(count, 0)
        self.assertEqual(len(self.calls), 1)
        self.assertTrue((self.output / 'default-build.log').is_file())

    def test_interrupted_build_keeps_attempted_command_and_sanitized_log_under_lock(self):
        original_run = self.fake_run
        original_write = runner.write_json
        def interrupted(command, log):
            original_run(command, log)
            raise KeyboardInterrupt('interrupted default build')
        def write(path, value, sanitizer):
            if path.name == 'aggregate-gate.json':
                import fcntl
                with self.args.lock.open('a') as lock, self.assertRaises(BlockingIOError):
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return original_write(path, value, sanitizer)
        with mock.patch.object(self, 'fake_run', side_effect=interrupted), \
                mock.patch.object(runner, 'write_json', side_effect=write):
            status, gate, count = self.execute()
        self.assertEqual((status, count), (1, 0))
        self.assertEqual(gate['default_build']['command'], ['ninja', '-C', str(self.build)])
        self.assertIsNone(gate['default_build']['exit_status'])
        self.assertIn('fixture output', (self.output / 'default-build.log').read_text())
        self.assertNotIn('private', (self.output / 'default-build.log').read_text())

    def test_tap_skip_keeps_both_snapshots_and_sanitized_evidence(self):
        self.observed[0]['stdout'] = 'ok 1 /large # SKIP slow mode'
        self.observed[0]['env'] = {'TOKEN': 'private-secret'}
        status, gate, count = self.execute()
        self.assertEqual(status, 1)
        self.assertEqual(count, 2)
        self.assertFalse(gate['all_passed'])
        self.assertTrue(gate['targets'][0]['tap_skipped_subtests'])
        self.assertNotIn('private-secret', (self.output / 'test-results.json').read_text())
        with self.assertRaises(RuntimeError):
            package.require_aggregate_gate(gate, self.source['commit'], self.build)

    def test_nonzero_meson_cannot_pass_even_when_every_result_says_ok(self):
        self.meson_exit = 2
        status, gate, count = self.execute()
        self.assertEqual(status, 1)
        self.assertEqual(count, 2)
        self.assertEqual(gate['meson_exit_status'], 2)

    def test_after_snapshot_mutation_is_rejected_by_committed_helper(self):
        after = copy.deepcopy(self.snapshot)
        after['build_outputs']['unexpected'] = {'sha256': 'new artifact'}
        status, gate, count = self.execute(snapshots=[self.snapshot, after])
        self.assertEqual(status, 1)
        self.assertEqual(count, 2)
        self.assertIn('aggregate changed', gate['errors'][-1])
        self.assertNotEqual(gate['build_freshness']['before'], gate['build_freshness']['after'])

    def test_failed_after_capture_cannot_be_filled_in_from_before(self):
        status, gate, count = self.execute(snapshots=[self.snapshot, RuntimeError('capture interrupted')])
        self.assertEqual(status, 1)
        self.assertEqual(count, 2)
        self.assertNotIn('after', gate['build_freshness'])
        self.assertTrue(any('capture interrupted' in error for error in gate['errors']))

    def test_dirty_source_before_build_prevents_running_anything(self):
        dirty = {**self.source, 'clean': False, 'status': ' M fixture.c'}
        status, gate, count = self.execute(sources=[dirty])
        self.assertEqual((status, count, len(self.calls)), (1, 0, 0))
        self.assertFalse(gate['all_passed'])

    def test_dirty_source_after_tests_rejects_gate(self):
        dirty = {**self.source, 'clean': False, 'status': '?? new-file.c'}
        status, gate, count = self.execute(sources=[self.source, self.source, dirty])
        self.assertEqual(status, 1)
        self.assertFalse(gate['integrity_checks']['tracked_tree_clean_before_and_after'])

    def test_interrupted_meson_keeps_after_facts_and_partial_results(self):
        original = self.fake_run
        def interrupted(command, log):
            result = original(command, log)
            if command[0] == 'meson':
                raise KeyboardInterrupt('fixture interruption')
            return result
        with mock.patch.object(self, 'fake_run', side_effect=interrupted):
            status, gate, count = self.execute()
        self.assertEqual(status, 1)
        self.assertEqual(count, 2)
        self.assertEqual(gate['recorded_results'], 2)
        self.assertIn('after', gate['build_freshness'])
        self.assertTrue(any('fixture interruption' in error for error in gate['errors']))
        self.assertTrue((self.output / 'source-integrity.json').is_file())
        self.assertTrue((self.output / 'elf-integrity.json').is_file())

    def test_raw_registry_env_change_is_detected_without_publishing_environment(self):
        original = self.fake_run
        def mutated(command, log):
            result = original(command, log)
            if command[0] == 'meson':
                changed = copy.deepcopy(self.registry)
                changed[0]['env'] = {'PRIVATE': 'not published'}
                self.registry_path.write_text(json.dumps(changed))
            return result
        with mock.patch.object(self, 'fake_run', side_effect=mutated):
            status, gate, count = self.execute()
        self.assertEqual(status, 1)
        self.assertFalse(gate['integrity_checks']['test_registry_unchanged'])
        self.assertNotIn('not published', (self.output / 'aggregate-gate.json').read_text())

    def test_changed_preexisting_executable_rejects_gate_even_with_ok_results(self):
        original = self.fake_run
        def mutated(command, log):
            result = original(command, log)
            if command[0] == 'meson':
                (self.build / runner.EXECUTABLES[0]).write_bytes(b'\x7fELFchanged')
            return result
        with mock.patch.object(self, 'fake_run', side_effect=mutated):
            status, gate, count = self.execute()
        self.assertEqual(status, 1)
        self.assertFalse(gate['integrity_checks']['preexisting_elf_files_unchanged'])
        self.assertEqual(gate['changed_preexisting_elf_files'], [runner.EXECUTABLES[0]])

    def test_sanitization_must_not_silently_remove_part_of_a_freshness_snapshot(self):
        snapshot = copy.deepcopy(self.snapshot)
        snapshot['tracked_source_files']['environment'] = {'sha256': 'a real tracked filename'}
        status, gate, count = self.execute(snapshots=[snapshot, copy.deepcopy(snapshot)])
        self.assertEqual(status, 1)
        self.assertFalse(gate['all_passed'])
        self.assertTrue(any('Sanitization would alter sealed' in error for error in gate['errors']))


if __name__ == '__main__':
    unittest.main()
