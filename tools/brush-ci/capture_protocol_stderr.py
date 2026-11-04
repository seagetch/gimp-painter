#!/usr/bin/env python3
"""Export bounded sanitized protocol-child stderr only; never relax the oracle.

The native host writes per-scenario stderr before checking child diagnostics, so
completed files remain useful after a parent assertion/abort. Missing later
cases are not passes. No stdout, wire, events, observations, profiles or env read.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import PurePath
import re
import stat

from capture_apt_failure import (Refused, directory, child_directory, read_file,
                                 sanitize, bound_capture)

CASES = ('query', 'init', 'host-quit', 'explicit-quit', 'success-open',
         'success-close', 'error-open', 'error-close', 'failed-main-write',
         'failed-main-write-recoverable', 'temporary-quit', 'temporary-main')
FILES = tuple(case + '.stderr' for case in CASES)
RUN = re.compile(r'painter-brush-ci\.[A-Za-z0-9]{8}\Z')
MAX_FILES = 12
MAX_INPUT_BYTES = 128 * 1024
MAX_OUTPUT_BYTES = 128 * 1024
MAX_TOTAL_INPUT_BYTES = 2 * 1024 * 1024
MAX_TOTAL_OUTPUT_BYTES = 2 * 1024 * 1024


def absolute(value):
    raw = os.fspath(value)
    path = PurePath(raw)
    if (not path.is_absolute() or raw != str(path) or '..' in path.parts
            or '.' in raw.split('/') or raw.startswith('//')):
        raise Refused('unsafe-absolute-directory')
    return path


def owned(fd):
    info = os.fstat(fd)
    if info.st_uid != os.geteuid() or info.st_mode & 0o022:
        raise Refused('unowned-or-writable-directory')


def paths(runner_temp, runtime_dir, artifact_dir, mode):
    if mode not in ('positive', 'sanitizer'):
        raise Refused('unsupported-protocol-mode')
    root, runtime, artifact = map(absolute, (runner_temp, runtime_dir, artifact_dir))
    try:
        relative = runtime.relative_to(root).parts
    except ValueError:
        raise Refused('runtime-outside-runner-root') from None
    if len(relative) != 2 or not RUN.fullmatch(relative[0]) or relative[1] != 'runtime':
        raise Refused('unexpected-runtime-layout')
    if runtime.is_relative_to(artifact) or artifact.is_relative_to(runtime):
        raise Refused('artifact-runtime-overlap')
    return root, relative[0], artifact



def normalize_driver_prefixes(raw, driver):
    # Only the validated owned driver root is eligible; no caller prefix input.
    # Require a path-token boundary, not a suffix inside another path or URL.
    for child, marker in (('source', b'<source>/'), ('build', b'<build>/')):
        prefix = os.fsencode(str(driver / child) + '/')
        raw = re.sub(rb'(?<![A-Za-z0-9_./-])' + re.escape(prefix), marker, raw)
    return raw


def collect(runner_temp, runtime_dir, artifact_dir, mode):
    root, run, artifact = paths(runner_temp, runtime_dir, artifact_dir, mode)
    # Validate output before reading private source logs, without creating it.
    outfd = directory(artifact)
    try:
        owned(outfd)
    finally:
        os.close(outfd)
    rootfd = directory(root)
    descriptors = [rootfd]
    try:
        fd = rootfd
        for name in (run, 'runtime', 'protocol-' + mode + '-child-logs'):
            fd = child_directory(fd, name)
            descriptors.append(fd)
        if stat.S_IMODE(os.fstat(fd).st_mode) != 0o700:
            raise Refused('child-log-directory-must-be-private')
        records = []
        total_input = total_output = 0
        for name in FILES:
            try:
                raw, source_bytes = read_file(fd, name, MAX_INPUT_BYTES)
            except FileNotFoundError:
                continue  # Later scenarios may not run after the parent abort.
            total_input += min(source_bytes, MAX_INPUT_BYTES)
            if total_input > MAX_TOTAL_INPUT_BYTES:
                raise Refused('total-child-input-byte-limit')
            raw = normalize_driver_prefixes(raw, root / run)
            clean, output_truncated = bound_capture(sanitize(raw))
            if len(clean) > MAX_OUTPUT_BYTES:
                raise Refused('child-output-byte-limit')
            total_output += len(clean)
            if total_output > MAX_TOTAL_OUTPUT_BYTES:
                raise Refused('total-child-output-byte-limit')
            records.append((name, clean, source_bytes, output_truncated))
        if not records or len(records) > MAX_FILES:
            raise Refused('no-or-excess-allowlisted-child-stderr')
        return records
    finally:
        for fd in reversed(descriptors):
            os.close(fd)


def preserve(artifact_dir, mode, records):
    if mode not in ('positive', 'sanitizer'):
        raise Refused('unsupported-protocol-mode')
    # Revalidate records before any destination creation. No caller filenames.
    names = [record[0] for record in records]
    if (not names or len(names) > MAX_FILES or len(set(names)) != len(names)
            or any(name not in FILES for name in names)):
        raise Refused('unsafe-child-output-list')
    if (any(len(record[1]) > MAX_OUTPUT_BYTES for record in records)
            or sum(len(record[1]) for record in records) > MAX_TOTAL_OUTPUT_BYTES):
        raise Refused('child-output-byte-limit')
    outfd = directory(absolute(artifact_dir))
    try:
        owned(outfd)
        destination = 'protocol-' + mode + '-child-stderr'
        os.mkdir(destination, mode=0o700, dir_fd=outfd)
        destfd = child_directory(outfd, destination)
        try:
            files = []
            for name, clean, source_bytes, output_truncated in records:
                f = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                            0o600, dir_fd=destfd)
                with os.fdopen(f, 'wb') as stream:
                    stream.write(clean)
                files.append({'name': name, 'source_bytes': source_bytes,
                              'captured_bytes': len(clean),
                              'source_truncated': source_bytes > MAX_INPUT_BYTES,
                              'sanitized_truncated': output_truncated,
                              'sanitized_sha256': hashlib.sha256(clean).hexdigest()})
            receipt = {'format': 1, 'diagnostic_only': True, 'acceptance_eligible': False,
                       'mode': mode, 'files': files,
                       'missing_allowlisted_files': [name for name in FILES if name not in names],
                       'max_files': MAX_FILES, 'max_input_bytes_per_file': MAX_INPUT_BYTES,
                       'max_output_bytes_per_file': MAX_OUTPUT_BYTES,
                       'max_total_input_bytes': MAX_TOTAL_INPUT_BYTES,
                       'max_total_output_bytes': MAX_TOTAL_OUTPUT_BYTES}
            f = os.open('receipt.json', os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                        0o600, dir_fd=destfd)
            with os.fdopen(f, 'w') as stream:
                json.dump(receipt, stream, indent=2)
                stream.write('\n')
        finally:
            os.close(destfd)
    finally:
        os.close(outfd)


class FixedArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        raise Refused('invalid-arguments')


def main():
    parser = FixedArgumentParser(description=__doc__)
    parser.add_argument('--runner-temp', required=True)
    parser.add_argument('--runtime-dir', required=True)
    parser.add_argument('--artifact-dir', required=True)
    parser.add_argument('--mode', required=True, choices=('positive', 'sanitizer'))
    args = parser.parse_args()
    records = collect(args.runner_temp, args.runtime_dir, args.artifact_dir, args.mode)
    preserve(args.artifact_dir, args.mode, records)
    print('Preserved bounded protocol-child stderr; native/checker results remain authoritative.')


if __name__ == '__main__':
    try:
        main()
    except (Refused, OSError) as error:
        reason = str(error) if isinstance(error, Refused) else 'unsafe-or-unavailable-input'
        print('Protocol stderr capture refused: ' + reason)
        raise SystemExit(2)
