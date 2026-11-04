#!/usr/bin/env python3
"""Failure-only bounded APT diagnostics; never changes acquisition or CI gates.

Read only status.txt and known update/download logs in one owned driver directory.
Do not read profiles, environment dumps, APT config/auth files or Xauthority.
Captured text is sanitized, bounded and diagnostic-only; it is not a retry.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import PurePath
import re
import stat
from urllib.parse import urlsplit, urlunsplit

RUN_NAME = re.compile(r"painter-(?:native-ui|brush-ci)\.[A-Za-z0-9]{8}\Z")
LOG_NAME = re.compile(r"(?:update|download-(?:00[0-9]|01[0-9]|02[0-7]))\.log\Z")
MAX_DIRECTORY_ENTRIES = 512
MAX_LOG_FILES = 29  # Current immutable 560-package lock: 28 batches + update.
MAX_FILE_BYTES = 128 * 1024
MAX_TOTAL_BYTES = 4 * 1024 * 1024
MAX_CAPTURE_BYTES = 128 * 1024
MAX_TOTAL_CAPTURE_BYTES = 4 * 1024 * 1024
STATUS_BYTES = 4096
URL = re.compile(r"(?i)\b(?:https?|ftp)://[^\s<>\"']+")
ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
SECRET = re.compile(r"(?i)\b[\w-]*(?:authorization|cookie|password|passwd|secret|token|api[_-]?key)[\w-]*\b|\b(?:bearer|basic)\s+\S+")
PRIVATE_PATH = re.compile(r"/(?:__w|workspace|home|root|tmp)(?:/[^\s<>\"']*)?")


class Refused(ValueError):
    """Fixed public reason only; never interpolate a private path or content."""


def directory(path):
    """Open every absolute component without following a symlink."""
    value = os.fspath(path)
    parts = PurePath(value).parts
    if not value.startswith('/') or '..' in parts or '.' in value.split('/'):
        raise Refused('unsafe-absolute-directory')
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in parts[1:]:
            new = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW,
                          dir_fd=fd)
            os.close(fd)
            fd = new
        return fd
    except BaseException:
        os.close(fd)
        raise Refused('unsafe-directory-component') from None


def child_directory(fd, name):
    new = os.open(name, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
    info = os.fstat(new)
    if info.st_uid != os.geteuid() or info.st_mode & 0o022:
        os.close(new)
        raise Refused('unowned-or-writable-directory')
    return new


def entries(fd):
    result = []
    with os.scandir(fd) as iterator:
        for item in iterator:
            if len(result) >= MAX_DIRECTORY_ENTRIES:
                raise Refused('directory-entry-limit')
            result.append(item.name)
    return result


def read_file(fd, name, limit):
    f = os.open(name, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=fd)
    try:
        info = os.fstat(f)
        if (not stat.S_ISREG(info.st_mode) or info.st_uid != os.geteuid()
                or info.st_nlink != 1 or info.st_mode & 0o022):
            raise Refused('unsafe-log-file')
        if info.st_size <= limit:
            data = os.read(f, limit)
        else:
            half = limit // 2
            prefix = os.read(f, half)
            head = prefix.rsplit(b'\n', 1)[0] if b'\n' in prefix else b''
            os.lseek(f, -half, os.SEEK_END)
            tail = os.read(f, half).split(b'\n', 1)
            # Do not export fragments cut through credential/URL lines.
            data = head + b'\n[diagnostic middle omitted by byte limit]\n'
            data += tail[1] if len(tail) == 2 else b''
        return data, info.st_size
    finally:
        os.close(f)


def sanitize(data):
    text = ANSI.sub('', data.decode('utf-8', errors='replace'))
    # Remove controls/formatting rather than inserting separators into a
    # credential label (for example pass<NUL>word). Output is ASCII only.
    text = ''.join(c for c in text if c in '\n\t' or 32 <= ord(c) <= 126)

    def url(match):
        try:
            parts = urlsplit(match.group())
            # Preserve public archive host/path and HTTP diagnostics, never
            # URL credentials, query/fragment, or non-public/proxy endpoints.
            if parts.hostname not in ('snapshot.debian.org', 'deb.debian.org',
                                      'security.debian.org'):
                return '[non-archive URL redacted]'
            return urlunsplit((parts.scheme, parts.hostname, parts.path, '', ''))
        except ValueError:
            return '[invalid URL redacted]'
    text = URL.sub(url, text)
    text = '\n'.join('[credential-bearing line redacted]' if SECRET.search(line) else line
                     for line in text.split('\n'))
    text = PRIVATE_PATH.sub('[private path redacted]', text)
    return text.encode('ascii')



def bound_capture(clean):
    if len(clean) <= MAX_CAPTURE_BYTES:
        return clean, False
    marker = b'\n[diagnostic sanitized text omitted by byte limit]\n'
    half = (MAX_CAPTURE_BYTES - len(marker)) // 2
    prefix, suffix = clean[:half], clean[-half:]
    head = prefix.rsplit(b'\n', 1)[0] if b'\n' in prefix else b''
    tail = suffix.split(b'\n', 1)[1] if b'\n' in suffix else b''
    return head + marker + tail, True


def failed_dependencies(outfd):
    try:
        raw, size = read_file(outfd, 'status.txt', STATUS_BYTES)
    except FileNotFoundError:
        return False
    if size > STATUS_BYTES:
        raise Refused('oversized-status')
    try:
        pairs = [line.split('=', 1) for line in raw.decode('ascii').splitlines()]
        if any(len(p) != 2 for p in pairs):
            return False
        status = dict(pairs)
        if len(status) != len(pairs):
            raise Refused('duplicate-status-key')
        return status.get('stage') == 'dependencies' and re.fullmatch(
            r'[1-9][0-9]{0,2}', status.get('exit_code', '')) is not None
    except UnicodeError:
        raise Refused('invalid-status') from None


def collect(temp, artifact):
    """Return sanitized records, or skip. Caller controls only validated paths."""
    outfd = directory(artifact)
    try:
        if not failed_dependencies(outfd):
            return None
    finally:
        os.close(outfd)
    rootfd = directory(temp)
    descriptors = [rootfd]
    try:
        names = [n for n in entries(rootfd) if RUN_NAME.fullmatch(n)]
        if len(names) != 1:
            raise Refused('expected-one-owned-driver-directory')
        fd = rootfd
        for name in (names[0], 'deps', 'apt', 'logs'):
            fd = child_directory(fd, name)
            descriptors.append(fd)
        names = sorted(n for n in entries(fd) if LOG_NAME.fullmatch(n))
        if not names or len(names) > MAX_LOG_FILES:
            raise Refused('log-file-count-limit')
        records = []
        total = 0
        capture_total = 0
        for name in names:
            raw, original_size = read_file(fd, name, MAX_FILE_BYTES)
            total += min(original_size, MAX_FILE_BYTES)
            if total > MAX_TOTAL_BYTES:
                raise Refused('total-byte-limit')
            clean, output_truncated = bound_capture(sanitize(raw))
            capture_total += len(clean)
            if capture_total > MAX_TOTAL_CAPTURE_BYTES:
                raise Refused('total-sanitized-byte-limit')
            records.append((name, clean, original_size, output_truncated))
        return records
    finally:
        for fd in reversed(descriptors):
            os.close(fd)


def preserve(artifact, records, commit):
    fd = directory(artifact)
    try:
        os.mkdir('apt-acquisition-diagnostics', mode=0o700, dir_fd=fd)
        dest = child_directory(fd, 'apt-acquisition-diagnostics')
        try:
            files = []
            for name, content, original_size, output_truncated in records:
                if not LOG_NAME.fullmatch(name):
                    raise Refused('unsafe-output-name')
                f = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                            0o600, dir_fd=dest)
                with os.fdopen(f, 'wb') as stream:
                    stream.write(content)
                files.append({'name': name, 'source_bytes': original_size,
                              'captured_bytes': len(content),
                              'source_truncated': original_size > MAX_FILE_BYTES,
                              'sanitized_truncated': output_truncated,
                              'sanitized_sha256': hashlib.sha256(content).hexdigest()})
            receipt = {'format': 1, 'diagnostic_only': True,
                       'acceptance_eligible': False, 'candidate_commit': commit,
                       'trigger': 'dependencies stage with nonzero driver exit',
                       'max_log_files': MAX_LOG_FILES, 'max_source_bytes_per_file': MAX_FILE_BYTES,
                       'max_total_source_bytes': MAX_TOTAL_BYTES,
                       'max_sanitized_bytes_per_file': MAX_CAPTURE_BYTES,
                       'max_total_sanitized_bytes': MAX_TOTAL_CAPTURE_BYTES, 'files': files,
                       'raw_logs_exported': False, 'configuration_or_environment_exported': False,
                       'retry_or_package_policy_change': False}
            f = os.open('receipt.json', os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                        0o600, dir_fd=dest)
            with os.fdopen(f, 'w') as stream:
                json.dump(receipt, stream, indent=2)
                stream.write('\n')
        finally:
            os.close(dest)
    finally:
        os.close(fd)


def main():
    if os.environ.get('GITHUB_ACTIONS') != 'true':
        raise Refused('ordinary-actions-only')
    commit = os.environ.get('GITHUB_SHA', '')
    if not re.fullmatch('[0-9a-f]{40}', commit):
        raise Refused('invalid-requested-commit')
    temp = os.environ.get('RUNNER_TEMP', '')
    workspace = os.environ.get('GITHUB_WORKSPACE', '')
    if not temp or not workspace:
        raise Refused('missing-owned-root')
    artifact = workspace + '/ci-results'
    records = collect(temp, artifact)
    if records is None:
        print('APT diagnostics skipped: no failed dependency stage.')
        return
    preserve(artifact, records, commit)
    print('Preserved bounded sanitized APT acquisition diagnostics; no retry or acceptance.')


if __name__ == '__main__':
    try:
        main()
    except (Refused, OSError) as error:
        # Do not echo OSError paths or arbitrary file contents into job logs.
        reason = str(error) if isinstance(error, Refused) else 'unsafe-or-unavailable-input'
        print('APT diagnostics refused: ' + reason)
        raise SystemExit(2)
