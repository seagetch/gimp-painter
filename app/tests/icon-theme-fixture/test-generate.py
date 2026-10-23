#!/usr/bin/env python3
"""Focused file/manifest unit checks; never launch GTK or touch real profiles."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

spec = importlib.util.spec_from_file_location('fixture', Path(__file__).with_name('generate.py'))
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'source'; self.root.mkdir()
        self.output = self.base / 'build/theme'
        self.stamp = self.base / 'build/ready.h'
        (self.root / 'asset').write_bytes(b'production-byte-fixture')
        (self.root / 'recipe').write_bytes(b'reviewed-recipe')
        self.plan = self.base / 'plan.tsv'
        self.plan.write_text('mode\tvector\ninput\trecipe\nfile\tasset\tDefault/scalable/apps/icon.svg\n')

    def build(self):
        return fixture.generate(self.root, self.plan, self.output, self.stamp)

    def verify(self):
        return fixture.verify(self.root, self.plan, self.output, self.stamp)

    def test_copies_exact_bytes(self):
        self.assertEqual(self.build()['files'], 1)
        self.assertEqual((self.output / 'Default/scalable/apps/icon.svg').read_bytes(),
                         (self.root / 'asset').read_bytes())
        self.assertEqual(self.verify()['status'], 'verified')

    def test_repeated_build_preserves_stamp_and_manifest(self):
        first = self.build(); mtime = self.stamp.stat().st_mtime_ns
        self.assertEqual(first, self.build())
        self.assertEqual(mtime, self.stamp.stat().st_mtime_ns)

    def test_source_change_invalidates_manifest_and_stamp(self):
        first = self.build()
        (self.root / 'asset').write_bytes(b'changed-production-bytes')
        with self.assertRaisesRegex(ValueError, 'current production inputs'):
            self.verify()
        self.assertNotEqual(first['stamp_sha256'], self.build()['stamp_sha256'])

    def test_recipe_change_invalidates(self):
        first = self.build()
        (self.root / 'recipe').write_bytes(b'new-recipe')
        self.assertNotEqual(first['stamp_sha256'], self.build()['stamp_sha256'])

    def test_recovers_deleted_asset(self):
        first = self.build()
        (self.output / 'Default/scalable/apps/icon.svg').unlink()
        self.assertEqual(first, self.build())

    def test_alias_preserves_actual_source_bytes(self):
        self.plan.write_text(self.plan.read_text() + 'file\tasset\tDefault/scalable/apps/alias.svg\n')
        self.assertEqual(self.build()['files'], 2)
        self.assertEqual((self.output / 'Default/scalable/apps/alias.svg').read_bytes(),
                         (self.root / 'asset').read_bytes())

    def test_membership_change_removes_only_owned_old_file(self):
        self.build()
        self.plan.write_text('mode\tvector\nfile\tasset\tDefault/renamed.svg\n')
        self.build()
        self.assertFalse((self.output / 'Default/scalable/apps/icon.svg').exists())
        self.assertTrue((self.output / 'Default/renamed.svg').is_file())

    def test_rejects_output_traversal(self):
        self.plan.write_text('mode\tvector\nfile\tasset\t../escape.svg\n')
        with self.assertRaisesRegex(ValueError, 'unsafe fixture output'):
            self.build()

    def test_rejects_reserved_manifest_destination(self):
        self.plan.write_text('mode\tvector\nfile\tasset\tmanifest.json\n')
        with self.assertRaisesRegex(ValueError, 'unsafe fixture output'):
            self.build()

    def test_same_source_duplicate_destination_is_deduplicated(self):
        self.plan.write_text(self.plan.read_text() + 'file\tasset\tDefault/scalable/apps/icon.svg\n')
        self.assertEqual(self.build()['files'], 1)

    def test_rejects_source_escape(self):
        (self.base / 'outside').write_bytes(b'outside')
        self.plan.write_text('mode\tvector\nfile\t../outside\tDefault/a.svg\n')
        with self.assertRaisesRegex(ValueError, 'source escapes'):
            self.build()

    def test_rejects_conflicting_destination(self):
        (self.root / 'other').write_bytes(b'other')
        self.plan.write_text(self.plan.read_text() + 'file\tother\tDefault/scalable/apps/icon.svg\n')
        with self.assertRaisesRegex(ValueError, 'conflicting fixture destination'):
            self.build()

    def test_rejects_unowned_output(self):
        self.output.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError, 'unowned fixture'):
            self.build()

    def test_interrupted_first_generation_recovers_owned_directory(self):
        original = fixture.write_changed
        def interrupted(path, data):
            if path.suffix == '.svg':
                raise OSError('simulated interrupted first copy')
            return original(path, data)
        with mock.patch.object(fixture, 'write_changed', side_effect=interrupted):
            with self.assertRaisesRegex(OSError, 'interrupted first copy'):
                self.build()
        self.assertTrue((self.output / fixture.OWNER_FILE).is_file())
        self.assertFalse((self.output / 'manifest.json').exists())
        self.assertEqual(self.build()['status'], 'verified')
        self.assertEqual(self.verify()['files'], 1)

    def test_partial_manifest_recovers_only_owned_fixture(self):
        self.build()
        (self.output / 'manifest.json').write_bytes(b'{"format":')
        self.assertEqual(self.build()['status'], 'verified')

    def test_rejects_unexpected_copied_file(self):
        self.build(); (self.output / 'unknown').write_bytes(b'unknown')
        with self.assertRaisesRegex(ValueError, 'unexpected fixture files'):
            self.verify()

    def test_rejects_missing_source(self):
        (self.root / 'asset').unlink()
        with self.assertRaisesRegex(ValueError, 'missing fixture input'):
            self.build()

    def test_does_not_claim_bitmap_recipe(self):
        self.plan.write_text('mode\tbitmap\nfile\tasset\tDefault/a.png\n')
        with self.assertRaisesRegex(ValueError, 'only the reviewed vector'):
            self.build()


if __name__ == '__main__':
    unittest.main()
