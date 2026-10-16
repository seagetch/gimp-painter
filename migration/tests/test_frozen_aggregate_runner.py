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
import struct
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


def compiled_catalog(header=b'Content-Type: text/plain; charset=UTF-8\n', endian='<'):
    # A real revision-0 GNU catalog containing its empty-msgid metadata entry.
    return (struct.pack(endian + '7I', 0x950412de, 0, 1, 28, 36, 0, 0) +
            struct.pack(endian + '4I', 0, 44, len(header), 45) + b'\0' + header + b'\0')


def locale_fixture(build):
    metadata = build / 'meson-info'
    metadata.mkdir(parents=True, exist_ok=True)
    prefix = build.parent / 'install'
    (build / 'config.h').write_text('#define GETTEXT_PACKAGE "gimp30"\n' +
                                   '#define LOCALEDIR ' + json.dumps(str(prefix / 'share/locale')) + '\n' +
                                   '#define GIMPPLUGINDIR ' + json.dumps(str(prefix / 'lib/gimp/3.0')) + '\n')
    (metadata / 'intro-buildoptions.json').write_text(json.dumps([
        {'name': 'prefix', 'value': str(prefix)}, {'name': 'datadir', 'value': 'share'}]))
    plan, installed, targets = {}, {}, []
    for directory, domain, global_catalog, install in (
            ('po', 'gimp30', True, True), ('po-plug-ins', 'gimp30-std-plug-ins', True, True),
            ('po-plug-ins', 'org.gimp.extension.goat-exercises', False, True),
            ('po-tips', 'gimp30-tips', False, False)):
        source = build / directory / 'fr/LC_MESSAGES' / (domain + '.mo')
        source.parent.mkdir(parents=True, exist_ok=True)
        if install:
            source.write_bytes(compiled_catalog())
        targets.append({'id': domain + '-fr', 'filename': [str(source)], 'installed': install,
                        'install_filename': [str(prefix / 'irrelevant-Meson-target-install-filename')],
                        'build_by_default': install})
        if install:
            relative = ('share/locale/' if global_catalog else 'lib/gimp/3.0/extensions/org.gimp.extension.goat-exercises/locale/') + 'fr/LC_MESSAGES/' + domain + '.mo'
            plan[str(source)] = {'destination': '{prefix}/' + relative, 'tag': 'i18n'}
            installed[str(source)] = str(prefix / relative)
    for filename, value in (('intro-install_plan.json', {'targets': plan}),
                            ('intro-installed.json', installed), ('intro-targets.json', targets)):
        (metadata / filename).write_text(json.dumps(value))
    return plan


class LocaleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / 'build'
        self.build.mkdir()
        self.plan = locale_fixture(self.build)
        self.destination = self.root / 'locale'
        self.core = self.build / 'po/fr/LC_MESSAGES/gimp30.mo'

    def read(self, name):
        return json.loads((self.build / 'meson-info' / name).read_text())

    def write(self, name, value):
        (self.build / 'meson-info' / name).write_text(json.dumps(value))

    def stage(self):
        return runner.stage_build_locales(self.build, self.destination)

    def test_only_declared_global_outputs_are_copied_with_hash_provenance(self):
        original = {name: Path(name).read_bytes() for name in self.plan}
        manifest = self.stage()
        self.assertEqual(manifest['catalog_count'], 2)
        self.assertEqual(manifest['domains'], ['gimp30-std-plug-ins.mo', 'gimp30.mo'])
        self.assertEqual(len(manifest['excluded_extension_catalog_outputs']), 1)
        self.assertEqual(len(manifest['metadata_sha256']), 5)
        self.assertFalse((self.root / 'install').exists())
        for name, record in manifest['catalogs'].items():
            payload = (self.destination / name).read_bytes()
            self.assertEqual(payload, (self.build / record['source']).read_bytes())
            self.assertEqual(hashlib.sha256(payload).hexdigest(), record['sha256'])
            self.assertEqual(len(payload), record['size_bytes'])
        self.assertEqual(original, {name: Path(name).read_bytes() for name in self.plan})
        self.assertTrue(runner.verify_staged_locales(self.destination, manifest))

    def test_missing_or_invalid_compiled_output_is_not_an_empty_directory_fallback(self):
        for payload in (None, b'', b'not a compiled catalog'):
            with self.subTest(payload=payload):
                if payload is None:
                    self.core.unlink()
                else:
                    self.core.write_bytes(payload)
                with self.assertRaises(RuntimeError):
                    self.stage()
                self.assertFalse(self.destination.exists())

    def test_metadata_sets_must_agree_in_both_directions(self):
        for name in ('intro-install_plan.json', 'intro-installed.json', 'intro-targets.json'):
            for remove in (True, False):
                with self.subTest(metadata=name, remove=remove):
                    locale_fixture(self.build)
                    value = self.read(name)
                    if name == 'intro-targets.json':
                        if remove:
                            value = value[1:]
                        else:
                            value.append({'id': 'extra', 'installed': True, 'filename': [str(self.build / 'extra.mo')]})
                    else:
                        entries = value['targets'] if name == 'intro-install_plan.json' else value
                        if remove:
                            del entries[str(self.core)]
                        else:
                            entries[str(self.build / 'extra.mo')] = next(iter(entries.values()))
                    self.write(name, value)
                    with self.assertRaisesRegex(RuntimeError, 'disagree'):
                        self.stage()

    def test_duplicate_declared_output_and_destination_are_rejected(self):
        targets = self.read('intro-targets.json')
        targets.append(copy.deepcopy(targets[0]))
        self.write('intro-targets.json', targets)
        with self.assertRaisesRegex(RuntimeError, 'Ambiguous'):
            self.stage()
        locale_fixture(self.build)
        plan = self.read('intro-install_plan.json')
        installed = self.read('intro-installed.json')
        second = next(name for name in installed if name != str(self.core))
        plan['targets'][second] = plan['targets'][str(self.core)]
        installed[second] = installed[str(self.core)]
        self.write('intro-install_plan.json', plan)
        self.write('intro-installed.json', installed)
        with self.assertRaisesRegex(RuntimeError, 'Duplicate'):
            self.stage()

    def test_leaf_and_ancestor_symlinks_cannot_supply_catalogs(self):
        original = self.core.read_bytes()
        other = self.root / 'outside.mo'
        other.write_bytes(original)
        self.core.unlink()
        self.core.symlink_to(other)
        with self.assertRaisesRegex(RuntimeError, 'regular build file'):
            self.stage()
        self.core.unlink()
        self.core.write_bytes(original)
        (self.build / 'po').rename(self.root / 'external-po')
        (self.build / 'po').symlink_to(self.root / 'external-po', target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, 'regular build file'):
            self.stage()

    def test_install_destination_mismatch_traversal_and_layout_are_rejected(self):
        for destination in ('{prefix}/share/locale/../escape/fr/LC_MESSAGES/gimp30.mo',
                            '{prefix}/share/locale/fr/not-LC_MESSAGES/gimp30.mo',
                            '{unknown}/fr/LC_MESSAGES/gimp30.mo'):
            with self.subTest(destination=destination):
                locale_fixture(self.build)
                plan = self.read('intro-install_plan.json')
                plan['targets'][str(self.core)]['destination'] = destination
                installed = self.read('intro-installed.json')
                installed[str(self.core)] = destination.replace('{prefix}', str(self.root / 'install'))
                self.write('intro-install_plan.json', plan)
                self.write('intro-installed.json', installed)
                with self.assertRaises(RuntimeError):
                    self.stage()
        locale_fixture(self.build)
        installed = self.read('intro-installed.json')
        installed[str(self.core)] += '-mismatch'
        self.write('intro-installed.json', installed)
        with self.assertRaisesRegex(RuntimeError, 'provenance disagrees'):
            self.stage()

    def test_core_and_standard_plugin_catalogs_are_required(self):
        plan = self.read('intro-install_plan.json')
        installed = self.read('intro-installed.json')
        del plan['targets'][str(self.core)]
        del installed[str(self.core)]
        targets = [row for row in self.read('intro-targets.json') if str(self.core) not in row['filename']]
        self.write('intro-install_plan.json', plan)
        self.write('intro-installed.json', installed)
        self.write('intro-targets.json', targets)
        with self.assertRaisesRegex(RuntimeError, 'requires core'):
            self.stage()

    def test_global_catalog_cannot_be_silently_excluded_by_agreeing_metadata(self):
        plan = self.read('intro-install_plan.json')
        installed = self.read('intro-installed.json')
        plan['targets'][str(self.core)]['destination'] = '{prefix}/lib/private/fr/LC_MESSAGES/gimp30.mo'
        installed[str(self.core)] = str(self.root / 'install/lib/private/fr/LC_MESSAGES/gimp30.mo')
        self.write('intro-install_plan.json', plan)
        self.write('intro-installed.json', installed)
        with self.assertRaisesRegex(RuntimeError, 'Unknown non-global'):
            self.stage()

    def test_duplicate_json_keys_and_false_extension_layout_are_rejected(self):
        path = self.build / 'meson-info/intro-installed.json'
        content = path.read_text()
        path.write_text(content[:-1] + ', ' + json.dumps(str(self.core)) + ': "duplicate"}')
        with self.assertRaisesRegex(RuntimeError, 'Duplicate key'):
            self.stage()
        locale_fixture(self.build)
        plan = self.read('intro-install_plan.json')
        installed = self.read('intro-installed.json')
        extension = next(name for name in installed if 'goat-exercises.mo' in name)
        destination = '{prefix}/lib/gimp/3.0/extensions/org.gimp.extension.wrong/locale/fr/LC_MESSAGES/org.gimp.extension.goat-exercises.mo'
        plan['targets'][extension]['destination'] = destination
        installed[extension] = destination.replace('{prefix}', str(self.root / 'install'))
        self.write('intro-install_plan.json', plan)
        self.write('intro-installed.json', installed)
        with self.assertRaisesRegex(RuntimeError, 'Invalid extension-private'):
            self.stage()

    def test_binary_structure_accepts_both_endiannesses_and_preserves_legacy_metadata(self):
        for endian in ('<', '>'):
            runner.validate_compiled_catalog(compiled_catalog(endian=endian))
        legacy = compiled_catalog(b'Plural-Forms: 2;(n!=1);\n')
        self.core.write_bytes(legacy)
        self.stage()
        self.assertEqual((self.destination / 'fr/LC_MESSAGES/gimp30.mo').read_bytes(), legacy)
        valid = compiled_catalog()
        for payload in (valid[:27], valid[:-1], valid[:-1] + b'x',
                        valid[:8] + struct.pack('<I', 10000) + valid[12:],
                        valid[:24] + struct.pack('<I', 9999) + valid[28:32]):
            with self.subTest(payload=payload), self.assertRaises(RuntimeError):
                runner.validate_compiled_catalog(payload)

    def test_revision_one_static_and_system_dependent_tables_are_bounded(self):
        header = b'Content-Type: text/plain; charset=UTF-8\n'
        for endian in ('<', '>'):
            normal_start, translated_start = 120, 121
            segment_start = translated_start + len(header) + 1
            static_start = segment_start + 7
            valid = (struct.pack(endian + '12I', 0x950412de, 0x10001, 1, 48, 56, 0, 0,
                                 1, 64, 1, 72, 76) +
                     struct.pack(endian + '8I', 0, normal_start, len(header), translated_start,
                                 7, segment_start, 80, 100) +
                     struct.pack(endian + '5I', static_start, 2, 0, 2, 0xffffffff) * 2 +
                     b'\0' + header + b'\0PRIu64\0a b\0')
            runner.validate_compiled_catalog(valid)
            for offset, value in ((32, len(valid)), (40, len(valid)), (44, len(valid)),
                                  (64, 0), (68, 1), (88, 1), (92, 0), (96, 0), (80, 1)):
                broken = bytearray(valid)
                struct.pack_into(endian + 'I', broken, offset, value)
                with self.subTest(endian=endian, offset=offset), self.assertRaises(RuntimeError):
                    runner.validate_compiled_catalog(broken)
        missing_extension = bytearray(compiled_catalog())
        struct.pack_into('<I', missing_extension, 4, 0x10001)
        with self.assertRaises(RuntimeError):
            runner.validate_compiled_catalog(missing_extension)

    def test_extra_directory_and_special_file_in_staged_locales_are_rejected(self):
        manifest = self.stage()
        extra = self.destination / 'unexpected'
        extra.mkdir()
        with self.assertRaisesRegex(RuntimeError, 'directory inventory'):
            runner.verify_staged_locales(self.destination, manifest)
        extra.rmdir()
        os.mkfifo(extra)
        with self.assertRaisesRegex(RuntimeError, 'special filesystem'):
            runner.verify_staged_locales(self.destination, manifest)
        extra.unlink()

    def test_staged_mutation_missing_extra_or_symlink_fails_verification(self):
        manifest = self.stage()
        path = self.destination / 'fr/LC_MESSAGES/gimp30.mo'
        original = path.read_bytes()
        for alteration in ('changed', 'missing', 'extra', 'symlink'):
            with self.subTest(alteration=alteration):
                if alteration == 'changed':
                    path.write_bytes(original + b'x')
                elif alteration == 'missing':
                    path.unlink()
                elif alteration == 'extra':
                    (path.parent / 'extra.mo').write_bytes(original)
                else:
                    path.unlink()
                    path.symlink_to(self.core)
                with self.assertRaises(RuntimeError):
                    runner.verify_staged_locales(self.destination, manifest)
                if path.is_symlink():
                    path.unlink()
                path.write_bytes(original)
                (path.parent / 'extra.mo').unlink(missing_ok=True)

    def test_existing_destination_or_changed_metadata_is_rejected(self):
        self.destination.mkdir()
        with self.assertRaises(FileExistsError):
            self.stage()
        self.destination.rmdir()
        original = runner.sha
        calls = 0
        def changed(path):
            nonlocal calls
            calls += 1
            return original(path) if calls <= 5 else 'changed metadata'
        with mock.patch.object(runner, 'sha', side_effect=changed):
            with self.assertRaisesRegex(RuntimeError, 'metadata changed'):
                self.stage()
        self.assertFalse(self.destination.exists())


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


class EnvironmentTests(unittest.TestCase):
    def test_private_paths_replace_host_state_without_changing_other_inputs(self):
        inherited = {key: '/unwritable/host/' + key for key in runner.PRIVATE_DIRECTORIES}
        inherited.update(GSETTINGS_BACKEND='dconf', PYTHONDONTWRITEBYTECODE='',
                         DISPLAY='localhost:91', XAUTHORITY='/private/authority',
                         PATH='/dependency/bin:/usr/bin', LD_LIBRARY_PATH='/dependency/lib',
                         FONTCONFIG_FILE='/dependency/etc/fonts/fonts.conf',
                         XDG_DATA_DIRS='/dependency/share:/usr/share',
                         XDG_CONFIG_DIRS='/dependency/etc/xdg', LANG='C.UTF-8')
        original = dict(inherited)
        with tempfile.TemporaryDirectory() as directory:
            environment = runner.isolated_child_environment(directory, inherited)
            self.assertEqual(inherited, original)
            for key in inherited.keys() - runner.PRIVATE_DIRECTORIES.keys() - {
                    'GSETTINGS_BACKEND', 'PYTHONDONTWRITEBYTECODE'}:
                self.assertEqual(environment[key], inherited[key], key)
            for key, name in runner.PRIVATE_DIRECTORIES.items():
                self.assertEqual(environment[key], str(Path(directory) / name))
            self.assertEqual(environment['GSETTINGS_BACKEND'], 'memory')
            self.assertEqual(environment['PYTHONDONTWRITEBYTECODE'], '1')

    def test_directories_exist_are_writable_private_and_support_capacity_queries(self):
        with tempfile.TemporaryDirectory() as directory:
            environment = runner.isolated_child_environment(directory, {})
            for key in runner.PRIVATE_DIRECTORIES:
                path = Path(environment[key])
                with self.subTest(variable=key):
                    self.assertTrue(path.is_absolute())
                    self.assertTrue(path.is_dir())
                    self.assertFalse(path.is_symlink())
                    self.assertEqual(path.stat().st_uid, os.getuid())
                    self.assertEqual(path.stat().st_mode & 0o777, 0o700)
                    (path / 'write-probe').write_text('writable')
                    self.assertEqual((path / 'write-probe').read_text(), 'writable')
            # Fontconfig can create its normal per-user cache, and GIMP's
            # default spill paths support statvfs before native initialization.
            font_cache = Path(environment['XDG_CACHE_HOME']) / 'fontconfig'
            self.assertTrue(font_cache.is_dir())
            self.assertEqual(font_cache.stat().st_mode & 0o777, 0o700)
            (font_cache / 'cache-probe').write_bytes(b'cache')
            for key in ('GIMP3_TEMPDIR', 'GIMP3_CACHEDIR'):
                self.assertGreater(os.statvfs(environment[key]).f_frsize, 0)

    def test_preexisting_directory_or_symlink_is_not_reused(self):
        for symlink in (False, True):
            with self.subTest(symlink=symlink), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                occupied = root / runner.PRIVATE_DIRECTORIES['HOME']
                if symlink:
                    occupied.symlink_to(root / 'missing-target', target_is_directory=True)
                else:
                    occupied.mkdir()
                    (occupied / 'old-state').write_text('untouched')
                with self.assertRaises(FileExistsError):
                    runner.isolated_child_environment(root, {})
                if not symlink:
                    self.assertEqual((occupied / 'old-state').read_text(), 'untouched')

    def test_child_uses_explicit_private_state_without_mutating_caller_environment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            environment = runner.isolated_child_environment(root, {'FROZEN_FIXTURE_KEEP': 'retained'})
            environment['PYTHONDONTWRITEBYTECODE'] = ''
            original = dict(environment)
            script = root / 'child.py'
            script.write_text('import json, os, tempfile\n'
                              'keys = ' + repr(list(runner.PRIVATE_DIRECTORIES)) + '\n'
                              'for key in keys:\n'
                              '    with open(os.path.join(os.environ[key], "child-write"), "w") as stream:\n'
                              '        stream.write("child")\n'
                              'print(json.dumps({key: os.environ[key] for key in keys}))\n'
                              'print(tempfile.gettempdir())\n'
                              'print(os.environ["GSETTINGS_BACKEND"])\n'
                              'print(os.environ["PYTHONDONTWRITEBYTECODE"])\n'
                              'print(os.environ["FROZEN_FIXTURE_KEEP"])\n')
            with mock.patch.dict(os.environ, {'HOME': '/unwritable/host', 'GSETTINGS_BACKEND': 'dconf'}):
                self.assertEqual(runner.run_command([sys.executable, str(script)], root / 'log', environment), 0)
                self.assertEqual(os.environ['HOME'], '/unwritable/host')
                self.assertEqual(os.environ['GSETTINGS_BACKEND'], 'dconf')
            self.assertEqual(environment, original)
            lines = (root / 'log').read_text().splitlines()
            self.assertEqual(json.loads(lines[0]), {key: environment[key] for key in runner.PRIVATE_DIRECTORIES})
            self.assertEqual(lines[1:], [environment['TMPDIR'], 'memory', '1', 'retained'])
            for key in runner.PRIVATE_DIRECTORIES:
                self.assertEqual((Path(environment[key]) / 'child-write').read_text(), 'child')


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
        locale_fixture(self.build)
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
        self.environments = []

    def fake_run(self, command, log_path, environment):
        self.calls.append(command)
        self.environments.append(dict(environment))
        for variable in runner.PRIVATE_DIRECTORIES:
            self.assertTrue(Path(environment[variable]).is_dir(), variable)
            self.assertFalse(Path(environment[variable]).is_relative_to(self.repo), variable)
            self.assertFalse(Path(environment[variable]).is_relative_to(self.build), variable)
        self.assertEqual(environment['GSETTINGS_BACKEND'], 'memory')
        self.assertEqual(environment['PYTHONDONTWRITEBYTECODE'], '1')
        self.assertEqual(environment['DISPLAY'], ':91')
        self.assertEqual(environment['XAUTHORITY'], '/private/authority')
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
        for environment in self.environments:
            for variable in runner.PRIVATE_DIRECTORIES:
                self.assertFalse(Path(environment[variable]).exists(), variable)
        return status, gate, capture.call_count

    def test_success_is_accepted_by_real_packager_and_uses_two_genuine_seals(self):
        status, gate, count = self.execute()
        self.assertEqual(status, 0, gate['errors'])
        self.assertEqual(count, 2)
        self.assertEqual(len(self.calls), 2)
        self.assertEqual({**self.environments[0], 'GIMP3_LOCALEDIR': self.environments[1]['GIMP3_LOCALEDIR']}, self.environments[1])
        self.assertEqual(gate['build_locales']['catalog_count'], 2)
        self.assertTrue(gate['integrity_checks']['staged_locale_files_unchanged'])
        self.assertFalse(Path(self.environments[1]['GIMP3_LOCALEDIR']).exists())
        for variable in runner.PRIVATE_DIRECTORIES:
            self.assertFalse(Path(self.environments[0][variable]).exists(), variable)
        self.assertEqual(gate['packager_validation'], 'accepted')
        self.assertEqual(gate['runtime_isolation']['directory_write_preflight'], 'passed')
        self.assertTrue(gate['runtime_isolation']['fontconfig_user_cache_created'])
        self.assertEqual(gate['registry_sha256'], package.sha(self.registry_path))
        self.assertEqual(package.require_aggregate_gate(gate, self.source['commit'], self.build), self.snapshot)
        self.assertNotIn('private', (self.output / 'meson-console.log').read_text())
        self.assertFalse(Path(gate['meson_command'][-1]).parent.exists())

    def test_missing_catalog_rejects_after_build_before_meson_or_before_seal(self):
        (self.build / 'po/fr/LC_MESSAGES/gimp30.mo').unlink()
        status, gate, count = self.execute()
        self.assertEqual((status, count, len(self.calls)), (1, 0, 1))
        self.assertFalse(gate['all_passed'])
        self.assertIn('existing regular build file', gate['errors'][-1])

    def test_catalog_mutation_during_meson_rejects_even_with_passing_results(self):
        original = self.fake_run
        def mutate(command, log, environment):
            result = original(command, log, environment)
            if command[0] == 'meson':
                locale = Path(environment['GIMP3_LOCALEDIR']) / 'fr/LC_MESSAGES/gimp30.mo'
                locale.write_bytes(b'changed catalog')
            return result
        with mock.patch.object(self, 'fake_run', side_effect=mutate):
            status, gate, count = self.execute()
        self.assertEqual((status, count), (1, 2))
        self.assertIn('locale catalog content changed', gate['errors'][-1])

    def test_inherited_temp_inside_source_or_build_cannot_place_private_state_there(self):
        for inherited in (self.repo, self.build):
            with self.subTest(inherited=inherited), \
                    mock.patch.dict(os.environ, {'TMPDIR': str(inherited)}), \
                    mock.patch.object(runner.tempfile, 'tempdir', str(inherited)):
                status, gate, count = self.execute()
            self.assertEqual((status, count), (0, 2), gate['errors'])
            self.assertTrue(all(Path(environment['HOME']).is_relative_to(self.output.parent)
                                for environment in self.environments))

    def test_unwritable_private_directory_rejects_before_build_and_cleans_up(self):
        attempted = []
        def denied(*args, **kwargs):
            attempted.append(Path(kwargs['dir']))
            raise PermissionError('fixture cannot write runtime directory')
        with mock.patch.object(runner.tempfile, 'TemporaryFile', side_effect=denied):
            status, gate, count = self.execute()
        self.assertEqual((status, count, len(self.calls)), (1, 0, 0))
        self.assertTrue(attempted)
        self.assertTrue(all(not path.parent.exists() for path in attempted))
        self.assertNotIn('runtime_isolation', gate)
        self.assertIn('cannot write runtime directory', gate['errors'][-1])

    def test_late_fontconfig_write_or_flush_failure_rejects_and_cleans_all_state(self):
        from contextlib import contextmanager
        real_temporary_file = runner.tempfile.TemporaryFile
        for operation in ('write', 'flush'):
            attempted = []
            @contextmanager
            def fail_late(*args, **kwargs):
                directory = Path(kwargs['dir'])
                attempted.append(directory)
                with real_temporary_file(*args, **kwargs) as stream:
                    if directory.name == 'fontconfig':
                        failing = mock.Mock(wraps=stream)
                        getattr(failing, operation).side_effect = OSError('fixture late Fontconfig failure')
                        yield failing
                    else:
                        yield stream
            with self.subTest(operation=operation), \
                    mock.patch.object(runner.tempfile, 'TemporaryFile', side_effect=fail_late):
                status, gate, count = self.execute()
            self.assertEqual((status, count, len(self.calls)), (1, 0, 0))
            self.assertEqual(len(attempted), len(runner.PRIVATE_DIRECTORIES) + 1)
            self.assertTrue(all(not path.exists() for path in attempted))
            self.assertNotIn('runtime_isolation', gate)
            self.assertIn('late Fontconfig failure', gate['errors'][-1])

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
        def interrupted(command, log, environment):
            original_run(command, log, environment)
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
        def interrupted(command, log, environment):
            result = original(command, log, environment)
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
        def mutated(command, log, environment):
            result = original(command, log, environment)
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
        def mutated(command, log, environment):
            result = original(command, log, environment)
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
