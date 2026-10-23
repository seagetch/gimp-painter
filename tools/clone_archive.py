#!/usr/bin/env python3
"""Bounded, hash-pinned storage for the unchanged strict Clone receipt contract.

Only a single XZ stream containing a flat tar of PUBLIC_FILES and receipt.json is
accepted. Extraction never calls tarfile.extract(): every header and byte count
is checked before writing into a private temporary directory. Archive storage
is not a new evidence format; validate_receipt still verifies the original bytes.
The acceptance route is {archive, archive_sha256, receipt_sha256}; archive is a
canonical repository-relative .tar.xz path and both pins are SHA-256 hex strings.
"""
import hashlib
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import stat
import tarfile
import tempfile

from clone_evidence import PUBLIC_FILES, ROOT, validate_receipt

MEMBERS = PUBLIC_FILES | {'receipt.json'}
MAX_COMPRESSED_BYTES = 32 * 1024 * 1024
MAX_EXPANDED_BYTES = 256 * 1024 * 1024
MAX_MEMBER_BYTES = 64 * 1024 * 1024
XZ_MEMORY_LIMIT = 128 * 1024 * 1024
CHUNK_BYTES = 1024 * 1024
BLOCK_BYTES = 512


def _hash_pin(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None


def archive_path(root, name):
    """Require one canonical, contained repository path for an archive route."""
    if (not isinstance(name, str) or not name or '\\' in name or
            PurePosixPath(name).is_absolute() or
            any(part in ('', '.', '..') for part in name.split('/')) or
            not name.endswith('.tar.xz')):
        raise ValueError('Invalid current core archive path')
    root = Path(root).resolve()
    path = root / name
    if not path.resolve().is_relative_to(root):
        raise ValueError('Current core archive escapes the source checkout')
    return path


def _decompress(archive, expected_sha, expanded):
    """Hash the exact consumed compressed bytes and cap decoder/output memory."""
    if not _hash_pin(expected_sha):
        raise ValueError('Invalid current archive hash pin')
    descriptor = os.open(archive, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(descriptor, 'rb') as source:
        # Reject FIFOs/devices without blocking, and never follow archive links.
        # The open descriptor, rather than a separate path stat, is the input.
        info = os.fstat(source.fileno())
        if not stat.S_ISREG(info.st_mode):
            raise ValueError('Current archive must be a regular file')
        if info.st_size > MAX_COMPRESSED_BYTES:
            raise ValueError('Current archive exceeds compressed size limit')
        decoder = lzma.LZMADecompressor(format=lzma.FORMAT_XZ,
                                        memlimit=XZ_MEMORY_LIMIT)
        compressed_hash = hashlib.sha256()
        compressed_bytes = expanded_bytes = 0
        while True:
            data = source.read(min(CHUNK_BYTES, MAX_COMPRESSED_BYTES - compressed_bytes + 1))
            if not data:
                break
            compressed_bytes += len(data)
            if compressed_bytes > MAX_COMPRESSED_BYTES:
                raise ValueError('Current archive exceeds compressed size limit')
            compressed_hash.update(data)
            if decoder.eof:
                raise ValueError('Trailing data or concatenated XZ streams')
            while True:
                output = decoder.decompress(data, max_length=min(
                    CHUNK_BYTES, MAX_EXPANDED_BYTES - expanded_bytes + 1))
                data = b''
                expanded_bytes += len(output)
                if expanded_bytes > MAX_EXPANDED_BYTES:
                    raise ValueError('Current archive exceeds expanded size limit')
                expanded.write(output)
                if decoder.eof:
                    if decoder.unused_data:
                        raise ValueError('Trailing data or concatenated XZ streams')
                    break
                if decoder.needs_input:
                    break
        if not decoder.eof:
            raise ValueError('Truncated XZ stream')
        if compressed_hash.hexdigest() != expected_sha:
            raise ValueError('Current archive hash mismatch')
    expanded.seek(0)


def _extract_flat_tar(expanded, directory):
    """Reject extension/link/sparse headers, aliases, duplicates and extra data."""
    seen = set()
    while True:
        header = expanded.read(BLOCK_BYTES)
        if len(header) != BLOCK_BYTES:
            raise ValueError('Truncated tar header or missing end marker')
        if header == bytes(BLOCK_BYTES):
            second = expanded.read(BLOCK_BYTES)
            if second != bytes(BLOCK_BYTES):
                raise ValueError('Invalid tar end marker')
            while True:
                padding = expanded.read(CHUNK_BYTES)
                if not padding:
                    break
                if len(padding) % BLOCK_BYTES or any(padding):
                    raise ValueError('Unexpected data after tar end marker')
            break
        entry = tarfile.TarInfo.frombuf(header, 'utf-8', 'strict')
        name = entry.name
        if (name not in MEMBERS or '/' in name or '\\' in name or
                name in ('.', '..') or PurePosixPath(name).is_absolute()):
            raise ValueError('Unknown or unsafe archive member: ' + name)
        if name in seen:
            raise ValueError('Duplicate archive member: ' + name)
        if entry.type not in (tarfile.REGTYPE, tarfile.AREGTYPE) or entry.linkname:
            raise ValueError('Archive member is not a plain regular file: ' + name)
        if entry.size < 0 or entry.size > MAX_MEMBER_BYTES:
            raise ValueError('Archive member exceeds size limit: ' + name)
        seen.add(name)
        with (directory / name).open('xb') as output:
            remaining = entry.size
            while remaining:
                data = expanded.read(min(CHUNK_BYTES, remaining))
                if not data:
                    raise ValueError('Truncated archive member: ' + name)
                output.write(data)
                remaining -= len(data)
        size = -entry.size % BLOCK_BYTES
        padding = expanded.read(size)
        if len(padding) != size or any(padding):
            raise ValueError('Invalid archive member padding: ' + name)
    if seen != MEMBERS:
        raise ValueError('Missing expected archive members: ' + ', '.join(sorted(MEMBERS - seen)))


def validate_archive(archive, root=ROOT, expected_sha=None, receipt_sha=None):
    """Return the strict directory-validator result after bounded extraction.

    Both hashes are mandatory. Only this exact verified archive storage path is
    allowed as a post-run tracked addition; source/receipt checks are unchanged.
    """
    errors = []
    result = {'errors': errors, 'sources_sha256': {}, 'normal_stdout': '', 'sanitizer_stdout': ''}
    try:
        archive, root = Path(archive), Path(root).resolve()
        if not _hash_pin(receipt_sha):
            raise ValueError('Invalid current receipt hash pin')
        if not archive.name.endswith('.tar.xz'):
            raise ValueError('Current archive must use .tar.xz storage')
        with tempfile.TemporaryDirectory(prefix='clone-receipt-') as temporary:
            directory = Path(temporary) / 'receipt'
            directory.mkdir()
            with tempfile.TemporaryFile() as expanded:
                _decompress(archive, expected_sha, expanded)
                _extract_flat_tar(expanded, directory)
            return validate_receipt(directory, root, receipt_sha,
                                    storage_paths=(archive.resolve(),))
    except (OSError, ValueError, TypeError, EOFError, lzma.LZMAError,
            tarfile.TarError, UnicodeError) as error:
        errors.append('Invalid current Clone archive: ' + str(error))
    return result
