#!/usr/bin/env python3
"""Private-log diagnostics: export fixed labels and hashes, never log prose.

Tail hashes describe only the bounded bytes read, not the whole log. Labels are
observations, not attribution of the build's cause. No log is opened for running
checkpoints. All path components are opened with O_NOFOLLOW.
"""
import argparse
import hashlib
import json
import os
import re
import stat
from pathlib import Path

CELLS = ('baseline-normal', 'current-normal', 'baseline-sanitizer', 'current-sanitizer')
STAGES = ('environment', 'configure', 'target-selection', 'compile', 'catalog-stage', 'build-identity', 'overlay-check', 'complete')
TAIL_BYTES = 65536
TAIL_LINES = 32
RULES = (
    ('sanitizer-runtime-not-found', rb'(?:libasan|libubsan|liblsan).{0,256}(?:not found|cannot open)'),
    ('missing-header', rb'fatal error:.{0,256}(?:\.h|\.hpp).{0,128}(?:No such file|not found)'),
    ('undefined-reference', rb'undefined reference|undefined symbol'),
    ('missing-file', rb'no such file or directory|file not found'),
    ('dependency-not-found', rb'[Dd]ependency .{0,256}not found'),
    ('ninja-stopped', rb'ninja: build stopped|FAILED:'),
    ('out-of-memory', rb'out of memory|cannot allocate memory'),
    ('compiler-killed', rb'killed signal|killed process'),
    ('no-space', rb'[Nn]o space left on device'),
    ('permission-denied', rb'permission denied'),
    ('timeout', rb'timed out|timeout expired'),
    ('python-exception', rb'Traceback \(most recent call last\)'),
)


# Headers are anchored and tool/source-specific. Never classify an arbitrary
# substring "ld:" inside meson.build or Meson's uppercase ERROR as GCC output.
MESON_HEADER = rb'^(?:[^\x00-\x20:]{1,2048}/)?meson\.build:[0-9]{1,7}:[0-9]{1,7}: ERROR: (.*)$'
COMPILER_HEADER = rb'^(?:(?:[^\x00-\x20:]{1,2048}\.(?:c|cc|cpp|cxx|h|hpp)):[0-9]{1,7}(?::[0-9]{1,7})?|(?:[^\x00-\x20:]{1,2048}/)?(?:cc|gcc|g\+\+|cc1|cc1plus|clang|clang\+\+)): (fatal error|error|warning): '
LINKER_HEADER = rb'^(?:[^\x00-\x20:]{1,2048}/)?(?:(?:x86_64-linux-gnu-)?ld(?:\.bfd|\.gold|\.lld)?|collect2): '
NATIVE_GIMP_MESSAGE = (b'Problem encountered: When cross-compiling, the build requires either an exe_wrapper '
                       b'or a native GIMP installed (gimp-console-3.0 or gimp-3.0).')
PROGRAM_MISSING = b'Program gimp-console-3.0 gimp-3.0 found: NO'
EXTRA_CATEGORIES = ('configure-error', 'linker-error', 'compiler-error', 'compiler-warning',
                    'meson-error', 'native-gimp-required', 'native-gimp-lookup-missing')


def classify(line):
    labels = [name for name, pattern in RULES if re.search(pattern, line, re.IGNORECASE)]
    meson = re.fullmatch(MESON_HEADER, line)
    if meson or line.startswith(b'ERROR: '):
        labels.extend(('configure-error', 'meson-error'))
        message = meson.group(1) if meson else line[len(b'ERROR: '):]
        if message == NATIVE_GIMP_MESSAGE:
            labels.append('native-gimp-required')
    compiler = re.match(COMPILER_HEADER, line)
    if compiler:
        labels.append('compiler-warning' if compiler.group(1) == b'warning' else 'compiler-error')
    if re.match(LINKER_HEADER, line):
        labels.append('linker-error')
    if line == PROGRAM_MISSING:
        labels.append('native-gimp-lookup-missing')
    return labels or ['uncategorized']


def directory(path):
    """Open an absolute directory without following any symlink component."""
    path = os.path.abspath(os.fspath(path))
    fd = os.open('/', os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in path.split('/')[1:]:
            if not part:
                continue
            nxt = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=fd)
            os.close(fd)
            fd = nxt
        return fd
    except BaseException:
        os.close(fd)
        raise


def read_tail(run, cell, stage):
    root = logs = fd = None
    try:
        root = directory(run)
        logs = os.open('logs', os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=root)
        fd = os.open(cell + '-' + stage + '.log', os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=logs)
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode):
            return {'status': 'unsafe-log'}
        start = max(0, before.st_size - TAIL_BYTES)
        os.lseek(fd, start, os.SEEK_SET)
        chunks = []
        remaining = TAIL_BYTES
        while remaining:
            chunk = os.read(fd, remaining)
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        data = b''.join(chunks)
        after = os.fstat(fd)
        if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (after.st_size, after.st_mtime_ns, after.st_ctime_ns):
            return {'status': 'changed-during-read'}
        # Drop a possibly partial first line; hashes still bind all bytes read.
        lines = (data.split(b'\n')[1:] if start else data.split(b'\n'))
        if lines and not lines[-1]:
            lines.pop()
        counts = {name: 0 for name, _ in RULES}
        counts.update({name: 0 for name in EXTRA_CATEGORIES})
        counts['uncategorized'] = 0
        records = []
        for line in lines:
            labels = classify(line)
            for name in labels:
                counts[name] += 1
            records.append({'line_sha256': hashlib.sha256(line).hexdigest(),
                            'byte_length': len(line), 'categories': labels})
        return {'status': 'read', 'file_size': before.st_size,
                'bytes_read': len(data), 'tail_truncated': bool(start),
                'tail_sha256': hashlib.sha256(data).hexdigest(),
                'category_counts': counts, 'tail': records[-TAIL_LINES:],
                'tail_lines_omitted': max(0, len(records) - TAIL_LINES)}
    except FileNotFoundError:
        return {'status': 'missing-log'}
    except OSError:
        return {'status': 'unsafe-or-unreadable-log'}
    finally:
        for item in (fd, logs, root):
            if item is not None:
                os.close(item)


def collect(run, cell, stage, state, exit_code=None):
    if cell not in CELLS or stage not in STAGES or state not in ('running', 'finished'):
        raise ValueError('invalid-selector')
    if state == 'running' and exit_code is not None:
        raise ValueError('running-exit-code')
    if state == 'finished' and (type(exit_code) is not int or not 0 <= exit_code <= 255):
        raise ValueError('invalid-exit-code')
    with open(__file__, 'rb') as source:
        producer = hashlib.sha256(source.read()).hexdigest()
    result = {'schema': 'matched-build-diagnostic-v2', 'cell': cell, 'stage': stage,
              'state': state, 'exit_code': exit_code,
              'exit_scope': 'build-cell-shell-status-at-checkpointed-stage-not-necessarily-tool-exit',
              'acceptance_eligible': False, 'native_failures_not_waived': True,
              'diagnostic_scope': 'bounded-log-labels-not-cause-attribution',
              'producer_sha256': producer}
    result['log'] = ({'status': 'not-read-running'} if state == 'running' else
                     {'status': 'not-applicable'} if stage == 'complete' else
                     read_tail(run, cell, stage))
    return result


def persist(out, cell, result):
    root = dest = None
    temp = '.' + cell + '.' + str(os.getpid()) + '.tmp'
    try:
        root = directory(out)
        try:
            os.mkdir('build-diagnostics', 0o700, dir_fd=root)
        except FileExistsError:
            pass
        dest = os.open('build-diagnostics', os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=root)
        fd = os.open(temp, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600, dir_fd=dest)
        with os.fdopen(fd, 'w') as stream:
            json.dump(result, stream, sort_keys=True, indent=2)
            stream.write('\n')
        os.rename(temp, cell + '.json', src_dir_fd=dest, dst_dir_fd=dest)
    finally:
        if dest is not None:
            try:
                os.unlink(temp, dir_fd=dest)
            except FileNotFoundError:
                pass
            os.close(dest)
        if root is not None:
            os.close(root)


def validate_roots(run, out):
    run = Path(os.path.abspath(run))
    out = Path(os.path.abspath(out))
    temp = Path(os.environ['RUNNER_TEMP']).resolve()
    workspace = Path(os.environ['GITHUB_WORKSPACE']).resolve()
    if run.parent != temp or not re.fullmatch(r'painter-matched-preclick\.[A-Za-z0-9]{8}', run.name):
        raise ValueError('invalid-run-root')
    if out != workspace / 'matched-results':
        raise ValueError('invalid-output-root')
    fd = directory(run)
    try:
        info = os.fstat(fd)
        if info.st_uid != os.getuid() or info.st_mode & 0o077:
            raise ValueError('invalid-run-ownership')
    finally:
        os.close(fd)
    fd = directory(out)
    os.close(fd)


class SafeParser(argparse.ArgumentParser):
    def error(self, message):
        self.exit(2, 'build-diagnostic: invalid-cli\n')


def main():
    parser = SafeParser(description=__doc__)
    parser.add_argument('--run', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--cell', required=True, choices=CELLS)
    parser.add_argument('--stage', required=True, choices=STAGES)
    parser.add_argument('--state', required=True, choices=('running', 'finished'))
    parser.add_argument('--exit-code', type=int)
    args = parser.parse_args()
    try:
        validate_roots(args.run, args.out)
        persist(args.out, args.cell, collect(args.run, args.cell, args.stage, args.state, args.exit_code))
    except (ValueError, OSError, KeyError):
        print('build-diagnostic: invalid-input-or-output')
        return 2
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
