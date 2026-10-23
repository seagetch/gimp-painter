#!/usr/bin/env python3
"""Authoritative ordinary-Meson gate; historical focused acceptance is separate.

Read-only Python/Git validation only. Never configures, builds, executes native
programs, repairs evidence, or consults recorded build/profile paths on disk.
"""
from __future__ import annotations

import argparse
from collections import Counter
import copy
import hashlib
import importlib.util
import json
import lzma
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys
import tarfile
import tempfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
COMMIT = '47202852286b654d88d3fa4f0bafd59e08030cad'
TREE = '47edf23ea7f95390751c76d649acb9429d38ff61'
RUNNER_SHA256 = '607de99f2babee17d82157d8e2ea7813c749b86162fc38494c3ec83294d647f9'
SCHEMA = 'ordinary-meson-clone-navigation-sealed-runtime-continuation-v1'
POLICY_SCHEMA = 'ordinary-meson-runtime-policy-disclosure-v1'
POLICY_PREFIX = 'run-004-policy-disclosure'
POLICY_HELPER = 'ordinary_policy_disclosure.py'
POLICY_HELPER_SHA256 = '130697009b55ea005c1442f40f11822df34b637e783869c966e53f969115056e'
POLICY_ATTESTATION_SHA256 = '94a2c8ef01b3821ef55e2870225a7663b2faeae1424621153e12a1a563867345'
POLICY_VALIDATION_SHA256 = '30fa892889e82a97cc6416c529f76e668260fcda38eeda584f7b5fc363236c54'
EXECUTION_ARCHIVE = 'run-004-validation-blocked-public.tar.xz'
EXECUTION_ARCHIVE_SHA256 = '5efd2b33103b8547c2b2cac9279643556f39894e90cf58a4345420acffe0b4b4'
EXECUTION_RECEIPT_SHA256 = 'db0cbf6086c25eb82314ff649a8b01aa5079e8dec349c1b697377f90b854186f'
EXECUTION_VALIDATION_SHA256 = '31f4b130c7b0afe225a3fcecf5f8146a6855d3be9766460bd482c4046d595393'
POLICY_FAILURE = ['RuntimeError: Runtime sanitizer policy weakened']
POLICY_FILES = {POLICY_HELPER: POLICY_HELPER_SHA256, 'policy-disclosure.json': POLICY_ATTESTATION_SHA256,
                'policy-disclosure-validation.json': POLICY_VALIDATION_SHA256,
                EXECUTION_ARCHIVE: EXECUTION_ARCHIVE_SHA256}
ORIGIN_FILE = 'origin-run-002-failed-public.tar.xz'
ORIGIN_ARCHIVE_SHA256 = '752cf49969332012193d8332f0b2664ef9214b45ef4ea239656a6d118d6469b1'
ORIGIN_RECEIPT_SHA256 = 'a109a310afa189edf6f90f4dbac0b0ca9b366f81a28c20565451f5fbb748e46f'
ORIGIN_RUNNER_SHA256 = 'aa92752dd5f2e7329e647edd15b76d981c6985eac57c29fd683013f46eb54d46'
RESOURCE_ORIGIN_FILE = 'origin-run-003-failed-public.tar.xz'
RESOURCE_ORIGIN_ARCHIVE_SHA256 = '1c3b60f98c8f228f65d8ab325bdd912af2b2931dad144cf298fa31a53703ddb5'
RESOURCE_ORIGIN_RECEIPT_SHA256 = '7ffa7de608ba5d79ad0c950b9d41e3812d3fe84ce0ec2070cf4c57d18ed39bb9'
RESOURCE_ORIGIN_RUNNER_SHA256 = '82f3a68c38fb27d98f90dbc7bac4b5bd06fdf1c0dd993b53a5f6541f9b32099f'
MENU_OUTPUTS = {'menus/dockable-menu.ui', 'menus/image-menu.ui', 'menus/validate_ui_menus-output.xml'}
MENU_SOURCES = {'menus/meson.build', 'menus/menus.xsl', 'menus/dialogs-menuitems.ui.in',
                'menus/dockable-menu.ui.in.in', 'menus/image-menu.ui.in.in'}
ARCHIVE = 'migration/tests/ordinary-meson-clone-navigation-evidence.tar.xz'
CHECKER = 'tools/check_ordinary_meson_acceptance.py'
TESTS = 'tools/test_ordinary_meson_acceptance.py'
NEW_FILES = {CHECKER, TESTS, ARCHIVE}
DOCUMENTS = {'tasks.md', 'migration/progress.tsv', 'migration/contracts/clone-layer-port.md',
             'migration/acceptance/clone.json'}
HELPERS = {'tools/clone_evidence.py', 'tools/run-painter-frozen-aggregate.py',
           'tools/package-linux-runtime.py', 'tools/run-painter-xvfb.py'}
NORMAL = {'core': 4, 'projection-update-bounds': 3, 'gimp-clone-layer': 62,
          'painter-layer-ui': 68, 'painter-layer-tree': 35, 'painter-xcf-roundtrip': 49,
          'update-geometry': 5, 'painter-navigation-events': 31,
          'painter-navigation-ui': 4, 'painter-navigation': 6}
SANITIZER = {k: NORMAL[k] for k in ('gimp-clone-layer', 'painter-navigation-events',
                                 'painter-navigation-ui', 'painter-navigation')}
REGISTRATIONS = {'app/tests/test-' + k + '.c' for k in NORMAL} | {'app/tests/test-gimp-clone-layout.cpp'}
METADATA = {'compile_commands.json', 'config.h', 'meson-info/intro-tests.json',
            'meson-info/intro-targets.json', 'meson-info/intro-buildoptions.json',
            'meson-info/intro-compilers.json', 'meson-info/intro-dependencies.json',
            'meson-info/intro-install_plan.json', 'meson-info/intro-installed.json',
            'meson-info/meson-info.json', 'meson-info/intro-machines.json'}
ORIGIN_COMMANDS = {'normal-default-build.log', 'normal-meson-console.log', 'create-detached-source.log',
            'materialize-pinned-data.log', 'sanitizer-configure.log',
            'sanitizer-ordinary-object-closure.log', 'sanitizer-selected-links.log',
            'sanitizer-declared-resources.log', 'sanitizer-meson-console.log'}
ORIGIN_PUBLIC_MEMBERS = ({'receipt.json', 'ordinary_meson_validation.py', 'expected-tests.json', 'toolchain.json',
            'source-integrity.json', 'elf-integrity.json', 'freshness.json', 'locales.json',
            'metadata.json', 'transforms.json', 'sanitizer-closure-commands.log',
            'sanitizer-closure-compile-database.json', 'sanitizer-declared-prerequisites.json',
            'instrumentation.json', 'sanitizer-build.ninja'} | ORIGIN_COMMANDS | {'helpers/' + p for p in HELPERS}
           | {'registration-sources/' + p for p in REGISTRATIONS}
           | {label + '-' + p.replace('/', '-') for label in ('normal', 'sanitizer') for p in METADATA}
           | {label + '-' + p for label in ('normal', 'sanitizer')
              for p in ('results.json', 'gate.json', 'loader.json', 'runtime-policy.json', 'testlog.txt')})
ORIGIN_PUBLIC_MEMBERS -= {'sanitizer-gate.json', 'sanitizer-loader.json'}
ORIGIN_ORDER = tuple('run-002/public/' + name for name in sorted(ORIGIN_PUBLIC_MEMBERS)) + (
    'run-002/validation.json', 'run-002/preservation-audit.json')
ORIGIN_MEMBERS = set(ORIGIN_ORDER)
RESOURCE_ORIGIN_COMMANDS = {'sanitizer-declared-menu-resources.log', 'normal-meson-console.log'}
RESOURCE_ORIGIN_PUBLIC_MEMBERS = ((ORIGIN_PUBLIC_MEMBERS - ORIGIN_COMMANDS
    - {'sanitizer-declared-prerequisites.json', 'normal-gate.json', 'normal-loader.json',
       'sanitizer-results.json', 'sanitizer-runtime-policy.json', 'sanitizer-testlog.txt'})
    | RESOURCE_ORIGIN_COMMANDS | {ORIGIN_FILE, 'menu-resource-plan.json',
       'resource-transition.json', 'resource-inputs.json'}
    | {'menu-source-inputs/' + name for name in MENU_SOURCES})
RESOURCE_ORIGIN_ORDER = tuple('run-003/public/' + name for name in sorted(RESOURCE_ORIGIN_PUBLIC_MEMBERS)) + (
    'run-003/validation.json', 'run-003/preservation-audit.json')
RESOURCE_ORIGIN_MEMBERS = set(RESOURCE_ORIGIN_ORDER)
COMMANDS = {'normal-meson-console.log', 'sanitizer-meson-console.log'}
EXECUTION_PUBLIC_MEMBERS = ((RESOURCE_ORIGIN_PUBLIC_MEMBERS - {ORIGIN_FILE, 'resource-transition.json',
            'sanitizer-declared-menu-resources.log'}) | COMMANDS | {RESOURCE_ORIGIN_FILE,
            'sealed-resources.json', 'normal-gate.json', 'normal-loader.json',
            'sanitizer-gate.json', 'sanitizer-loader.json', 'sanitizer-results.json',
            'sanitizer-runtime-policy.json', 'sanitizer-testlog.txt'})
EXECUTION_ORDER = tuple('run-004/public/' + name for name in sorted(EXECUTION_PUBLIC_MEMBERS)) + (
    'run-004/validation.json', 'run-004/preservation-audit.json')
EXECUTION_MEMBERS = set(EXECUTION_ORDER)
MEMBERS = {POLICY_PREFIX + '/' + name for name in POLICY_FILES}
TRANSFORM_MEMBERS = (COMMANDS | {'sanitizer-build.ninja', 'sanitizer-closure-commands.log'}
                     | {label + '-' + p.replace('/', '-') for label in ('normal', 'sanitizer') for p in METADATA}
                     | {label + '-' + p for label in ('normal', 'sanitizer') for p in ('results.json', 'testlog.txt')})
# Four-file recovery envelope retains the exact original archive once. Fixed
# limits also bound every level of the immutable execution/provenance chain.
MAX_COMPRESSED = 8 * 1024 * 1024
MAX_EXPANDED = 8 * 1024 * 1024
MAX_MEMBER = 8 * 1024 * 1024
# Measured run-004: 7,409,096 compressed; 175,242,504 member bytes;
# largest member 51,471,324 bytes.
EXECUTION_MAX_COMPRESSED = 8 * 1024 * 1024
EXECUTION_MAX_EXPANDED = 192 * 1024 * 1024
EXECUTION_MAX_MEMBER = 64 * 1024 * 1024
# Measured immutable origin: 2,481,160 compressed; 164,816,334 member bytes;
# largest member 46,431,404 bytes. Preserve its exact original member order.
ORIGIN_MAX_COMPRESSED = 4 * 1024 * 1024
ORIGIN_MAX_EXPANDED = 192 * 1024 * 1024
ORIGIN_MAX_MEMBER = 64 * 1024 * 1024
# Measured immutable resource origin: 4,924,928 compressed; 154,898,368
# member bytes; largest member 41,395,461 bytes. Nested build origin is unchanged.
RESOURCE_ORIGIN_MAX_COMPRESSED = 8 * 1024 * 1024
RESOURCE_ORIGIN_MAX_EXPANDED = 160 * 1024 * 1024
RESOURCE_ORIGIN_MAX_MEMBER = 48 * 1024 * 1024
XZ_MEMORY = 128 * 1024 * 1024
CHUNK = 1024 * 1024
PHASES = {'before_build', 'after_build', 'before_tests', 'after_tests', 'after'}
DIRECT = ('14.007/component-visibility-regression', '14.007/source-mode-projection-regression',
          *('25.%03d' % i for i in range(1, 9)), '25.008/mirrored-arrow-input',
          '25.009/common-angle-draw', '25.009/transformed-drag-events', '25.009/fractional-pan-difference')
GUI = ('34.005/clone-components-portrait-smoke', '34.005/canvas-overlay-portrait-smoke',
       '34.005/transformed-drag-portrait-smoke')
TASKS = set(DIRECT + GUI)
PENDING = ('Current-source ordinary-Meson sanitizer acceptance is pending under the 2026-10-17 '
           'workflow correction; the sealed per-object focused run remains historical scoped execution '
           'evidence, not compliance with the Oct11 prescribed sanitizer workflow.')
PARTIAL = {'14.007', '14.009', '14.018', '14.019', '14.020'}


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(CHUNK), b''):
            h.update(block)
    return h.hexdigest()


def hash_pin(value):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{64}', value) is not None


def unique_object(pairs):
    result = {}
    for k, v in pairs:
        require(k not in result, 'Duplicate JSON key: ' + k)
        result[k] = v
    return result


def read(path):
    return json.loads(Path(path).read_text(), object_pairs_hook=unique_object)


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.PIPE)


def canonical_name(name):
    return (isinstance(name, str) and bool(name) and '\\' not in name and '\x00' not in name
            and not PurePosixPath(name).is_absolute()
            and all(p not in ('', '.', '..') for p in name.split('/')))


def plain_path(root, name):
    require(canonical_name(name), 'Noncanonical path: ' + str(name))
    path = Path(root) / name
    require(path.resolve().is_relative_to(Path(root).resolve()), 'Path escapes root: ' + name)
    require(not path.is_symlink() and path.is_file(), 'Not a regular file: ' + name)
    for parent in path.parents:
        if parent == Path(root):
            break
        require(not parent.is_symlink(), 'Linked parent: ' + name)
    return path


def decompress(archive, expected_sha, output, origin=None):
    require(hash_pin(expected_sha), 'Invalid archive SHA-256')
    require(origin in (None, 'run-002', 'run-003', 'run-004'), 'Unknown fixed archive origin')
    compressed_limit, expanded_limit = MAX_COMPRESSED, MAX_EXPANDED
    if origin == 'run-002':
        require(expected_sha == ORIGIN_ARCHIVE_SHA256, 'Only the exact failed origin is admissible')
        compressed_limit, expanded_limit = ORIGIN_MAX_COMPRESSED, ORIGIN_MAX_EXPANDED
    elif origin == 'run-003':
        require(expected_sha == RESOURCE_ORIGIN_ARCHIVE_SHA256, 'Only the exact failed resource origin is admissible')
        compressed_limit, expanded_limit = RESOURCE_ORIGIN_MAX_COMPRESSED, RESOURCE_ORIGIN_MAX_EXPANDED
    elif origin == 'run-004':
        require(expected_sha == EXECUTION_ARCHIVE_SHA256, 'Only the exact blocked run-004 archive is admissible')
        compressed_limit, expanded_limit = EXECUTION_MAX_COMPRESSED, EXECUTION_MAX_EXPANDED
    descriptor = os.open(archive, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW)
    with os.fdopen(descriptor, 'rb') as stream:
        info = os.fstat(stream.fileno())
        require(stat.S_ISREG(info.st_mode) and info.st_size <= compressed_limit,
                'Archive is not a bounded regular file')
        decoder = lzma.LZMADecompressor(format=lzma.FORMAT_XZ, memlimit=XZ_MEMORY)
        h = hashlib.sha256()
        compressed = expanded = 0
        while True:
            data = stream.read(min(CHUNK, compressed_limit - compressed + 1))
            if not data:
                break
            compressed += len(data)
            require(compressed <= compressed_limit, 'Compressed archive limit exceeded')
            h.update(data)
            require(not decoder.eof, 'Trailing data or concatenated XZ streams')
            while True:
                part = decoder.decompress(data, max_length=min(CHUNK, expanded_limit - expanded + 1))
                data = b''
                expanded += len(part)
                require(expanded <= expanded_limit, 'Expanded archive limit exceeded')
                output.write(part)
                if decoder.eof:
                    require(not decoder.unused_data, 'Trailing data or concatenated XZ streams')
                    break
                if decoder.needs_input:
                    break
        require(decoder.eof, 'Truncated XZ stream')
        require(h.hexdigest() == expected_sha, 'Archive SHA-256 mismatch')
    output.seek(0)


def extract_tar(stream, directory, origin=None):
    """USTAR regular members only; never tarfile.extract or extractall."""
    require(origin in (None, 'run-002', 'run-003', 'run-004'), 'Unknown fixed archive origin')
    members, order, member_limit = MEMBERS, sorted(MEMBERS), MAX_MEMBER
    if origin == 'run-002':
        members, order, member_limit = ORIGIN_MEMBERS, list(ORIGIN_ORDER), ORIGIN_MAX_MEMBER
    elif origin == 'run-003':
        members, order, member_limit = RESOURCE_ORIGIN_MEMBERS, list(RESOURCE_ORIGIN_ORDER), RESOURCE_ORIGIN_MAX_MEMBER
    elif origin == 'run-004':
        members, order, member_limit = EXECUTION_MEMBERS, list(EXECUTION_ORDER), EXECUTION_MAX_MEMBER
    seen = []
    while True:
        header = stream.read(512)
        require(len(header) == 512, 'Truncated tar header')
        if header == bytes(512):
            require(stream.read(512) == bytes(512), 'Invalid tar end marker')
            while True:
                padding = stream.read(CHUNK)
                if not padding:
                    break
                require(len(padding) % 512 == 0 and not any(padding), 'Extra tar data')
            break
        entry = tarfile.TarInfo.frombuf(header, 'utf-8', 'strict')
        name = entry.name
        require(canonical_name(name) and name in members, 'Unknown or unsafe archive member: ' + name)
        require(name not in seen, 'Duplicate archive member: ' + name)
        require(entry.type in (tarfile.REGTYPE, tarfile.AREGTYPE) and not entry.linkname,
                'Archive member is not a regular file: ' + name)
        require(entry.uid == entry.gid == entry.mtime == 0 and entry.mode == 0o644
                and not entry.uname and not entry.gname, 'Noncanonical archive metadata: ' + name)
        require(0 <= entry.size <= member_limit, 'Archive member exceeds limit: ' + name)
        seen.append(name)
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open('xb') as target:
            remaining = entry.size
            while remaining:
                data = stream.read(min(CHUNK, remaining))
                require(data, 'Truncated archive member: ' + name)
                target.write(data)
                remaining -= len(data)
        padding = stream.read(-entry.size % 512)
        require(len(padding) == -entry.size % 512 and not any(padding), 'Invalid member padding')
    require(seen == order, 'Missing members or noncanonical archive ordering')


def route_gate(matrix):
    route = matrix.get('ordinary_meson_supplement')
    keys = {'archive', 'archive_sha256', 'receipt_sha256', 'source_commit', 'source_tree',
            'runner_sha256', 'terminal_exit_code', 'terminal_validation', 'normal_counts',
            'sanitizer_counts', 'integration_files_sha256', 'policy_disclosure'}
    require(isinstance(route, dict) and set(route) == keys, 'Missing/incomplete ordinary supplement route')
    require(route['archive'] == ARCHIVE and route['source_commit'] == COMMIT
            and route['source_tree'] == TREE and route['runner_sha256'] == RUNNER_SHA256,
            'Ordinary route source/runner/archive pin mismatch')
    require(all(hash_pin(route[k]) for k in ('archive_sha256', 'receipt_sha256')), 'Invalid ordinary hash pin')
    require(route['receipt_sha256'] == EXECUTION_RECEIPT_SHA256, 'Wrong immutable run-004 receipt')
    require(type(route['terminal_exit_code']) is int and route['terminal_exit_code'] == 1,
            'Original runner exit 1 must remain explicit')
    require(isinstance(route['terminal_validation'], dict) and route['terminal_validation'].get('passed') is False
            and route['terminal_validation'] == {'passed': False, 'errors': POLICY_FAILURE,
                                                'receipt_sha256': EXECUTION_RECEIPT_SHA256},
            'Original policy-redaction validation failure must remain explicit')
    recovery = route['policy_disclosure']
    require(isinstance(recovery, dict) and set(recovery) == {'schema', 'validator_sha256', 'attestation_sha256',
            'validation_sha256', 'terminal_exit_code', 'terminal_validation'}, 'Missing/incomplete policy-disclosure route')
    require(recovery['schema'] == POLICY_SCHEMA and recovery['validator_sha256'] == POLICY_HELPER_SHA256
            and recovery['attestation_sha256'] == POLICY_ATTESTATION_SHA256
            and recovery['validation_sha256'] == POLICY_VALIDATION_SHA256, 'Policy-disclosure pins differ')
    require(type(recovery['terminal_exit_code']) is int and recovery['terminal_exit_code'] == 0,
            'Evidence-only recovery did not terminate with integer exit zero')
    report = recovery['terminal_validation']
    require(isinstance(report, dict) and report.get('passed') is True
            and hashlib.sha256((json.dumps(report, indent=2, ensure_ascii=False) + '\n').encode()).hexdigest() == POLICY_VALIDATION_SHA256,
            'Recovery validation is not the exact observed successful disclosure report')
    require(route['normal_counts'] == NORMAL and route['sanitizer_counts'] == SANITIZER,
            'Ordinary target counts are not exact 267/103 scope')
    pins = route['integration_files_sha256']
    require(isinstance(pins, dict) and set(pins) == NEW_FILES and all(hash_pin(h) for h in pins.values()),
            'Missing/extra integration-file pins')
    require(pins[ARCHIVE] == route['archive_sha256'], 'Archive integration pin differs')
    return route


def tree_entries(root, commit):
    records = {}
    for line in git(root, 'ls-tree', '-r', '-z', commit).split(b'\0'):
        if line:
            metadata, raw_name = line.split(b'\t', 1)
            mode, kind, oid = metadata.decode().split()
            name = raw_name.decode()
            require(canonical_name(name), 'Noncanonical Git path')
            records[name] = (mode, kind, oid)
    return records


def file_seal(path, mode, oid):
    """Bind working bytes to Git blobs, bypassing index stat/assume-unchanged."""
    if mode == '120000':
        require(path.is_symlink(), 'Expected tracked symlink: ' + str(path))
        data = os.fsencode(os.readlink(path))
        require(hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == oid,
                'Changed tracked symlink: ' + str(path))
        # This exact pinned source has no tracked links. Do not silently widen it.
        raise ValueError('Unexpected symlink in fixed execution source')
    require(mode in ('100644', '100755') and path.is_file() and not path.is_symlink(),
            'Missing/wrong tracked file type: ' + str(path))
    info = path.stat()
    require(bool(info.st_mode & 0o111) == (mode == '100755'), 'Changed tracked executable mode: ' + str(path))
    blob = hashlib.sha1(b'blob ' + str(info.st_size).encode() + b'\0')
    content = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(CHUNK), b''):
            blob.update(data)
            content.update(data)
    require(blob.hexdigest() == oid, 'Working file differs from frozen Git object: ' + str(path))
    return {'sha256': content.hexdigest(), 'bytes': info.st_size, 'mode': oct(info.st_mode & 0o777)}


def source_gate(root, commit, tree, state=None, current=False, pins=None):
    root = Path(root)
    require(git(root, 'rev-parse', commit + '^{tree}').decode().strip() == tree, 'Unavailable/mismatched frozen Git tree')
    entries = tree_entries(root, commit)
    indexed = {}
    for row in git(root, 'ls-files', '--stage', '-z').split(b'\0'):
        if row:
            metadata, name = row.split(b'\t', 1)
            mode, oid, stage = metadata.decode().split()
            require(stage == '0', 'Unmerged source index')
            indexed[name.decode()] = (mode, oid)
    allowed = NEW_FILES if current else set()
    require(set(indexed) == set(entries) | allowed, 'Missing/extra tracked current or frozen file')
    require(not git(root, 'ls-files', '--others', '--exclude-standard', '-z'), 'Untracked publication/source file')
    flags = git(root, 'ls-files', '-v', '-z').decode().split('\0')
    require(not any(f and (f[0].islower() or f[0] == 'S') for f in flags), 'Hidden index entry')
    if not current:
        require(git(root, 'rev-parse', 'HEAD').decode().strip() == commit
                and not git(root, 'status', '--porcelain', '--untracked-files=all'), 'Frozen view is not exact and clean')
    if state is not None:
        require(state['commit'] == commit and state['tree'] == tree and state['clean'] is True
                and not state['status'] and not state['hidden_index_entries']
                and not state['committed_content_mismatches'], 'Unclean/incorrect source snapshot')
        require(set(state['tracked_files']) == {n for n, (_, k, _) in entries.items() if k != 'commit'}
                and set(state['submodules']) == {n for n, (_, k, _) in entries.items() if k == 'commit'},
                'Incomplete source snapshot membership')
    for name, (mode, kind, oid) in entries.items():
        require(indexed[name][0] == mode, 'Index mode differs: ' + name)
        path = root / name
        if kind == 'commit':
            require(indexed[name] == (mode, oid), 'Changed pinned gitlink: ' + name)
            child = state['submodules'][name] if state is not None else None
            if child is not None:
                require(child['pinned_commit'] == oid, 'Unpinned receipt submodule')
            child_tree = git(path, 'rev-parse', oid + '^{tree}').decode().strip()
            source_gate(path, oid, child_tree, child)
        elif current and name in DOCUMENTS:
            plain_path(root, name)
            require(bool(path.stat().st_mode & 0o111) == (mode == '100755'), 'Document mode changed')
        else:
            require(indexed[name] == (mode, oid), 'Index differs from frozen source: ' + name)
            plain_path(root, name)
            fingerprint = file_seal(path, mode, oid)
            if state is not None:
                require(fingerprint == state['tracked_files'][name], 'Source manifest differs from Git/working bytes: ' + name)
    if current:
        for name in NEW_FILES:
            require(indexed[name][0] == '100644', 'New integration mode must be 100644')
            fingerprint = file_seal(plain_path(root, name), indexed[name][0], indexed[name][1])
            require(fingerprint['sha256'] == pins[name], 'Changed integration file: ' + name)
        require((root / CHECKER).read_bytes() == Path(__file__).read_bytes(), 'Invoked checker differs from publication checker')



def covered_tasks_gate(root, frozen):
    """Only covered task facts constrain future documentary edits."""
    def task_rows(path):
        rows = {}
        for line in path.read_text().splitlines():
            columns = [part.strip() for part in line.split('|')]
            if len(columns) == 7 and re.fullmatch(r'\d{2}\.\d{3}(?:/[\w.-]+)?', columns[1]):
                require(columns[1] not in rows, 'Duplicated task identifier')
                rows[columns[1]] = columns
        return rows
    before, after = task_rows(frozen / 'tasks.md'), task_rows(root / 'tasks.md')
    for name in TASKS:
        require(name in before and name in after and after[name][2] == '[x]'
                and after[name][4:6] == before[name][4:6], 'Covered task/dependency/criterion changed: ' + name)
        for dependency in after[name][4].split(','):
            dependency = dependency.strip()
            if dependency and dependency != '-':
                require(dependency in after and after[dependency][2] == '[x]', 'Unmet covered dependency: ' + dependency)
    progress = {}
    for line in (root / 'migration/progress.tsv').read_text().splitlines():
        row = line.split('\t')
        if row[0] in TASKS:
            require(row[0] not in progress and len(row) == 8, 'Duplicate/malformed covered progress row')
            progress[row[0]] = row
    require(set(progress) == TASKS and all(row[1] == 'DONE' and ARCHIVE in row[3] for row in progress.values()),
            'Covered progress status/evidence differs from accepted claims')


def covered_facts_gate(root, frozen, matrix, route):
    """Keep accepted claims truthful without freezing unrelated future prose."""
    covered_tasks_gate(root, frozen)
    baseline = read(frozen / 'migration/acceptance/clone.json')
    for key in ('reports', 'current_core_sources_sha256', 'historical_snapshots', 'current_core_receipt'):
        require(matrix[key] == baseline[key], 'Historical matrix evidence changed: ' + key)
    old_rows = {r['id']: r for r in baseline['rows']}
    new_rows = {r['id']: r for r in matrix['rows']}
    require(len(matrix['rows']) == len(new_rows) == 28 and set(new_rows) == set(old_rows), 'Changed Clone row coverage')
    evidence_fields = ('dependencies', 'acceptance_condition', 'source', 'named_tests',
                       'additional_roundtrip_tests', 'additional_dialog_tests', 'reports')
    for name, row in new_rows.items():
        original = old_rows[name]
        require(all(row.get(k) == original.get(k) for k in evidence_fields), 'Covered Clone evidence changed: ' + name)
        expected_gaps = [gap for gap in original['gaps'] if gap != PENDING]
        require(row['gaps'] == expected_gaps and row['state'] == ('PARTIAL' if name in PARTIAL else 'COMPONENT_VERIFIED'),
                'Covered Clone state/gaps conflict with acceptance: ' + name)
    expected = copy.deepcopy(baseline['ordinary_meson_sanitizer_acceptance'])
    expected['status'] = 'PASSED'
    expected['observed_ordinary_results'] = {'source_commit': COMMIT, 'source_tree': TREE,
        'normal_counts': NORMAL, 'sanitizer_counts': SANITIZER, 'normal_total': 267,
        'sanitizer_total': 103, 'receipt_sha256': route['receipt_sha256']}
    require(matrix['ordinary_meson_sanitizer_acceptance'] == expected and PENDING not in matrix['global_gates'],
            'Ordinary acceptance facts or workflow status differ')


def closure_audit(root, frozen, matrix, route):
    old = (frozen / 'tasks.md').read_text()
    new = (root / 'tasks.md').read_text()
    expected = old
    rows = {}
    for line in old.splitlines(keepends=True):
        columns = [s.strip() for s in line.split('|')]
        if len(columns) == 7:
            rows[columns[1]] = columns
            if columns[1] in TASKS:
                require(columns[2] == '[ ]', 'Frozen task was not reopened')
                expected = expected.replace(line, line.replace('| [ ] |', '| [x] |', 1), 1)
    require(TASKS <= set(rows) and new == expected, 'Changes exceed exact 17 task checkboxes')
    final_rows = {p[1]: p for line in new.splitlines() if len(p := [x.strip() for x in line.split('|')]) == 7}
    for name in TASKS:
        for dependency in final_rows[name][4].split(','):
            dependency = dependency.strip()
            if dependency and dependency != '-':
                require(dependency in final_rows and final_rows[dependency][2] == '[x]', 'Unmet dependency: ' + name + ': ' + dependency)
    before = (frozen / 'migration/progress.tsv').read_text().splitlines(keepends=True)
    after = (root / 'migration/progress.tsv').read_text().splitlines(keepends=True)
    require(len(before) == len(after), 'Added/deleted progress row')
    seen = set()
    for a, b in zip(before, after):
        old_columns, columns = a.rstrip('\n').split('\t'), b.rstrip('\n').split('\t')
        name = old_columns[0]
        if name not in TASKS:
            require(a == b, 'Unrelated progress row changed: ' + name)
            continue
        seen.add(name)
        require(len(columns) == len(old_columns) == 8 and columns[0] == name and columns[1] == 'DONE'
                and old_columns[1] == ('BLOCKED' if name in GUI else 'DOING'), 'Wrong progress restoration: ' + name)
        require(columns[2] == old_columns[2] and columns[6] == old_columns[6], 'Progress ownership/children changed')
        for i in (3, 4, 5, 7):
            require(columns[i].startswith(old_columns[i]), 'Historical progress text replaced: ' + name)
        require(ARCHIVE in columns[3] and columns[3] != old_columns[3], 'Missing ordinary progress evidence: ' + name)
    require(seen == TASKS, 'Missing/duplicated closure progress scope')
    contract = (frozen / 'migration/contracts/clone-layer-port.md').read_bytes()
    updated = (root / 'migration/contracts/clone-layer-port.md').read_bytes()
    require(updated.startswith(contract) and len(updated) > len(contract), 'Historical contract must be append-only')
    baseline = read(frozen / 'migration/acceptance/clone.json')
    expected_matrix = copy.deepcopy(baseline)
    require(len(expected_matrix['rows']) == 28, 'Unexpected frozen Clone row count')
    for row in expected_matrix['rows']:
        require(row['gaps'].count(PENDING) == 1, 'Unexpected frozen workflow gap')
        row['gaps'].remove(PENDING)
        row['state'] = 'PARTIAL' if row['id'] in PARTIAL else 'COMPONENT_VERIFIED'
    expected_matrix['global_gates'].remove(PENDING)
    acceptance = expected_matrix['ordinary_meson_sanitizer_acceptance']
    acceptance['status'] = 'PASSED'
    acceptance['observed_ordinary_results'] = {'source_commit': COMMIT, 'source_tree': TREE,
        'normal_counts': NORMAL, 'sanitizer_counts': SANITIZER, 'normal_total': 267,
        'sanitizer_total': 103, 'receipt_sha256': route['receipt_sha256']}
    expected_matrix['ordinary_meson_supplement'] = route
    require(matrix == expected_matrix, 'Matrix exceeds exact closure fields or changes historical evidence')
    for name in DOCUMENTS:
        metadata, indexed_name = git(root, 'ls-files', '--stage', '-z', '--', name).rstrip(b'\0').split(b'\t', 1)
        mode, oid, stage = metadata.decode().split()
        require(stage == '0' and indexed_name.decode() == name, 'Unmerged/missing closure document')
        file_seal(root / name, mode, oid)


def load_verified(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    exec(compile(path.read_bytes(), str(path), 'exec'), module.__dict__)
    return module



def source_phases_gate(source, observations):
    require(set(source) == {'normal', 'sanitizer'}, 'Wrong source phase families')
    first = source['normal']['before_build']
    expected = {'observed_phases': sorted(PHASES), 'missing_phases': [], 'observed_sources_unchanged': True}
    require(isinstance(observations, dict) and set(observations) == {'normal', 'sanitizer'},
            'Missing source phase observations')
    for label in ('normal', 'sanitizer'):
        require(set(source[label]) == PHASES and all(state == first for state in source[label].values()),
                'Missing/changed execution source phase seal')
        require(observations[label] == expected and observations[label]['observed_sources_unchanged'] is True,
                'Incomplete/changed source phase observations')
    return first


def execution_package_gate(directory, frozen, route):
    require(sha(directory / 'receipt.json') == route['receipt_sha256'], 'Terminal receipt pin mismatch')
    receipt = read(directory / 'receipt.json')
    require(receipt.get('schema') == SCHEMA and receipt.get('status') == 'passed'
            and receipt.get('errors') == [] and receipt.get('finished_utc'),
            'Missing/failed/pending terminal receipt')
    require(set(receipt['files']) == EXECUTION_PUBLIC_MEMBERS - {'receipt.json'}, 'Incomplete/extra public receipt membership')
    for name, pin in receipt['files'].items():
        require(hash_pin(pin) and sha(plain_path(directory, name)) == pin, 'Changed public member: ' + name)
    require(sha(directory / 'ordinary_meson_validation.py') == RUNNER_SHA256 == receipt['runner_sha256'],
            'Runner source pin mismatch before import')
    require(set(receipt['helpers_sha256']) == HELPERS, 'Missing/extra helper source pins')
    # All imported helper bytes are bound directly to immutable Git objects
    # before runner.validate() can execute any helper module.
    for name in HELPERS:
        pinned = git(frozen, 'show', COMMIT + ':' + name)
        require((directory / 'helpers' / name).read_bytes() == pinned
                and hashlib.sha256(pinned).hexdigest() == receipt['helpers_sha256'][name],
                'Helper differs from frozen Git bytes before import: ' + name)
    for name in MENU_SOURCES:
        require((directory / 'menu-source-inputs' / name).read_bytes() == git(frozen, 'show', COMMIT + ':' + name),
                'Menu resource input differs from frozen Git bytes')
    for name in REGISTRATIONS:
        require((directory / 'registration-sources' / name).read_bytes() == git(frozen, 'show', COMMIT + ':' + name),
                'Registration source differs from frozen Git bytes')
    source = read(directory / 'source-integrity.json')
    first = source_phases_gate(source, receipt.get('source_phase_observations'))
    source_gate(frozen, COMMIT, TREE, first)
    package = load_verified(directory / 'helpers/tools/package-linux-runtime.py', 'pinned_ordinary_package')
    supplemental_facts(directory, receipt, package.parse_ldd_line)
    runner = load_verified(directory / 'ordinary_meson_validation.py', 'pinned_ordinary_runner')
    origin_package_gate(directory)
    errors = runner.validate(directory, COMMIT, TREE)
    require(errors == POLICY_FAILURE, 'Frozen validator must reproduce only the exact policy-redaction error: ' + str(errors))
    return first




def package_gate(directory, frozen, route):
    """Validate immutable execution and reviewed evidence-only policy disclosure."""
    recovery = directory / POLICY_PREFIX
    for name, pin in POLICY_FILES.items():
        require(sha(plain_path(recovery, name)) == pin, 'Changed policy-disclosure member: ' + name)
    with tempfile.TemporaryDirectory(prefix='ordinary-immutable-run004-') as temporary:
        root = Path(temporary)
        with tempfile.TemporaryFile() as stream:
            decompress(recovery / EXECUTION_ARCHIVE, EXECUTION_ARCHIVE_SHA256, stream, origin='run-004')
            extract_tar(stream, root, origin='run-004')
        public = root / 'run-004/public'
        require(sha(public.parent / 'validation.json') == EXECUTION_VALIDATION_SHA256
                and read(public.parent / 'validation.json') == route['terminal_validation'],
                'Original blocked validation bytes/claim changed')
        source = execution_package_gate(public, frozen, route)
        # All original runner/helper bytes and every recovery file were pinned
        # before this code import. The reviewed helper reproduces the sole old
        # failure, then changes only the first read of two attested fixed values.
        helper = load_verified(recovery / POLICY_HELPER, 'pinned_policy_disclosure')
        report = helper.validate(public, read(recovery / 'policy-disclosure.json'))
        require(report == read(recovery / 'policy-disclosure-validation.json')
                == route['policy_disclosure']['terminal_validation'], 'Recomputed policy-disclosure validation differs')
        return source


def build_origin_claim_gate(receipt, origin_receipt):
    claim = receipt.get('origin')
    require(isinstance(claim, dict) and claim.get('provenance_only') is True
            and claim.get('inherited_test_pass_credit') is False
            and claim.get('fresh_configure_or_compile_in_this_attempt') is False,
            'Origin must not receive inherited pass credit or a fresh-build claim')
    require(claim == {'archive_file': ORIGIN_FILE, 'archive_sha256': ORIGIN_ARCHIVE_SHA256,
        'receipt_sha256': ORIGIN_RECEIPT_SHA256, 'runner_sha256': ORIGIN_RUNNER_SHA256, 'status': 'failed',
        'historical_commands': origin_receipt['commands'], 'provenance_only': True,
        'inherited_test_pass_credit': False, 'fresh_configure_or_compile_in_this_attempt': False},
        'Origin must remain failed provenance without inherited pass credit or fresh-build claim')


def build_origin_package_gate(directory):
    """Bound the immutable nested failed archive before the runner reads it."""
    archive = plain_path(directory, ORIGIN_FILE)
    with tempfile.TemporaryDirectory(prefix='ordinary-failed-origin-') as temporary:
        root = Path(temporary)
        with tempfile.TemporaryFile() as stream:
            decompress(archive, ORIGIN_ARCHIVE_SHA256, stream, origin='run-002')
            extract_tar(stream, root, origin='run-002')
        public = root / 'run-002/public'
        require(sha(public / 'receipt.json') == ORIGIN_RECEIPT_SHA256, 'Wrong preserved failed-origin receipt')
        receipt = read(public / 'receipt.json')
        require(receipt['schema'] == 'ordinary-meson-clone-navigation-supplement-v2'
                and receipt['status'] == 'failed' and receipt['source_commit'] == COMMIT
                and receipt['source_tree'] == TREE and receipt['runner_sha256'] == ORIGIN_RUNNER_SHA256
                and receipt['errors'] == ['RuntimeError: Command failed: sanitizer-meson-console.log'],
                'Failed origin must remain exact failed provenance')
        require(set(receipt['files']) == ORIGIN_PUBLIC_MEMBERS - {'receipt.json'}, 'Origin public membership differs')
        for name, pin in receipt['files'].items():
            require(hash_pin(pin) and sha(plain_path(public, name)) == pin, 'Changed preserved origin member: ' + name)
        require(sha(public / 'ordinary_meson_validation.py') == ORIGIN_RUNNER_SHA256, 'Wrong original runner bytes')
        for record in receipt['commands']:
            require(type(record['exit_code']) is int, 'Origin command status is not an integer')
        build_origin_claim_gate(read(directory / 'receipt.json'), receipt)


def origin_claim_gate(receipt, origin_receipt):
    claim = receipt.get('origin')
    require(isinstance(claim, dict) and claim.get('provenance_only') is True
            and claim.get('inherited_test_pass_credit') is False
            and claim.get('build_commands_in_this_attempt') is False,
            'Origin must not receive inherited pass credit or a current build claim')
    require(claim == {'archive_file': RESOURCE_ORIGIN_FILE, 'archive_sha256': RESOURCE_ORIGIN_ARCHIVE_SHA256,
        'receipt_sha256': RESOURCE_ORIGIN_RECEIPT_SHA256, 'runner_sha256': RESOURCE_ORIGIN_RUNNER_SHA256,
        'status': 'failed', 'historical_commands': origin_receipt['commands'], 'provenance_only': True,
        'inherited_test_pass_credit': False, 'build_commands_in_this_attempt': False,
        'nested_build_archive_sha256': ORIGIN_ARCHIVE_SHA256, 'nested_build_receipt_sha256': ORIGIN_RECEIPT_SHA256},
        'Origin must preserve exact failed resource/build provenance and zero current build commands')


def origin_package_gate(directory):
    """Strictly parse both fixed failed origins before the reviewed runner reads them."""
    archive = plain_path(directory, RESOURCE_ORIGIN_FILE)
    with tempfile.TemporaryDirectory(prefix='ordinary-failed-resource-origin-') as temporary:
        root = Path(temporary)
        with tempfile.TemporaryFile() as stream:
            decompress(archive, RESOURCE_ORIGIN_ARCHIVE_SHA256, stream, origin='run-003')
            extract_tar(stream, root, origin='run-003')
        public = root / 'run-003/public'
        require(sha(public / 'receipt.json') == RESOURCE_ORIGIN_RECEIPT_SHA256, 'Wrong preserved failed-resource receipt')
        receipt = read(public / 'receipt.json')
        require(receipt['schema'] == 'ordinary-meson-clone-navigation-resource-continuation-v1'
                and receipt['status'] == 'failed' and receipt['source_commit'] == COMMIT
                and receipt['source_tree'] == TREE and receipt['runner_sha256'] == RESOURCE_ORIGIN_RUNNER_SHA256
                and receipt['errors'] == ['RuntimeError: Disk reserve reached: normal-meson-console.log',
                'Final sanitizer seal: Missing source evidence phases for sanitizer: after_tests, before_tests; observed snapshots unchanged'],
                'Resource origin must remain exact failed provenance with absent sanitizer runtime phases')
        require(set(receipt['files']) == RESOURCE_ORIGIN_PUBLIC_MEMBERS - {'receipt.json'}, 'Resource origin public membership differs')
        for name, pin in receipt['files'].items():
            require(hash_pin(pin) and sha(plain_path(public, name)) == pin, 'Changed preserved resource member: ' + name)
        require(sha(public / 'ordinary_meson_validation.py') == RESOURCE_ORIGIN_RUNNER_SHA256, 'Wrong resource origin runner bytes')
        for record in receipt['commands']:
            require(type(record['exit_code']) is int, 'Resource origin command status is not an integer')
        origin_claim_gate(read(directory / 'receipt.json'), receipt)
        build_origin_package_gate(public)


def role_inventory_gate(inst, database):
    objects, build_only = inst['objects'], inst['build_only_objects']
    roles = inst['role_evidence']['roles']
    database_by_output = {row['output']: row for row in database}
    require(len(objects) == 1373 and len(build_only) == 3 and len(database) == 1376,
            'Wrong fixed 1373 runtime + 3 build-only = 1376 ordinary object scope')
    require(not set(objects) & set(build_only) and set(objects) | set(build_only) == set(database_by_output)
            and len(database_by_output) == len(database), 'Incomplete/duplicate runtime and build-only closure')
    require(roles['full_build_compile_objects'] == len(database)
            and roles['runtime_compile_objects'] == len(objects)
            and roles['build_only_compile_objects'] == len(build_only)
            and set(roles['runtime_objects']) == set(objects)
            and set(roles['build_only_objects']) == set(build_only), 'Structural role counts/membership differ')
    return database_by_output



def transformations_gate(directory, receipt, transforms):
    require(set(transforms) == TRANSFORM_MEMBERS, 'Missing/extra raw/public transformation provenance')
    for name, record in transforms.items():
        require(name in receipt['files'] and record['public_sha256'] == sha(directory / name)
                and hash_pin(record['raw_sha256']), 'Invalid public transformation seal')
        inner = record.get('environment_removal') or record.get('prefix_transform')
        if inner:
            require(inner['raw_sha256'] == record['raw_sha256'], 'Transformation raw source mismatch')



def toolchain_gate(toolchain):
    require(set(toolchain) == {'cc', 'c++', 'meson', 'ninja', 'ar', 'nm', 'ldd', 'git'},
            'Missing/extra recorded toolchain entry')
    for name, record in toolchain.items():
        require(set(record) == {'path', 'resolved_path', 'fingerprint', 'version'}
                and all(isinstance(record[k], str) and Path(record[k]).is_absolute()
                        for k in ('path', 'resolved_path')), 'Malformed recorded tool identity: ' + name)
        version = record['version']
        require(isinstance(version, list) and 1 <= len(version) <= 3
                and all(isinstance(line, str) and line.strip() for line in version),
                'Missing recorded tool version: ' + name)
        fingerprint = record['fingerprint']
        if 'symlink' in fingerprint:
            require(set(fingerprint) == {'symlink', 'content'} and isinstance(fingerprint['symlink'], str)
                    and bool(fingerprint['symlink']), 'Malformed recorded tool link: ' + name)
            fingerprint = fingerprint['content']
        require(set(fingerprint) == {'sha256', 'bytes', 'mode'} and hash_pin(fingerprint['sha256'])
                and type(fingerprint['bytes']) is int and fingerprint['bytes'] > 0
                and isinstance(fingerprint['mode'], str) and re.fullmatch(r'0o[0-7]{3}', fingerprint['mode'])
                and int(fingerprint['mode'], 8) & 0o111, 'Missing executable tool fingerprint: ' + name)



def loader_text_gate(loader, parse_ldd_line):
    for name, record in loader.items():
        require(record['method'] == 'ldd with exact private Meson-result environment'
                and isinstance(record['ldd_output'], str) and 'not found' not in record['ldd_output'],
                'Missing/unresolved actual loader observation: ' + name)
        pairs = [pair for line in record['ldd_output'].splitlines() if (pair := parse_ldd_line(line))]
        require(len(pairs) == len(dict(pairs)) and dict(pairs) == {key: value['path'] for key, value in record['libraries'].items()},
                'Loader mapping differs from recorded ldd output: ' + name)


def metadata_seals_gate(directory, label, metadata, transforms, baseline):
    for name, raw_pin in metadata.items():
        public = label + '-' + name.replace('/', '-')
        require(transforms[public]['raw_sha256'] == raw_pin, 'Metadata raw/public seal mismatch')
        if name == 'compile_commands.json':
            # The frozen freshness helper does not inventory compile databases.
            # The native runner independently seals/rechecks their raw metadata;
            # these two public databases preserve those exact original bytes.
            require(sha(directory / public) == raw_pin, 'Compile database differs from exact raw metadata seal')
        else:
            require(baseline['build_metadata'][name]['sha256'] == raw_pin,
                    'Metadata raw/public/freshness seal mismatch')


def supplemental_facts(directory, receipt, parse_ldd_line=None):
    """Require facts the execution runner checked but its verifier summarizes."""
    commands = receipt['commands']
    require(len(commands) == 2 and Counter(c['log'] for c in commands) == Counter(COMMANDS),
            'Each of the two fresh continuation phases must occur exactly once; no build command is permitted')
    transforms = read(directory / 'transforms.json')
    transformations_gate(directory, receipt, transforms)
    toolchain_gate(read(directory / 'toolchain.json'))
    metadata = read(directory / 'metadata.json')
    fresh = read(directory / 'freshness.json')
    elf = read(directory / 'elf-integrity.json')
    source = read(directory / 'source-integrity.json')['normal']['before_build']
    database = read(directory / 'sanitizer-closure-compile-database.json')
    for command in commands:
        name = command['log']
        require(type(command['exit_code']) is int and command['exit_code'] == 0
                and command.get('owned_processes_quiescent') is True and command.get('finished_utc'),
                'Nonterminal/failed command: ' + name)
        require(command['public_log_sha256'] == sha(directory / name)
                and command['raw_log_sha256'] == transforms[name]['raw_sha256'], 'Command log seal differs')
        argv = command['argv']
        label = 'normal' if name.startswith('normal-') else 'sanitizer'
        names = NORMAL if label == 'normal' else SANITIZER
        require(len(argv) == 13 + len(names) and Path(argv[0]).is_absolute()
                and re.fullmatch(r'python3(?:\.\d+)?', Path(argv[0]).name)
                and argv[1:12] == ['-B', receipt[label + '_source'] + '/tools/run-painter-xvfb.py',
                'meson', 'test', '-C', receipt[label + '_build'], '--no-rebuild', '--num-processes',
                '1', '--print-errorlogs', '--logbase'] and Path(argv[12]).is_absolute()
                and argv[13:] == list(names), 'Changed selected ordinary test command')
        expected_timeout = 3550
        require(type(command['timeout_seconds']) is int and command['timeout_seconds'] == expected_timeout,
                'Changed command timeout policy')
    for label, counts in (('normal', NORMAL), ('sanitizer', SANITIZER)):
        required_phases = PHASES
        require(set(elf[label]) == required_phases and set(fresh[label]) == required_phases,
                'Missing/extra ELF/freshness phase seals')
        require(elf[label]['after_tests'] == elf[label]['after'], 'ELF changed after test phase')
        registry = read(directory / (label + '-meson-info-intro-tests.json'))
        require(all(sum(row['name'] == name for row in registry) == 1 for name in counts),
                'Missing/duplicate selected Meson registration')
        expected_metadata = METADATA | ({'build.ninja'} if label == 'sanitizer' else set())
        require(set(metadata[label]) == expected_metadata, 'Missing configuration/compiler/backend metadata')
        baseline = fresh[label]['before_tests']
        require(baseline['source_commit'] == COMMIT and baseline['tracked_source_files'] == source['tracked_files']
                and baseline['build_directory'] == receipt[label + '_build'], 'Freshness source/build binding differs')
        metadata_seals_gate(directory, label, metadata[label], transforms, baseline)
        info = read(directory / (label + '-meson-info-meson-info.json'))
        require(info['directories']['source'] == receipt[label + '_source']
                and info['directories']['build'] == receipt[label + '_build'], 'Wrong recorded Meson directories')
        machines = read(directory / (label + '-meson-info-intro-machines.json'))
        require(machines['host'] == machines['build'] and machines['host']['system'] == 'linux', 'Not native Linux')
        options = {r['name']: r['value'] for r in read(directory / (label + '-meson-info-intro-buildoptions.json'))}
        expected_options = receipt[label + '_configuration']
        require(all(options[k] == v for k, v in expected_options.items()) and options['backend'] == 'ninja'
                and options['b_coverage'] is False, 'Wrong configured backend/options')
        if label == 'sanitizer':
            require(options['buildtype'] == 'debugoptimized' and str(options['optimization']) == '1'
                    and options['auto_features'] == 'disabled' and options['libunwind'] is False
                    and options['headless-tests'] == 'disabled' and options['painter-http'] == 'disabled',
                    'Weakened bounded sanitizer configuration')
        console = (directory / (label + '-meson-console.log')).read_text()
        require(re.search(r'^Xvfb verified: 127\.0\.0\.1:\d+, 1080x1920x24, authenticated accepted, empty authority rejected$',
                          console, re.M), 'Missing authenticated loopback portrait Xvfb proof')
        loader = read(directory / (label + '-loader.json'))
        require(parse_ldd_line is not None, 'Pinned loader parser is required')
        loader_text_gate(loader, parse_ldd_line)
        policy = read(directory / (label + '-runtime-policy.json'))
        require(policy['private_directory_variables'] == ['GIMP3_CACHEDIR', 'GIMP3_DIRECTORY', 'GIMP3_TEMPDIR',
                'HOME', 'TMPDIR', 'XDG_CACHE_HOME', 'XDG_CONFIG_HOME', 'XDG_DATA_HOME', 'XDG_RUNTIME_DIR', 'XDG_STATE_HOME'],
                'Private profile isolation policy differs')
        if label == 'normal':
            require(policy['asan_options'] is None and policy['ubsan_options'] is None, 'Unexpected normal sanitizer options')
            loader = read(directory / 'normal-loader.json')
            require(all(not any(n.startswith(('libasan.', 'libubsan.')) for n in row['libraries'])
                        for row in loader.values()), 'Normal loader contains sanitizer runtime')
        require(policy['inherited_environment_published'] is False and policy['suppression_files'] == [],
                'Unfiltered environment or suppression policy')
    inst = read(directory / 'instrumentation.json')
    require(inst['instrumented_object_count'] == len(inst['objects']) and inst['vptr_enabled'] is True
            and inst['third_party_dependencies_instrumented'] is False and inst['leak_detection'] is False,
            'Instrumentation scope/limitations changed')
    objects, build_only = inst['objects'], inst['build_only_objects']
    database_by_output = role_inventory_gate(inst, database)
    for name, obj in (objects | build_only).items():
        require('/GR-' not in obj['argv'], 'C++ RTTI disabled')
        if obj['source'] is not None:
            parts = PurePosixPath(obj['source']).parts
            state = source
            while parts and parts[0] in state['submodules']:
                state = state['submodules'][parts[0]]
                parts = parts[1:]
            require(parts and '/'.join(parts) in state['tracked_files']
                    and obj['source_sha256'] == state['tracked_files']['/'.join(parts)]['sha256'],
                    'Runtime/build-only source not bound to pinned execution source')
        else:
            row = database_by_output[name]
            generated = Path(os.path.normpath(str(Path(row['directory']) / row['file'])))
            build = Path(receipt['sanitizer_build'])
            require(generated.is_relative_to(build), 'Generated runtime source escapes sealed build')
            relative = str(generated.relative_to(build))
            outputs = fresh['sanitizer']['before_tests']['build_outputs']
            require(relative in outputs and outputs[relative].get('sha256') == obj['source_sha256'],
                    'Generated runtime source lacks sealed build-output identity')
    registry = read(directory / 'sanitizer-meson-info-intro-tests.json')
    build = Path(receipt['sanitizer_build'])
    targets = {str(Path(r['cmd'][0]).relative_to(build)) for r in registry if r['name'] in SANITIZER}
    require(len(inst['links']) == 4 and {a[a.index('-o') + 1] for a in inst['links']} == targets,
            'Ordinary links do not exactly cover four selected sanitizer targets')
    linked = {a for line in inst['links'] for a in line if a.endswith('.a') and not Path(a).is_absolute()}
    require(linked == set(inst['linked_archives']), 'Missing/extra linked archive seal')
    for name in ('app/core/gimpclonelayer.cpp', 'app/display/gimpdisplayshell-tool-events.c',
                 'app/display/gimppainternavigation.c', 'app/painter/binding-store.cpp',
                 'app/tests/test-gimp-clone-layer.c', 'app/tests/test-gimp-clone-layout.cpp',
                 'app/tests/test-painter-navigation-events.c'):
        require(any(o['source'] == name and o['asan_symbol'] is True for o in inst['objects'].values()),
                'Required source lacks actual instrumented object: ' + name)


def historical_gate(frozen, matrix):
    names = ('clone_evidence', 'clone_archive')
    saved = {n: sys.modules.get(n) for n in names}
    try:
        for name in names:
            path = frozen / 'tools' / (name + '.py')
            require(path.read_bytes() == git(frozen, 'show', COMMIT + ':tools/' + name + '.py'),
                    'Historical helper pin mismatch')
            sys.modules[name] = load_verified(path, name)
        path = frozen / 'tools/check_clone_acceptance.py'
        require(path.read_bytes() == git(frozen, 'show', COMMIT + ':tools/check_clone_acceptance.py'),
                'Historical checker pin mismatch')
        original = load_verified(path, 'pinned_historical_acceptance')
        errors = original.validate(root=frozen, matrix=matrix)
        require(not errors, 'Historical frozen gate rejected evidence: ' + '; '.join(errors))
    finally:
        for name, previous in saved.items():
            if previous is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = previous


def validate(root=ROOT, matrix=None, current_evidence=None, frozen_source=None, audit_closure=False):
    """Return errors; focused evidence can never supply the ordinary requirement."""
    try:
        root = Path(root).resolve()
        matrix = read(root / 'migration/acceptance/clone.json') if matrix is None else matrix
        route = route_gate(matrix)
        require(matrix == read(root / 'migration/acceptance/clone.json'), 'Supplied matrix differs from current publication file')
        require(current_evidence is None, 'Focused current_evidence cannot replace the ordinary archive')
        require(frozen_source is not None, 'Provide --frozen-source at the exact pinned commit with gimp-data materialized')
        frozen = Path(frozen_source).resolve()
        require(root != frozen, 'Current and historical frozen views must be distinct')
        archive = plain_path(root, ARCHIVE)
        with tempfile.TemporaryDirectory(prefix='ordinary-meson-acceptance-') as temporary:
            directory = Path(temporary)
            with tempfile.TemporaryFile() as stream:
                decompress(archive, route['archive_sha256'], stream)
                extract_tar(stream, directory)
            source = package_gate(directory, frozen, route)
            source_gate(root, COMMIT, TREE, source,
                        current=True, pins=route['integration_files_sha256'])
            covered_facts_gate(root, frozen, matrix, route)
            if audit_closure:
                closure_audit(root, frozen, matrix, route)
            historical_gate(frozen, matrix)
        return []
    except (OSError, ValueError, TypeError, KeyError, IndexError, RuntimeError, EOFError,
            lzma.LZMAError, tarfile.TarError, UnicodeError, subprocess.SubprocessError) as error:
        return ['Ordinary-Meson acceptance rejected: ' + type(error).__name__ + ': ' + str(error)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--frozen-source', type=Path, required=True)
    parser.add_argument('--audit-closure', action='store_true', help='One-time exact seven-file/17-task/28-row publication audit')
    args = parser.parse_args()
    errors = validate(args.root, frozen_source=args.frozen_source, audit_closure=args.audit_closure)
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print('Ordinary-Meson policy-disclosure acceptance verified: frozen 267 normal / 103 ASan+UBSan cases; original offline failure retained; exact bounded publication and immutable historical evidence. Broader gates remain open.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
