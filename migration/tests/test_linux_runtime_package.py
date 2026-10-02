#!/usr/bin/env python3
"""Small package recipe safety tests; these do not claim application coverage."""
import importlib.util
import copy
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('package_linux', REPO/'tools/package-linux-runtime.py')
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


runtime_spec=importlib.util.spec_from_file_location('runtime_test',REPO/'tools/test-linux-runtime.py')
runtime_test=importlib.util.module_from_spec(runtime_spec)
runtime_spec.loader.exec_module(runtime_test)


class PackageTests(unittest.TestCase):
    def test_only_baseline_host_abi_is_accepted(self):
        self.assertEqual(package.BASE_ABI,runtime_test.BASE_ABI)
        for module in [package,runtime_test]:
            module.require_base_abi('libc.so.6')
            for name in ['libblas.so.3','libgtk-3.so.0','libunknown.so.1']:
                with self.assertRaises(RuntimeError):module.require_base_abi(name)

    def test_new_untracked_source_is_rechecked(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'known.cpp').write_text('known')
            with mock.patch.object(package,'command',side_effect=['known.cpp','']):
                initial=package.source_inventory(root)
            (root/'new.cpp').write_text('new')
            with mock.patch.object(package,'command',side_effect=['known.cpp','new.cpp']):
                changed=package.source_inventory(root)
            with self.assertRaises(RuntimeError):
                package.assert_snapshot_unchanged(initial,changed,{}, {})

    def test_gimp_data_commit_and_content_are_rechecked(self):
        initial={'commit':'original','inventory':{'files_sha256':{'asset':'aaa'}}}
        for changed in [{'commit':'changed','inventory':initial['inventory']},
                        {'commit':'original','inventory':{'files_sha256':{'asset':'bbb'}}}]:
            with self.assertRaises(RuntimeError):
                package.assert_snapshot_unchanged({}, {},initial,changed)
        package.assert_snapshot_unchanged({}, {},initial,initial)

    def test_default_toolrc_contains_all_painter_tools_once(self):
        toolrc=(REPO/'etc/toolrc').read_text()
        tools=re.findall(r'GimpToolInfo "([^"]+)"',toolrc)
        for name in ['gimp-painter-mypaint-tool','gimp-painter-smudge-tool','gimp-bucket-fill-brush-tool','gimp-perspective-guide-tool']:
            self.assertEqual(tools.count(name),1,name)

    def test_ldd_path_with_spaces_and_unicode(self):
        line='  libglib-2.0.so.0 => /path with spaces/日本語/libglib-2.0.so.0 (0x123456)'
        self.assertEqual(package.parse_ldd_line(line),('libglib-2.0.so.0','/path with spaces/日本語/libglib-2.0.so.0'))
        self.assertIsNone(package.parse_ldd_line('linux-vdso.so.1 (0x123456)'))

    def test_inventory_seals_content_mode_and_relative_link(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'file').write_bytes(b'example')
            (root/'file').chmod(0o755)
            (root/'alias').symlink_to('file')
            contents = package.inventory(root)
            self.assertEqual(contents['file']['bytes'],7)
            self.assertEqual(contents['file']['mode'],'0o755')
            self.assertEqual(contents['file']['sha256'],package.sha(root/'file'))
            self.assertEqual(contents['alias'],{'symlink':'file'})

    def test_runtime_copy_omits_caches_and_build_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source, target = root/'source', root/'target'
            source.mkdir()
            for name in ['runtime.so','module.py','unused.pyc','unused.a','unused.pc','unused.la']:
                (source/name).write_bytes(b'fixture')
            (source/'__pycache__').mkdir()
            (source/'__pycache__/cached').write_bytes(b'fixture')
            package.copy_runtime(source,target)
            self.assertEqual(sorted(p.name for p in target.iterdir()),['module.py','runtime.so'])

    def test_runtime_symlinks_cannot_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)/'bundle'
            root.mkdir()
            (root/'escape').symlink_to('../outside')
            with self.assertRaises(RuntimeError):
                package.relative_symlinks(root)

    def test_absolute_usr_link_requires_bundled_target(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'link').symlink_to('/usr/lib/missing.so')
            with self.assertRaises(RuntimeError):
                package.relative_symlinks(root)

    def test_absolute_usr_link_becomes_relative(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root/'usr/lib/file.so'
            target.parent.mkdir(parents=True)
            target.write_bytes(b'fixture')
            (root/'link').symlink_to('/usr/lib/file.so')
            package.relative_symlinks(root)
            self.assertEqual(os.readlink(root/'link'),'usr/lib/file.so')

    def test_elf_probe_ignores_links_and_plain_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'elf').write_bytes(b'\x7fELFfixture')
            (root/'plain').write_text('not ELF')
            (root/'link').symlink_to('elf')
            self.assertTrue(package.elf(root/'elf'))
            self.assertFalse(package.elf(root/'plain'))
            self.assertFalse(package.elf(root/'link'))


@unittest.skipUnless(shutil.which('ninja'), 'Ninja is required for convergence fixtures')
class FreshnessTests(unittest.TestCase):
    """Private, tiny Ninja/Meson-metadata fixtures; never use the GIMP build."""

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.repo = Path(self.directory.name)/'source with spaces'
        self.repo.mkdir()
        self.build = self.repo/'build'
        self.build.mkdir()
        (self.build/'app').mkdir()
        (self.build/'meson-info').mkdir()
        (self.build/'config.h').write_text('configuration\n')
        (self.repo/'.gitignore').write_text('build/\ngimp-data/\n')
        (self.repo/'version.txt').write_text('tested version\n')
        (self.repo/'resource.txt').write_text('tested resource\n')
        (self.repo/'installed.txt').write_text('installed source resource\n')
        (self.repo/'generate.py').write_text(
            'import os\nfrom pathlib import Path\n'
            'value = os.environ.get("PACKAGE_FIXTURE_VERSION", Path("../version.txt").read_text())\n'
            'out = Path("git-version.h")\n'
            'if not out.exists() or out.read_text() != value: out.write_text(value)\n')
        (self.repo/'check.py').write_text(
            'import os\nraise SystemExit(int(os.environ.get("PACKAGE_FIXTURE_FAIL", "0")))\n')
        (self.build/'build.ninja').write_text(
            'rule generate\n  command = python3 ../generate.py\n  restat = 1\n'
            'rule copy\n  command = cp $in $out\n'
            'rule check\n  command = python3 ../check.py\n  restat = 1\n'
            'build PHONY: phony\n'
            'build git-version.h: generate ../version.txt | PHONY\n'
            'build app/gimp-3.0: copy git-version.h\n'
            'build app/gimp-console-3.0: copy git-version.h\n'
            'build resource.dat: copy ../resource.txt\n'
            'build check-def-files: check PHONY\n'
            'build all: phony app/gimp-3.0 app/gimp-console-3.0 resource.dat check-def-files\n'
            'default all\n')
        package.json_write(self.build/'meson-info/intro-install_plan.json', {
            'targets': {str(self.build/name): {} for name in
                        ['app/gimp-3.0', 'app/gimp-console-3.0', 'resource.dat']},
            'data': {str(self.repo/'installed.txt'): {}}})
        # Duplicate display names are legitimate in the real registry. Distinct
        # registry indexes and commands must both be preserved by the gate.
        self.registry = [{'name': 'fixture', 'cmd': [str(self.build/name)],
                          'suite': ['gimp:fixture'], 'timeout': 60,
                          'env': {'UI_TEST': 'yes', 'FIXTURE_ONLY': 'synthetic metadata'}} for name in
                         ['app/gimp-3.0', 'app/gimp-console-3.0']]
        package.json_write(self.build/'meson-info/intro-tests.json', self.registry)
        self.git(self.repo, 'init', '--quiet')
        self.git(self.repo, 'add', '.')
        self.git(self.repo, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                 'commit', '--quiet', '-m', 'fixture')
        data = self.repo/'gimp-data'
        data.mkdir()
        (data/'asset.txt').write_text('pinned resource\n')
        self.git(data, 'init', '--quiet')
        self.git(data, 'add', '.')
        self.git(data, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                 'commit', '--quiet', '-m', 'fixture data')
        subprocess.run(['ninja', '-C', str(self.build)], check=True, capture_output=True)
        self.snapshot = package.capture_freshness_snapshot(self.build, self.repo)
        self.commit = package.command(['git', 'rev-parse', 'HEAD'], cwd=self.repo)
        self.gate = {'source_commit': self.commit, 'all_passed': True,
                     'build_inputs': {name: {'sha256': package.sha(self.build/name)} for name in
                                      ['app/gimp-3.0', 'app/gimp-console-3.0']},
                     'build_freshness': {'before': self.snapshot, 'after': copy.deepcopy(self.snapshot)}}
        self.gate.update({
            'schema_version': 1, 'scope': 'all_registered_meson_tests_frozen_linux_normal_build',
            'coverage_complete': True, 'registered_tests': 2, 'recorded_results': 2,
            'registry_sha256': package.sha(self.build/'meson-info/intro-tests.json'),
            'unmatched_results': [], 'meson_exit_status': 0, 'baseline_exemptions_applied': False,
            'integrity_checks': {key: True for key in package.AGGREGATE_INTEGRITY_CHECKS},
            'changed_tracked_files': [], 'changed_preexisting_elf_files': [],
            'result_counts': {'OK': 2},
            'suite_counts': {'gimp:fixture': {'registered': 2, 'outcomes': {'OK': 2}}},
            'targets': [{'registry_index': index, 'name': row['name'], 'suites': row['suite'],
                         'registered_command': row['cmd'], 'actual_command': row['cmd'],
                         'result': 'OK', 'returncode': 0, 'classification': 'passed',
                         'tap_skipped_subtests': [], 'runtime_crash_diagnostics': [],
                         'failure_evidence': []} for index, row in enumerate(self.registry, 1)]})

    def git(self, root, *args):
        return subprocess.run(['git', '-C', str(root), *args], check=True, capture_output=True)

    def verify(self):
        expected = package.require_aggregate_gate(self.gate, self.commit, self.build)
        return package.verify_build_freshness(self.build, self.repo, expected)

    def test_phony_restat_is_accepted_despite_predicted_descendants(self):
        dry = subprocess.run(['ninja', '-C', str(self.build), '-n'], check=True,
                             capture_output=True, text=True)
        self.assertNotIn('no work to do.', dry.stdout)
        self.assertIn('cp git-version.h app/gimp-3.0', dry.stdout)
        evidence, log = self.verify()
        self.assertEqual(evidence['exit_code'], 0)
        self.assertNotIn('cp git-version.h', log)
        self.assertEqual(self.snapshot, package.capture_freshness_snapshot(self.build, self.repo))

    def test_failed_aggregate_never_becomes_eligible(self):
        for value in [False, None, 1, 'true']:
            self.gate['all_passed'] = value
            with self.assertRaisesRegex(RuntimeError, 'Aggregate gate must pass'):
                self.verify()

    def test_all_passed_cannot_hide_failure_skip_crash_or_false_totals(self):
        original = copy.deepcopy(self.gate)
        cases = [
            ('meson_exit_status', 4), ('baseline_exemptions_applied', True),
            ('result_counts', {'OK': 1, 'FAIL': 1}),
            ('suite_counts', {'gimp:fixture': {'registered': 2, 'outcomes': {'OK': 1, 'FAIL': 1}}}),
            ('integrity_checks', {key: False for key in package.AGGREGATE_INTEGRITY_CHECKS}),
            ('changed_preexisting_elf_files', ['changed.so'])]
        for key, value in cases:
            with self.subTest(field=key):
                self.gate = copy.deepcopy(original)
                self.gate[key] = value
                with self.assertRaises(RuntimeError):
                    self.verify()
        cases = [('result', 'FAIL'), ('result', 'SKIP'), ('result', 'TIMEOUT'),
                 ('returncode', -11), ('classification', 'known_upstream_baseline_failure_still_fails'),
                 ('tap_skipped_subtests', ['ok 1 # SKIP unavailable']),
                 ('runtime_crash_diagnostics', ['script-fu: fatal error: Segmentation fault']),
                 ('failure_evidence', ['not ok /fixture']), ('runtime_crash_diagnostics', None)]
        for key, value in cases:
            with self.subTest(target_field=key):
                self.gate = copy.deepcopy(original)
                self.gate['targets'][0][key] = value
                with self.assertRaisesRegex(RuntimeError, 'failures, skips or runtime crashes'):
                    self.verify()

    def test_all_passed_cannot_hide_incomplete_or_mismatched_registry(self):
        original = copy.deepcopy(self.gate)
        cases = [('coverage_complete', False), ('registered_tests', 1), ('recorded_results', 1),
                 ('targets', original['targets'][:1]), ('registry_sha256', 'old registry'),
                 ('unmatched_results', [{'name': 'extra'}])]
        for key, value in cases:
            with self.subTest(field=key):
                self.gate = copy.deepcopy(original)
                self.gate[key] = value
                with self.assertRaisesRegex(RuntimeError, 'complete coverage'):
                    self.verify()
        for key, value in [('registry_index', 2), ('name', 'different'), ('suites', ['wrong']),
                           ('registered_command', ['wrong']), ('actual_command', ['wrong'])]:
            with self.subTest(target_field=key):
                self.gate = copy.deepcopy(original)
                self.gate['targets'][0][key] = value
                with self.assertRaises(RuntimeError):
                    self.verify()

    def test_sanitized_registry_archive_does_not_replace_raw_registry_hash(self):
        archive = Path(self.directory.name)/'registered-tests.json'
        archive.write_text(json.dumps([{key: value for key, value in row.items()
                                       if key != 'env'} for row in self.registry]))
        self.assertNotEqual(package.sha(archive), self.gate['registry_sha256'])
        package.require_full_aggregate_coverage(self.gate, self.build)
        self.gate['registry_sha256'] = package.sha(archive)
        with self.assertRaisesRegex(RuntimeError, 'current Meson test registry'):
            package.require_full_aggregate_coverage(self.gate, self.build)

    def test_wrong_commit_or_tested_executable_is_rejected(self):
        self.gate['source_commit'] = 'wrong'
        with self.assertRaisesRegex(RuntimeError, 'exact source commit'):
            self.verify()
        self.gate['source_commit'] = self.commit
        (self.build/'app/gimp-3.0').write_text('different executable')
        with self.assertRaisesRegex(RuntimeError, 'exact tested executable'):
            self.verify()

    def test_old_or_incomplete_gate_is_rejected(self):
        for seal in [None, {}, {'before': {'format': 1}, 'after': {'format': 1}},
                     {'before': self.snapshot}]:
            self.gate['build_freshness'] = seal
            with self.assertRaisesRegex(RuntimeError, 'before/after'):
                self.verify()

    def test_state_changed_during_aggregate_is_rejected(self):
        self.gate['build_freshness']['after']['build_outputs']['resource.dat']['sha256'] = 'different'
        with self.assertRaisesRegex(RuntimeError, 'aggregate changed'):
            self.verify()

    def test_source_change_is_rejected_before_build(self):
        (self.repo/'version.txt').write_text('changed source\n')
        with mock.patch.object(package.subprocess, 'run', wraps=subprocess.run) as run:
            with self.assertRaisesRegex(RuntimeError, 'pre-build changed'):
                self.verify()
            self.assertNotIn(mock.call(['ninja', '-C', str(self.build)], capture_output=True, text=True), run.call_args_list)

    def test_untracked_source_is_rejected(self):
        (self.repo/'new.c').write_text('new source')
        with self.assertRaisesRegex(RuntimeError, 'source_inventory'):
            self.verify()

    def test_resource_or_secondary_output_change_is_rejected(self):
        for root, name in [(self.repo, 'installed.txt'), (self.build, 'resource.dat'),
                           (self.repo/'gimp-data', 'asset.txt')]:
            path = root/name
            original = path.read_bytes()
            path.write_bytes(b'changed resource')
            with self.assertRaisesRegex(RuntimeError, 'pre-build changed'):
                self.verify()
            path.write_bytes(original)

    def test_changed_command_is_rejected_even_if_output_would_be_identical(self):
        graph = self.build/'build.ninja'
        graph.write_text(graph.read_text().replace('command = cp $in $out', 'command = cp $in $out && true'))
        with self.assertRaisesRegex(RuntimeError, 'ninja_commands_sha256'):
            self.verify()

    def test_new_graph_output_is_rejected(self):
        graph = self.build/'build.ninja'
        graph.write_text(graph.read_text() + 'build extra.dat: copy ../resource.txt\n')
        with self.assertRaisesRegex(RuntimeError, 'build_outputs'):
            self.verify()

    def test_missing_output_is_rejected(self):
        (self.build/'resource.dat').unlink()
        with self.assertRaisesRegex(RuntimeError, 'pre-build changed'):
            self.verify()

    def test_mode_change_is_rejected(self):
        (self.build/'resource.dat').chmod(0o755)
        with self.assertRaisesRegex(RuntimeError, 'pre-build changed'):
            self.verify()

    def test_symlink_change_is_rejected(self):
        resource = self.build/'resource.dat'
        resource.unlink()
        resource.symlink_to('../resource.txt')
        with self.assertRaisesRegex(RuntimeError, 'pre-build changed'):
            self.verify()

    def test_configuration_and_command_payload_changes_are_rejected(self):
        for name in ['config.h', 'meson-info/intro-install_plan.json']:
            path = self.build/name
            original = path.read_text()
            path.write_text(original + '\n')
            with self.assertRaisesRegex(RuntimeError, 'build_metadata'):
                self.verify()
            path.write_text(original)

    def test_installed_tree_addition_and_circular_link_are_rejected(self):
        resource = self.repo/'installed.txt'
        resource.unlink()
        resource.mkdir()
        (resource/'one').write_text('first resource')
        expected = package.capture_freshness_snapshot(self.build, self.repo)
        (resource/'two').write_text('new resource')
        with self.assertRaisesRegex(RuntimeError, 'install_inputs'):
            package.verify_build_freshness(self.build, self.repo, expected)
        (resource/'loop').symlink_to('.')
        with self.assertRaisesRegex(RuntimeError, 'Circular'):
            package.capture_freshness_snapshot(self.build, self.repo)

    def test_changed_bytes_during_convergence_require_retest(self):
        with mock.patch.dict(os.environ, {'PACKAGE_FIXTURE_VERSION': 'changed during build'}):
            with self.assertRaisesRegex(RuntimeError, 'default build changed'):
                self.verify()
        self.assertEqual((self.build/'app/gimp-3.0').read_text(), 'changed during build')

    def test_failed_ninja_check_is_rejected(self):
        with mock.patch.dict(os.environ, {'PACKAGE_FIXTURE_FAIL': '1'}):
            with self.assertRaisesRegex(RuntimeError, 'convergence failed'):
                self.verify()

    def test_bookkeeping_and_timestamp_churn_are_not_artifact_changes(self):
        # Ninja normally appends/recompacts these files; the graph and artifact
        # identities are independently sealed instead of sealing log timestamps.
        (self.build/'.ninja_log').touch()
        (self.build/'.ninja_deps').write_bytes(b'fixture bookkeeping')
        (self.build/'resource.dat').touch()
        self.assertEqual(self.snapshot, package.capture_freshness_snapshot(self.build, self.repo))


if __name__ == '__main__':
    unittest.main()
