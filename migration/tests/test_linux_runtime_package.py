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

batch_spec=importlib.util.spec_from_file_location('filter_batch',REPO/'migration/tests/filter-package-smoke.py')
filter_batch=importlib.util.module_from_spec(batch_spec)
batch_spec.loader.exec_module(filter_batch)


class InstalledFilterTests(unittest.TestCase):
    def test_missing_runtime_always_restores_on_observer_error(self):
        smoke = runtime_test.filter_smoke
        for kind in ('helper', 'plugin'):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                target = root/kind
                target.write_bytes(b'original runtime executable')
                target.chmod(0o755)
                with mock.patch.object(smoke, 'run', side_effect=RuntimeError('observer failed')):
                    with self.assertRaisesRegex(RuntimeError, 'observer failed'):
                        smoke.run_missing_runtime(['host'], {}, {kind:target}, root/'trial', kind)
                self.assertEqual(target.read_bytes(), b'original runtime executable')
                self.assertEqual(target.stat().st_mode & 0o777, 0o755)
                self.assertFalse((root/('trial.disabled-'+kind)).exists())

    def test_missing_runtime_reaches_host_with_isolated_profile(self):
        smoke = runtime_test.filter_smoke
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            executables = {kind:root/kind for kind in ('helper', 'plugin')}
            for path in executables.values():
                path.write_bytes(b'original executable')
            profiles = []
            def observed(command, environment, expected, output, missing_runtime):
                self.assertFalse(expected[missing_runtime].exists())
                self.assertTrue(expected['plugin' if missing_runtime == 'helper' else 'helper'].exists())
                self.assertEqual(command, ['actual-host'])
                self.assertTrue(Path(environment['HOME']).is_dir())
                self.assertNotEqual(environment['GIMP_PAINTER_PROFILE'], 'ambient-profile')
                profiles.append(environment['GIMP_PAINTER_PROFILE'])
                output.mkdir()
                return {'status':'passed'}
            with mock.patch.object(smoke, 'run', side_effect=observed):
                for kind in ('helper', 'plugin'):
                    report = smoke.run_missing_runtime(['actual-host'], {'GIMP_PAINTER_PROFILE':'ambient-profile'},
                                                       executables, root/('trial-'+kind), kind)
                    self.assertEqual(report['restored_executable_sha256'], smoke.sha(executables[kind]))
            self.assertEqual(len(set(profiles)), 2)

    def test_filter_capsule_reads_verified_fixture_and_rejects_truncation(self):
        oracle = json.loads((REPO/'migration/fixtures/legacy-blinds-package-smoke.json').read_text())
        fixture = REPO/oracle['fixture']
        self.assertEqual(package.sha(fixture), oracle['fixture_sha256'])
        capsule = filter_batch.filter_capsule(fixture)
        self.assertIn(b'saved-state', capsule)
        self.assertIn(b'plug-in-blinds', capsule)
        with tempfile.TemporaryDirectory() as directory:
            broken = Path(directory)/'truncated.xcf'
            broken.write_bytes(fixture.read_bytes()[:80])
            with self.assertRaises(AssertionError):
                filter_batch.filter_capsule(broken)


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


class DependencyFixtures:
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.work = Path(self.directory.name)
        self.directories = [self.work/name for name in ['primary', 'http', 'extra']]
        for directory in self.directories:
            (directory/'root').mkdir(parents=True)
        self.lib = Path('usr/lib')/package.TRIPLET/'libfixture.so.0'

    def put(self, index, relative, data=b'fixture'):
        path = self.directories[index]/relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path


class DependencyRootTests(DependencyFixtures, unittest.TestCase):
    def test_secondary_needed_library_retains_resolved_archive_path(self):
        target = self.put(1, Path('root')/(str(self.lib)+'.1'))
        link = target.with_name('libfixture.so.0')
        link.symlink_to(target.name)
        source, relative, origin = package.dependency_library(self.directories, link.name, link)
        self.assertEqual(source, link)
        self.assertEqual(relative, self.lib)
        self.assertEqual(origin['dependency_root'], 1)
        self.assertEqual(origin['matching_dependency_roots'], [1])
        self.assertEqual(origin['archive_path'], str(target.relative_to(self.directories[1]/'root')))
        self.assertEqual(origin['sha256'], package.sha(target))

    def test_repeatable_roots_and_identical_collisions_follow_declared_order(self):
        relative = Path('root')/self.lib
        first = self.put(1, relative)
        self.put(2, relative)
        directories = package.dependency_directories(self.directories[0], self.directories[1:])
        source, origin = package.dependency_file(directories, relative)
        self.assertEqual(source, first)
        self.assertEqual(origin['matching_dependency_roots'], [1, 2])
        primary = self.put(0, relative)
        source, origin = package.dependency_file(directories, relative)
        self.assertEqual(source, primary)
        self.assertEqual(origin['matching_dependency_roots'], [0, 1, 2])

    def test_single_root_resolution_is_unchanged(self):
        source = self.put(0, Path('root')/self.lib)
        selected, relative, origin = package.dependency_library([self.directories[0]], source.name, source)
        self.assertEqual(selected, source)
        self.assertEqual(relative, self.lib)
        self.assertEqual(origin['dependency_root'], 0)

    def test_inconsistent_library_bytes_modes_and_links_are_rejected(self):
        relative = Path('root')/self.lib
        self.put(0, relative)
        other = self.put(1, relative, b'different')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies'):
            package.dependency_file(self.directories, relative)
        other.write_bytes(b'fixture')
        other.chmod(0o755)
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies'):
            package.dependency_file(self.directories, relative)
        other.unlink()
        target = self.put(1, Path('root')/(str(self.lib)+'.1'))
        other.symlink_to(target.name)
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies'):
            package.dependency_file(self.directories, relative)

    def test_soname_collision_in_other_search_directory_is_rejected(self):
        source = self.put(0, Path('root')/self.lib)
        self.put(1, Path('root')/self.lib.parent/'blas'/self.lib.name, b'other ABI')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency SONAME'):
            package.dependency_library(self.directories, source.name, source)

    def test_missing_and_host_non_abi_libraries_are_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Dependency file missing'):
            package.dependency_library(self.directories, self.lib.name, self.directories[1]/'root'/self.lib)
        with self.assertRaisesRegex(RuntimeError, 'Non-baseline host dependency'):
            package.dependency_library(self.directories, self.lib.name, Path('/usr/lib')/self.lib.name)
        self.assertIsNone(package.dependency_library(self.directories, 'libc.so.6', Path('/usr/lib/libc.so.6')))

    def test_dependency_link_cannot_resolve_to_host_or_another_root(self):
        outside = self.put(1, Path('root')/self.lib)
        link = self.directories[0]/'root'/self.lib
        link.parent.mkdir(parents=True)
        link.symlink_to(outside)
        with self.assertRaisesRegex(RuntimeError, 'escapes root'):
            package.dependency_library(self.directories, link.name, link)

    def test_absolute_usr_library_link_uses_extracted_target(self):
        target = self.put(1, Path('root')/(str(self.lib)+'.1'), b'extracted library')
        link = target.with_name(self.lib.name)
        link.symlink_to('/'+str(self.lib)+'.1')
        source, relative, origin = package.dependency_library(self.directories, link.name, link)
        self.assertEqual(source, link)
        self.assertEqual(relative, self.lib)
        self.assertEqual(origin['sha256'], package.sha(target))
        self.assertEqual(origin['resolved_path'], 'root/'+str(self.lib)+'.1')
        self.assertEqual(origin['archive_path'], str(self.lib)+'.1')

    def test_matching_absolute_links_compare_each_roots_actual_bytes(self):
        for index in [0, 1]:
            target = self.put(index, Path('root')/(str(self.lib)+'.1'), b'matching library')
            target.with_name(self.lib.name).symlink_to('/'+str(self.lib)+'.1')
        source, origin = package.dependency_file(self.directories, Path('root')/self.lib)
        self.assertEqual(source, self.directories[0]/'root'/self.lib)
        self.assertEqual(origin['matching_dependency_roots'], [0, 1])
        target.write_bytes(b'inconsistent library')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies'):
            package.dependency_file(self.directories, Path('root')/self.lib)

    def test_absolute_usr_asset_chain_relocates_without_host_lookup(self):
        relative = Path('usr/share/fixture')
        target = self.put(1, Path('root')/relative/'target', b'extracted asset')
        (target.parent/'first').symlink_to('/'+str(relative/'second'))
        (target.parent/'second').symlink_to('/'+str(relative/'relative'))
        (target.parent/'relative').symlink_to('target')
        runtime, origins = self.work/'runtime', {}
        package.copy_dependency_tree(self.directories, relative, runtime, origins)
        package.relative_symlinks(runtime)
        for name in ['first', 'second', 'relative']:
            staged = runtime/relative/name
            self.assertTrue(staged.is_symlink())
            self.assertFalse(Path(os.readlink(staged)).is_absolute())
            self.assertEqual(staged.read_bytes(), b'extracted asset')
            self.assertEqual(staged.resolve(), runtime/relative/'target')
        self.assertEqual(origins[str(relative/'first')]['resolved_path'], 'root/'+str(relative/'target'))

    def test_absolute_usr_links_never_use_existing_host_file(self):
        host = Path('/usr/bin/python3')
        self.assertTrue(host.is_file())
        target = self.put(1, 'root/usr/bin/python3', b'declared-root-only')
        link = self.put(1, 'root/usr/share/fixture/link')
        link.unlink()
        link.symlink_to(host)
        _, origin = package.dependency_file(self.directories, 'root/usr/share/fixture/link')
        self.assertEqual(origin['sha256'], package.sha(target))
        self.assertNotEqual(origin['sha256'], package.sha(host))
        target.unlink()
        with self.assertRaisesRegex(RuntimeError, 'missing or not regular'):
            package.dependency_file(self.directories, 'root/usr/share/fixture/link')

    def test_absolute_usr_intermediate_directory_link_is_rooted(self):
        target = self.put(1, 'root/usr/share/fixture/actual/file')
        (target.parent.parent/'alias').symlink_to('/usr/share/fixture/actual')
        source, origin = package.dependency_file(self.directories, 'root/usr/share/fixture/alias/file')
        self.assertEqual(source, target)
        self.assertEqual(origin['resolved_path'], 'root/usr/share/fixture/actual/file')

    def test_absolute_usr_link_cycles_and_escapes_are_rejected(self):
        directory = self.directories[1]/'root/usr/share/fixture'
        directory.mkdir(parents=True)
        first, second = directory/'first', directory/'second'
        first.symlink_to('/usr/share/fixture/second')
        second.symlink_to('first')
        with self.assertRaisesRegex(RuntimeError, 'symlink cycle'):
            package.dependency_file(self.directories, 'root/usr/share/fixture/first')
        second.unlink()
        for destination in ['/etc/passwd', '../../../../outside', '/usr/../../outside']:
            with self.subTest(destination=destination):
                second.symlink_to(destination)
                with self.assertRaisesRegex(RuntimeError, 'escapes root'):
                    package.dependency_file(self.directories, 'root/usr/share/fixture/first')
                second.unlink()

    def test_allowlisted_trees_merge_only_matching_files(self):
        relative = Path('usr/share/glib-2.0/schemas')
        self.put(0, Path('root')/relative/'common.xml')
        self.put(1, Path('root')/relative/'common.xml')
        self.put(1, Path('root')/relative/'http.xml')
        self.put(1, Path('root')/relative/'unused.pyc')
        runtime, origins = self.work/'runtime', {}
        package.copy_dependency_tree(self.directories, relative, runtime, origins)
        self.assertEqual(sorted(p.name for p in (runtime/relative).iterdir()), ['common.xml', 'http.xml'])
        self.assertEqual(origins[str(relative/'common.xml')]['matching_dependency_roots'], [0, 1])
        self.assertEqual(origins[str(relative/'http.xml')]['dependency_root'], 1)
        self.put(1, Path('root')/relative/'common.xml', b'inconsistent')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies'):
            package.copy_dependency_tree(self.directories, relative, runtime, {})

    def test_tree_file_directory_collision_is_rejected(self):
        relative = Path('usr/share/fixture')
        self.put(0, Path('root')/relative)
        self.put(1, Path('root')/relative/'child')
        with self.assertRaisesRegex(RuntimeError, 'not regular'):
            package.copy_dependency_tree(self.directories, relative, self.work/'runtime', {})

    def test_missing_or_duplicate_root_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Dependency root missing'):
            package.dependency_directories(self.work/'missing')
        with self.assertRaisesRegex(RuntimeError, 'Duplicate dependency directory'):
            package.dependency_directories(self.directories[0], [self.directories[0]])


@unittest.skipUnless(shutil.which('dpkg-deb'), 'dpkg-deb is required for archive provenance fixtures')
class DependencyNoticeTests(DependencyFixtures, unittest.TestCase):
    def setUp(self):
        super().setUp()
        self.package_name = 'libfixture'
        self.notice_relative = Path('root/usr/share/doc/libfixture/copyright')
        self.notice = self.put(1, self.notice_relative, b'Fixture copyright notice\n')
        self.put(1, Path('root')/self.lib)
        control = self.put(1, 'root/DEBIAN/control',
            b'Package: libfixture\nVersion: 1.0\nArchitecture: amd64\n'
            b'Maintainer: Fixture <fixture@example.invalid>\n'
            b'Source: fixture-source (1.0-1)\nDescription: Packaging test fixture\n')
        self.archive_relative = Path('apt/archives/libfixture_1.0_amd64.deb')
        self.archive = self.directories[1]/self.archive_relative
        self.archive.parent.mkdir(parents=True)
        subprocess.run(['dpkg-deb', '--build', str(control.parents[1]), str(self.archive)],
                       check=True, capture_output=True)
        self.item = {'package': self.package_name, 'version': '1.0', 'architecture': 'amd64',
                     'filename': self.archive.name, 'sha256': package.sha(self.archive)}
        self.bundle = self.work/'bundle'
        self.bundle.mkdir()

    def notices(self, packages=None):
        return package.runtime_package_notices(self.directories,
            packages or {self.archive.name: self.item}, {str(self.lib)}, self.bundle)

    def test_secondary_archive_and_notice_are_both_recorded(self):
        owner = self.notices()[self.package_name]
        self.assertEqual(owner['archive_origin']['dependency_root'], 1)
        self.assertEqual(owner['archive_origin']['path'], str(self.archive_relative))
        self.assertEqual(owner['archive_origin']['sha256'], self.item['sha256'])
        self.assertEqual(owner['copyright_origin']['dependency_root'], 1)
        self.assertEqual(owner['copyright_origin']['path'], str(self.notice_relative))
        self.assertEqual(owner['copyright_sha256'], package.sha(self.notice))
        self.assertEqual(owner['source_package'], 'fixture-source')
        self.assertEqual(owner['source_version'], '1.0-1')
        self.assertEqual(owner['bundled_files'], [str(self.lib)])
        self.assertEqual((self.bundle/owner['copyright']).read_bytes(), self.notice.read_bytes())

    def test_matching_archives_and_notices_choose_first_root(self):
        self.put(0, self.archive_relative, self.archive.read_bytes())
        self.put(0, self.notice_relative, self.notice.read_bytes())
        owner = self.notices()[self.package_name]
        for field in ['archive_origin', 'copyright_origin']:
            self.assertEqual(owner[field]['dependency_root'], 0)
            self.assertEqual(owner[field]['matching_dependency_roots'], [0, 1])

    def test_missing_archive_is_rejected(self):
        self.archive.unlink()
        with self.assertRaisesRegex(RuntimeError, 'Dependency file missing: apt/archives'):
            self.notices()

    def test_missing_notice_is_rejected(self):
        self.notice.unlink()
        with self.assertRaisesRegex(RuntimeError, 'Dependency file missing: root/usr/share/doc'):
            self.notices()

    def test_absolute_usr_notice_link_copies_declared_root_bytes(self):
        notice_bytes = self.notice.read_bytes()
        target = self.put(1, 'root/usr/share/doc/fixture-source/copyright', notice_bytes)
        self.notice.unlink()
        self.notice.symlink_to('/usr/share/doc/fixture-source/copyright')
        owner = self.notices()[self.package_name]
        self.assertEqual((self.bundle/owner['copyright']).read_bytes(), notice_bytes)
        self.assertEqual(owner['copyright_origin']['resolved_path'], str(target.relative_to(self.directories[1])))

    def test_conflicting_archive_copy_is_rejected(self):
        self.put(0, self.archive_relative, b'wrong archive')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies: apt/archives'):
            self.notices()

    def test_conflicting_notice_copy_is_rejected(self):
        self.put(0, self.notice_relative, b'unrelated notice')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting dependency copies: root/usr/share/doc'):
            self.notices()

    def test_wrong_locked_checksum_is_rejected(self):
        self.item['sha256'] = 'wrong checksum'
        with self.assertRaisesRegex(RuntimeError, 'Locked archive checksum mismatch'):
            self.notices()

    def test_ambiguous_package_archives_cannot_overwrite_provenance(self):
        other = {**self.item, 'filename': 'libfixture_other_amd64.deb', 'version': 'other'}
        self.put(1, Path('apt/archives')/other['filename'], self.archive.read_bytes())
        with self.assertRaisesRegex(RuntimeError, 'Ambiguous runtime package archives'):
            self.notices({self.archive.name: self.item, other['filename']: other})


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
