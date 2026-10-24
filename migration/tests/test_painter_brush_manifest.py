"""Explicit asset-list safety, legacy selection, and installed-byte tests."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('manifest', ROOT / 'tools/generate_painter_brush_manifest.py')
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.lock = manifest.load_lock(manifest.LOCK.read_bytes())
        self.rows = manifest.records(self.lock)

    def test_exact_tree_and_reproducible_list(self):
        manifest.validate(manifest.ASSETS, self.rows)
        self.assertEqual((manifest.ASSETS / 'meson.build').read_text(), manifest.render(self.rows))
        self.assertEqual(manifest.render(list(reversed(self.rows))), manifest.render(self.rows))

    def test_legacy_selection_and_additional_preserved_assets(self):
        # Ruby Dir.glob subdir/* + .sort selected only these two suffixes.
        selected = sorted(r['path'] for r in self.rows
                          if '/' in r['path'] and r['path'].endswith(('.myb', '_prev.png')))
        self.assertEqual(len(selected), 354)
        names = {r['path'] for r in self.rows}
        self.assertEqual(len(names), 363)
        self.assertEqual(sum(n.endswith('.myb') for n in names), 177)
        self.assertEqual(names - set(selected), {'FX_blender_prev.png',
            'deevad/readme.txt', 'kaerhon/ReadMe.txt',
            *[n + '/files' for n in ['classic', 'deevad', 'experimental', 'kaerhon', 'ramon', 'tanda']]})

    def test_independent_pinned_git_census(self):
        prefix = 'data/mypaint-brushes/'
        paths = subprocess.check_output(['git', '-C', str(ROOT), 'ls-tree', '-r', '--name-only',
            self.lock['source_commit'], '--', prefix], text=True).splitlines()
        old_selected = sorted(p[len(prefix):] for p in paths
            if len(Path(p[len(prefix):]).parts) == 2 and p.endswith(('.myb', '_prev.png')))
        current = sorted(r['path'] for r in self.rows
            if len(Path(r['path']).parts) == 2 and r['path'].endswith(('.myb', '_prev.png')))
        self.assertEqual(current, old_selected)
        self.assertEqual(len(current), 354)

    def test_invalid_paths_and_duplicate_rejected(self):
        for name in ['../outside', '/absolute', "bad'quote", 'bad\\name', 'bad\nname', 'a//b', 'meson.build']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                manifest.records({'assets': [dict(self.rows[0], path=name)]})
        with self.assertRaises(ValueError):
            manifest.records({'assets': [self.rows[0], self.rows[0]]})

    def test_changed_lock_rejected_and_independently_sealed(self):
        seal = json.loads((ROOT / 'migration/fixtures/legacy-mypaint/manifest.json').read_text())
        self.assertEqual(seal['files']['assets.json']['sha256'], manifest.LOCK_SHA256)
        for key, value in [('source_commit', '0' * 40), ('source_root', 'other'), ('assets', self.rows[:-1])]:
            changed = dict(self.lock, **{key: value})
            with self.subTest(key=key), self.assertRaises(ValueError):
                manifest.load_lock(json.dumps(changed).encode())

    def test_space_and_unicode_paths_are_literal(self):
        rows = [dict(self.rows[0], path='group name/筆 test.myb')]
        text = manifest.render(rows)
        self.assertIn("'group name/筆 test.myb',", text)
        self.assertIn("'painter-mypaint-brushes/group name'", text)

    def test_missing_extra_changed_and_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            data = b'asset'
            row = dict(path='a.myb', bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
            with self.assertRaises(ValueError): manifest.validate(p, [row])
            (p / 'a.myb').write_bytes(data)
            manifest.validate(p, [row])
            (p / 'extra').write_bytes(data)
            with self.assertRaises(ValueError): manifest.validate(p, [row])
            (p / 'extra').unlink()
            (p / 'a.myb').write_bytes(b'wrong')
            with self.assertRaises(ValueError): manifest.validate(p, [row])
            (p / 'a.myb').unlink()
            (p / 'a.myb').symlink_to('/not-read')
            with self.assertRaises(ValueError): manifest.validate(p, [row])

    def test_real_meson_install(self):
        # Standalone asset-only Meson project exercises the same generated file;
        # no full application build, system install or mutable source tree.
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            source = p / 'source'
            source.mkdir()
            shutil.copytree(manifest.ASSETS, source / 'assets')
            (source / 'meson.build').write_text("project('painter-assets')\ngimpdatadir = 'share/gimp/3.0'\nsubdir('assets')\n")
            for command in [['meson', 'setup', str(p / 'build'), str(source), '--prefix=/usr'],
                            ['meson', 'install', '-C', str(p / 'build'), '--destdir', str(p / 'dest')]]:
                subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            dest = p / 'dest/usr/share/gimp/3.0/painter-mypaint-brushes'
            manifest.validate(dest, self.rows)
            self.assertEqual({f.relative_to(dest).as_posix() for f in dest.rglob('*') if f.is_file()},
                             {r['path'] for r in self.rows})

    def test_real_meson_space_unicode_install(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            source = p / 'source'
            source.mkdir()
            data = b'literal path fixture'
            row = dict(path='group name/筆 test.myb', bytes=len(data),
                       sha256=hashlib.sha256(data).hexdigest())
            asset = source / row['path']
            asset.parent.mkdir()
            asset.write_bytes(data)
            (source / 'meson.build').write_text("project('painter-literal-path')\ngimpdatadir = 'share/gimp/3.0'\n" + manifest.render([row]))
            for command in [['meson', 'setup', str(p / 'build'), str(source), '--prefix=/usr'],
                            ['meson', 'install', '-C', str(p / 'build'), '--destdir', str(p / 'dest')]]:
                subprocess.run(command, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            manifest.validate(p / 'dest/usr/share/gimp/3.0/painter-mypaint-brushes', [row])


if __name__ == '__main__':
    unittest.main()
