#!/usr/bin/env python3
"""Overlay one reviewed diagnostic test on an immutable production checkout.

This is diagnostic-only and never eligible for baseline acceptance. No donor
production files, other tests, or source-guard changes are imported. Initialize
pinned submodules first; keep builds and the receipt outside the source checkout.
Use --verify RECEIPT after compilation/testing to reject further source drift.
Pass the same --diagnostic-mode to preparation and verification. The receipt
declares required runtime settings; this helper does not enable or run them.
The donor is the separately pinned historical diagnostic checkout, never the
evolving normal candidate. Only its frozen test file is imported.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import stat
import subprocess


SOURCE_COMMIT = '3d6f62235c6bc9e2505d2c2b6ad1d42e15d081af'
SOURCE_TREE = 'a5a14a81a10857834301888a0cd2b3cf902bf17d'
DONOR_COMMIT = '1618cc5958148c7d534f9e3893681e14fcc94d80'
DONOR_TREE = '253fcbdeb7c39391ddf8bad89b7877e1e18911b8'
DONOR_TEST_BLOB = 'e3b3b28d1da397282dface38c04ed43d9fe1f0c7'
ORIGINAL_TEST_SHA256 = '49eb5a701b4f0a082c1f88f6a2f3a34546c738dfec4de1d28bf7989b12d5d664'
# Reviewed opt-in instrumentation; changing these bytes requires a new review.
INSTRUMENTED_TEST_SHA256 = '78079b8209f0bb4a06f9c44ef15a2ee393c3d917da7ecad213cee8b259c61152'
TEST_PATH = 'app/tests/test-common-layer-popup.cpp'
DIAGNOSTIC_MODES = ('passive', 'controlled-order')
RUNTIME_FLAG = 'GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES'


def git(repository, *args):
    environment = os.environ.copy()
    environment['GIT_NO_REPLACE_OBJECTS'] = '1'
    return subprocess.check_output(['git', '-C', str(repository), *args],
                                   env=environment)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def require_instrumented_pin():
    value = INSTRUMENTED_TEST_SHA256
    if (not isinstance(value, str) or len(value) != 64 or
            any(character not in '0123456789abcdef' for character in value) or
            value == ORIGINAL_TEST_SHA256):
        raise ValueError('Reviewed instrumented test SHA-256 is not pinned')


def expected_receipt(diagnostic_mode='passive'):
    if diagnostic_mode not in DIAGNOSTIC_MODES:
        raise ValueError('Unknown diagnostic mode: ' + str(diagnostic_mode))
    require_instrumented_pin()
    return {
        'format': 3,
        'diagnostic_mode': diagnostic_mode,
        'runtime_flag': RUNTIME_FLAG,
        # None means unset, not an empty string or a value of "0".
        'runtime_flag_required_value': '1' if diagnostic_mode == 'controlled-order' else None,
        'source_commit': SOURCE_COMMIT,
        'source_tree': SOURCE_TREE,
        'donor_commit': DONOR_COMMIT,
        'donor_tree': DONOR_TREE,
        'donor_test_blob': DONOR_TEST_BLOB,
        'donor_scope': 'Only the frozen test is imported; donor production and other worktree bytes are not imported or attested',
        'original_test_sha256': ORIGINAL_TEST_SHA256,
        'instrumented_test_sha256': INSTRUMENTED_TEST_SHA256,
        'only_allowed_path': TEST_PATH,
        'diagnostic_only': True,
        'acceptance_eligible': False,
        'source_guard_suppressed': False,
        'scope': 'Immutable production and all other tests; one reviewed diagnostic test overlay only',
        'native_execution': 'not established by preparing or verifying this overlay',
    }


def checked_path(root, relative):
    path = Path(relative)
    if path.is_absolute() or '..' in path.parts or not path.parts:
        raise ValueError('Unsafe tracked path: ' + relative)
    current = root
    for part in path.parts[:-1]:
        current /= part
        if current.is_symlink() or not current.is_dir():
            raise ValueError('Missing or symlinked source parent: ' + relative)
    return root / path


def tree_entries(repository):
    entries = {}
    for record in git(repository, 'ls-tree', '-r', '-z', '--full-tree', 'HEAD').split(b'\0'):
        if not record:
            continue
        header, path = record.split(b'\t', 1)
        mode, kind, object_id = header.decode('ascii').split()
        relative = os.fsdecode(path)
        if relative in entries:
            raise ValueError('Duplicate tracked path: ' + relative)
        entries[relative] = (mode, kind, object_id)
    return entries


def check_index(repository, entries):
    expected_index = sorted((mode, object_id, '0', path)
                            for path, (mode, _kind, object_id) in entries.items())
    actual_index = []
    for record in git(repository, 'ls-files', '--stage', '-z').split(b'\0'):
        if record:
            header, path = record.split(b'\t', 1)
            mode, object_id, stage = header.decode('ascii').split()
            actual_index.append((mode, object_id, stage, os.fsdecode(path)))
    if sorted(actual_index) != expected_index:
        raise ValueError('Checkout index differs from the immutable tree')


def check_repository(repository, expected_commit, expected_tree=None, instrumented=False):
    if Path(os.fsdecode(git(repository, 'rev-parse', '--show-toplevel')).strip()).resolve() != repository:
        raise ValueError('Source must be the root of its own initialized checkout')
    if git(repository, 'rev-parse', 'HEAD').decode().strip() != expected_commit:
        raise ValueError('Wrong immutable source commit: ' + str(repository))
    if expected_tree and git(repository, 'rev-parse', 'HEAD^{tree}').decode().strip() != expected_tree:
        raise ValueError('Wrong immutable source tree')

    entries = tree_entries(repository)
    check_index(repository, entries)
    # No exclude flags: ignored files are drift too. Build outside the checkout.
    if git(repository, 'ls-files', '--others', '-z'):
        raise ValueError('Unexpected untracked or ignored source files')

    for relative, (mode, kind, object_id) in entries.items():
        path = checked_path(repository, relative)
        if mode == '160000' and kind == 'commit':
            if path.is_symlink() or not path.is_dir():
                raise ValueError('Missing or symlinked pinned submodule: ' + relative)
            check_repository(path.resolve(), object_id)
            continue
        if kind != 'blob' or mode not in {'100644', '100755', '120000'}:
            raise ValueError('Unsupported source entry: ' + relative)
        if mode == '120000':
            if not path.is_symlink():
                raise ValueError('Tracked symlink drift: ' + relative)
            data = os.fsencode(os.readlink(path))
        else:
            if path.is_symlink() or not path.is_file():
                raise ValueError('Missing or non-regular tracked source: ' + relative)
            executable = bool(path.stat().st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH))
            if executable != (mode == '100755'):
                raise ValueError('Tracked source mode drift: ' + relative)
            data = path.read_bytes()
        if instrumented and relative == TEST_PATH:
            if mode != '100644' or sha(data) != INSTRUMENTED_TEST_SHA256:
                raise ValueError('Instrumented diagnostic test content drift')
        else:
            blob = hashlib.sha1(b'blob ' + str(len(data)).encode('ascii') + b'\0' + data).hexdigest()
            if blob != object_id:
                raise ValueError('Unreviewed tracked source drift: ' + relative)
    return entries


def check_source(source, instrumented=False):
    require_instrumented_pin()
    entries = check_repository(source, SOURCE_COMMIT, SOURCE_TREE, instrumented)
    if TEST_PATH not in entries or entries[TEST_PATH][:2] != ('100644', 'blob'):
        raise ValueError('Original diagnostic test is not a regular tracked source file')
    original = git(source, 'show', SOURCE_COMMIT + ':' + TEST_PATH)
    if sha(original) != ORIGINAL_TEST_SHA256:
        raise ValueError('Frozen original diagnostic test SHA-256 changed')
    return original


def check_donor(donor):
    """Read only the frozen test from the exact historical diagnostic donor.

    The whole donor index must match its pinned tree, but unrelated donor files
    and submodules are not read, imported, or attested as build inputs.
    """
    require_instrumented_pin()
    if Path(os.fsdecode(git(donor, 'rev-parse', '--show-toplevel')).strip()).resolve() != donor:
        raise ValueError('Donor must be the root of its own checkout')
    if git(donor, 'rev-parse', 'HEAD').decode().strip() != DONOR_COMMIT:
        raise ValueError('Wrong immutable diagnostic donor commit')
    if git(donor, 'rev-parse', 'HEAD^{tree}').decode().strip() != DONOR_TREE:
        raise ValueError('Wrong immutable diagnostic donor tree')
    entries = tree_entries(donor)
    if entries.get(TEST_PATH) != ('100644', 'blob', DONOR_TEST_BLOB):
        raise ValueError('Wrong frozen donor test tree entry or Git blob')
    check_index(donor, entries)
    frozen = git(donor, 'cat-file', 'blob', DONOR_TEST_BLOB)
    if sha(frozen) != INSTRUMENTED_TEST_SHA256:
        raise ValueError('Frozen donor test Git blob SHA-256 changed')
    donor_test = checked_path(donor, TEST_PATH)
    if donor_test.is_symlink() or not donor_test.is_file():
        raise ValueError('Donor diagnostic test must be a regular file')
    if donor_test.stat().st_mode & (stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH):
        raise ValueError('Donor diagnostic test mode drift')
    instrumented = donor_test.read_bytes()
    if instrumented != frozen or sha(instrumented) != INSTRUMENTED_TEST_SHA256:
        raise ValueError('Donor instrumented diagnostic test content drift')
    return instrumented


def verify_worktree(source, receipt, diagnostic_mode='passive'):
    # Canonical JSON comparison rejects extra fields and bool/int substitutions.
    if json.dumps(receipt, sort_keys=True) != json.dumps(expected_receipt(diagnostic_mode), sort_keys=True):
        raise ValueError('Wrong or tampered diagnostic overlay receipt')
    check_source(source, instrumented=True)
    return True


def prepare(source, donor, output, diagnostic_mode='passive'):
    source, donor, output = source.resolve(), donor.resolve(), output.resolve()
    receipt = expected_receipt(diagnostic_mode)
    if source == donor:
        raise ValueError('Donor and immutable source must be separate directories')
    if output == source or source in output.parents:
        raise ValueError('Diagnostic receipt must be outside the source checkout')
    if output == donor or donor in output.parents:
        raise ValueError('Diagnostic receipt must be outside the donor checkout')
    if output.exists() or not output.parent.is_dir():
        raise ValueError('Receipt must be a new file in an existing external directory')
    original = check_source(source)
    instrumented = check_donor(donor)
    target = checked_path(source, TEST_PATH)
    if target.stat().st_nlink != 1:
        raise ValueError('Refusing to modify a hard-linked diagnostic test')
    try:
        target.write_bytes(instrumented)
        verify_worktree(source, receipt, diagnostic_mode)
        with output.open('x', encoding='utf-8') as stream:
            stream.write(json.dumps(receipt, indent=2) + '\n')
    except Exception:
        target.write_bytes(original)
        raise
    return receipt


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate receipt field: ' + key)
        result[key] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--donor', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--verify', type=Path)
    parser.add_argument('--diagnostic-mode', choices=DIAGNOSTIC_MODES, default='passive',
                        help='Expected runtime mode; passive requires the early-updates flag unset')
    args = parser.parse_args()
    if args.verify:
        if args.donor or args.output:
            parser.error('--verify cannot be combined with --donor or --output')
        receipt = json.loads(args.verify.read_text(), object_pairs_hook=unique_object)
        verify_worktree(args.source.resolve(), receipt, args.diagnostic_mode)
        print(f'Immutable production and {args.diagnostic_mode} diagnostic-only test overlay are unchanged; acceptance_eligible=false')
    else:
        if not args.donor or not args.output:
            parser.error('--donor and --output are required without --verify')
        prepare(args.source, args.donor, args.output, args.diagnostic_mode)
        print(f'Prepared {args.diagnostic_mode} diagnostic-only overlay; native execution unverified; acceptance_eligible=false')


if __name__ == '__main__':
    main()
