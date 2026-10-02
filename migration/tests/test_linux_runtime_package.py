#!/usr/bin/env python3
"""Small package recipe safety tests; these do not claim application coverage."""
import importlib.util
import os
import re
from pathlib import Path
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


if __name__ == '__main__':
    unittest.main()
