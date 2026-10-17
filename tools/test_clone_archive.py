#!/usr/bin/env python3
"""Standalone mocked archive controls; no native application is executed."""
import hashlib
import io
import json
import lzma
import os
import subprocess
import tarfile
import unittest
from unittest.mock import patch

import check_clone_acceptance as acceptance
import clone_archive as archive
import test_clone_evidence as evidence_controls
from clone_evidence import validate_receipt


class ArchiveControls(unittest.TestCase):
    def setUp(self):
        self.fixture = evidence_controls.ReceiptControls()
        self.fixture.setUp()
        self.root, self.output = self.fixture.root, self.fixture.output
        self.archive = self.root / 'current.tar.xz'
        self.members = [(name, (self.output / name).read_bytes(), tarfile.REGTYPE, '')
                        for name in sorted(archive.MEMBERS)]
        self.receipt_sha = hashlib.sha256((self.output / 'receipt.json').read_bytes()).hexdigest()
        self.pack()

    def tearDown(self):
        self.fixture.tearDown()

    def raw_tar(self, members=None):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode='w', format=tarfile.USTAR_FORMAT) as tar:
            for name, content, kind, link in self.members if members is None else members:
                info = tarfile.TarInfo(name)
                info.type, info.linkname = kind, link
                info.size = len(content)
                tar.addfile(info, io.BytesIO(content))
        return buffer.getvalue()

    def pack(self, members=None, raw=None):
        data = lzma.compress(self.raw_tar(members) if raw is None else raw)
        self.archive.write_bytes(data)
        self.archive_sha = hashlib.sha256(data).hexdigest()

    def validate(self, **kwargs):
        return archive.validate_archive(self.archive, self.root,
            kwargs.get('archive_sha', self.archive_sha), kwargs.get('receipt_sha', self.receipt_sha))

    def assertRejected(self, **kwargs):
        self.assertTrue(self.validate(**kwargs)['errors'])

    def test_positive_preserves_exact_receipt_and_evidence(self):
        direct = validate_receipt(self.output, self.root, self.receipt_sha)
        wrapped = self.validate()
        self.assertEqual(direct['errors'], [])
        self.assertEqual(wrapped, direct)
        self.assertEqual(wrapped['receipt']['files'], self.fixture.receipt['files'])

    def test_only_exact_tracked_archive_storage_is_allowed(self):
        subprocess.run(['git', '-C', str(self.root), 'add', 'current.tar.xz'], check=True)
        self.assertEqual(self.validate()['errors'], [])
        (self.root / 'unexpected-source.c').write_text('unsealed input')
        subprocess.run(['git', '-C', str(self.root), 'add', 'unexpected-source.c'], check=True)
        self.assertTrue(any('unsealed additional' in error for error in self.validate()['errors']))

    def test_original_directory_storage_preserved(self):
        destination = self.root / 'evidence'
        self.output.rename(destination)
        subprocess.run(['git', '-C', str(self.root), 'add', 'evidence'], check=True)
        self.assertEqual(validate_receipt(destination, self.root, self.receipt_sha)['errors'], [])

    def test_archive_receipt_and_artifact_hash_tampering(self):
        self.assertRejected(archive_sha='0' * 64)
        self.assertRejected(receipt_sha='0' * 64)
        changed = [(name, b'[]' if name == 'normal-results.json' else data, kind, link)
                   for name, data, kind, link in self.members]
        self.pack(changed)
        self.assertRejected()

    def test_source_and_runtime_failures_still_rejected(self):
        source = self.root / 'app/core/gimpclonelayer.cpp'
        source.write_text('tampered native source')
        self.assertRejected()
        source.write_text('/* mocked control input */\n')
        normal = json.loads((self.output / 'normal-results.json').read_text())
        normal[0]['returncode'] = 1
        self.fixture.write('normal-results.json', normal)
        self.fixture.seal()
        self.members = [(name, (self.output / name).read_bytes(), tarfile.REGTYPE, '')
                        for name in sorted(archive.MEMBERS)]
        self.receipt_sha = hashlib.sha256((self.output / 'receipt.json').read_bytes()).hexdigest()
        self.pack()
        self.assertRejected()

    def test_missing_extra_and_duplicate_members(self):
        for members in (self.members[:-1], self.members + [('extra', b'', tarfile.REGTYPE, '')],
                        self.members + [self.members[0]]):
            with self.subTest(names=[entry[0] for entry in members]):
                self.pack(members)
                self.assertRejected()

    def test_absolute_traversal_nested_and_alias_members(self):
        for name in ('/receipt.json', '../receipt.json', 'a/../receipt.json',
                     'a/receipt.json', './receipt.json', 'a\\receipt.json'):
            with self.subTest(name=name):
                self.pack(self.members + [(name, b'', tarfile.REGTYPE, '')])
                self.assertRejected()
        self.assertFalse((self.fixture.base / 'receipt.json').exists())

    def test_links_directories_devices_and_extensions(self):
        for kind in (tarfile.SYMTYPE, tarfile.LNKTYPE, tarfile.DIRTYPE,
                     tarfile.FIFOTYPE, tarfile.CHRTYPE, tarfile.BLKTYPE,
                     tarfile.XHDTYPE, tarfile.XGLTYPE, tarfile.GNUTYPE_LONGNAME,
                     tarfile.GNUTYPE_LONGLINK, tarfile.GNUTYPE_SPARSE,
                     tarfile.CONTTYPE):
            with self.subTest(kind=kind):
                name = self.members[0][0]
                self.pack([(name, b'', kind, 'outside' if kind in (tarfile.SYMTYPE, tarfile.LNKTYPE) else '')]
                          + self.members[1:])
                self.assertRejected()

    def test_compressed_expanded_member_and_decoder_memory_limits(self):
        for constant, bound in (('MAX_COMPRESSED_BYTES', self.archive.stat().st_size - 1),
                                ('MAX_EXPANDED_BYTES', len(self.raw_tar()) - 1),
                                ('MAX_MEMBER_BYTES', 1), ('XZ_MEMORY_LIMIT', 1)):
            with self.subTest(limit=constant), patch.object(archive, constant, bound):
                self.assertRejected()

    def test_exact_size_limits_succeed(self):
        with patch.object(archive, 'MAX_COMPRESSED_BYTES', self.archive.stat().st_size), \
             patch.object(archive, 'MAX_EXPANDED_BYTES', len(self.raw_tar())), \
             patch.object(archive, 'MAX_MEMBER_BYTES', max(len(row[1]) for row in self.members)):
            self.assertEqual(self.validate()['errors'], [])

    def test_malformed_truncated_and_concatenated_xz(self):
        valid = self.archive.read_bytes()
        for data in (b'', b'not xz', valid[:-1], valid + b'trailing', valid + valid,
                     bytes([valid[0] ^ 1]) + valid[1:]):
            with self.subTest(size=len(data)):
                self.archive.write_bytes(data)
                self.archive_sha = hashlib.sha256(data).hexdigest()
                self.assertRejected()

    def test_multichunk_decoding_and_separate_trailing_stream(self):
        with patch.object(archive, 'CHUNK_BYTES', 512):
            self.assertEqual(self.validate()['errors'], [])
        valid = self.archive.read_bytes()
        self.archive.write_bytes(valid + valid)
        self.archive_sha = hashlib.sha256(valid + valid).hexdigest()
        with patch.object(archive, 'CHUNK_BYTES', len(valid)):
            self.assertRejected()

    def test_malformed_tar_headers_end_markers_and_padding(self):
        valid = self.raw_tar()
        consumed = sum(512 + ((len(row[1]) + 511) // 512) * 512 for row in self.members)
        corrupt_checksum = bytes([valid[0] ^ 1]) + valid[1:]
        # First file length is not block-aligned in the synthetic fixture.
        padding = 512 + len(self.members[0][1])
        self.assertNotEqual(padding % 512, 0)
        for raw in (b'', valid[:511], corrupt_checksum, valid[:consumed],
                    valid[:consumed + 512], valid + b'nonzero', valid + bytes(1),
                    valid[:padding] + b'X' + valid[padding + 1:],
                    valid[:consumed + 512] + b'X' + valid[consumed + 513:]):
            with self.subTest(size=len(raw)):
                self.pack(raw=raw)
                self.assertRejected()

    def test_archive_symlink_and_fifo_rejected_without_opening(self):
        saved = self.root / 'saved.tar.xz'
        self.archive.rename(saved)
        self.archive.symlink_to(saved)
        self.assertRejected()
        self.archive.unlink()
        os.mkfifo(self.archive)
        self.assertRejected()

    def test_invalid_or_missing_hash_pins(self):
        for pin in (None, '', 'F' * 64, 'a' * 63, 123, []):
            with self.subTest(pin=pin):
                self.assertRejected(archive_sha=pin)
                self.assertRejected(receipt_sha=pin)

    def test_canonical_contained_archive_route(self):
        self.assertEqual(archive.archive_path(self.root, 'current.tar.xz'), self.archive)
        for path in ('', '/absolute.tar.xz', '../parent.tar.xz', './current.tar.xz',
                     'a//current.tar.xz', 'a/../current.tar.xz', 'a\\current.tar.xz',
                     'wrong.tar.gz', None, 123):
            with self.subTest(path=path), self.assertRaises(ValueError):
                archive.archive_path(self.root, path)
        (self.root / 'outside').symlink_to(self.fixture.base, target_is_directory=True)
        with self.assertRaises(ValueError):
            archive.archive_path(self.root, 'outside/current.tar.xz')

    def test_acceptance_dispatch_preserves_directory_and_adds_archive(self):
        matrix = json.loads((acceptance.ROOT / 'migration/acceptance/clone.json').read_text())
        # A sentinel result stops evidence reuse, while the spies prove dispatch.
        sentinel = {'errors': ['mocked receipt rejection']}
        with patch.object(acceptance, 'validate_receipt', return_value=sentinel) as directory, \
             patch.object(acceptance, 'validate_archive', return_value=sentinel) as compressed:
            matrix['current_core_receipt'] = {'directory': 'old-path', 'receipt_sha256': self.receipt_sha}
            errors = acceptance.validate(matrix=matrix)
            self.assertIn('mocked receipt rejection', errors)
            directory.assert_called_once_with(acceptance.ROOT / 'old-path', acceptance.ROOT, self.receipt_sha)
            compressed.assert_not_called()
            directory.reset_mock()
            matrix['current_core_receipt'] = {'archive': 'current.tar.xz',
                'archive_sha256': self.archive_sha, 'receipt_sha256': self.receipt_sha}
            errors = acceptance.validate(matrix=matrix)
            self.assertIn('mocked receipt rejection', errors)
            compressed.assert_called_once_with(acceptance.ROOT / 'current.tar.xz', acceptance.ROOT,
                                               self.archive_sha, self.receipt_sha)
            directory.assert_not_called()
        for route in ({'archive': 'current.tar.xz'},
                      dict(matrix['current_core_receipt'], directory='ambiguous'),
                      dict(matrix['current_core_receipt'], archive='../outside.tar.xz')):
            matrix['current_core_receipt'] = route
            self.assertTrue(any('Invalid current core receipt route' in error
                                for error in acceptance.validate(matrix=matrix)))


if __name__ == '__main__':
    unittest.main()
