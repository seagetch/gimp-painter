#!/usr/bin/env python3
"""Focused integrity and path containment tests for fixture materialization."""
import hashlib
import io
import json
from pathlib import Path
import tarfile
import tempfile
import unittest

from filter_context_fixture_bundle import materialize


class BundleTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.archive = self.root / "fixture.tar.gz"
        self.manifest = self.root / "fixture.json"
        self.output = self.root / "output"

    def tearDown(self):
        self.temporary.cleanup()

    def make(self, entries):
        members = {}
        with tarfile.open(self.archive, "w:gz") as archive:
            for name, data, kind in entries:
                item = tarfile.TarInfo(name)
                item.type = kind
                item.size = len(data)
                archive.addfile(item, io.BytesIO(data))
                members[name] = dict(size=len(data), sha256=hashlib.sha256(data).hexdigest())
        self.manifest.write_text(json.dumps(dict(schema_version=1, archive=self.archive.name,
            archive_sha256=hashlib.sha256(self.archive.read_bytes()).hexdigest(), members=members,
            member_count=len(members), total_bytes=sum(item['size'] for item in members.values()))))

    def test_exact_bytes(self):
        self.make([("fixture/pixels.raw", bytes(range(256)), tarfile.REGTYPE)])
        materialize(self.archive, self.manifest, self.output)
        self.assertEqual((self.output / "fixture/pixels.raw").read_bytes(), bytes(range(256)))

    def test_changed_archive(self):
        self.make([("fixture/a", b"abc", tarfile.REGTYPE)])
        with self.archive.open("ab") as stream:
            stream.write(b"changed")
        with self.assertRaises(ValueError):
            materialize(self.archive, self.manifest, self.output)

    def test_parent_escape(self):
        self.make([("../escaped", b"abc", tarfile.REGTYPE)])
        with self.assertRaises(ValueError):
            materialize(self.archive, self.manifest, self.output)
        self.assertFalse((self.root / "escaped").exists())

    def test_symlink(self):
        self.make([("fixture/link", b"", tarfile.SYMTYPE)])
        with self.assertRaises(ValueError):
            materialize(self.archive, self.manifest, self.output)

    def test_duplicate_member(self):
        self.make([("fixture/a", b"abc", tarfile.REGTYPE)] * 2)
        with self.assertRaises(ValueError):
            materialize(self.archive, self.manifest, self.output)

    def test_changed_member_hash(self):
        self.make([("fixture/a", b"abc", tarfile.REGTYPE)])
        document = json.loads(self.manifest.read_text())
        document['members']['fixture/a']['sha256'] = '0' * 64
        self.manifest.write_text(json.dumps(document))
        with self.assertRaises(ValueError):
            materialize(self.archive, self.manifest, self.output)

    def test_nonempty_destination(self):
        self.make([("fixture/a", b"abc", tarfile.REGTYPE)])
        self.output.mkdir()
        (self.output / "existing").write_text("retained")
        with self.assertRaises(ValueError):
            materialize(self.archive, self.manifest, self.output)
        self.assertEqual((self.output / "existing").read_text(), "retained")


if __name__ == "__main__":
    unittest.main()
