#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact comparison of complete actual-old selection-mask Surface records."""
import gzip
import hashlib
import pathlib
import subprocess
import sys
import tempfile


def cases_from(data):
    cases = {}
    current = None
    for line in data.splitlines(keepends=True):
        if not line.startswith(b'SEL_'):
            raise ValueError('Expected only sealed SEL_ capture records')
        if line.startswith(b'SEL_CASE '):
            current = int(line.split()[1])
            if current in cases:
                raise ValueError(f'Duplicate case {current}')
            cases[current] = []
        if current is None:
            raise ValueError('Record precedes case header')
        cases[current].append(line)
        if line == b'SEL_COMPLETE\n':
            current = None
    if current is not None or set(cases) != set(range(10)):
        raise ValueError('Expected exactly ten complete old-runtime cases')
    return {case: b''.join(lines) for case, lines in cases.items()}


def main():
    if len(sys.argv) != 3:
        raise SystemExit('Usage: compare-selection.py OLD_VALUES.tsv.gz TRACE_EXECUTABLE')
    expected = gzip.decompress(pathlib.Path(sys.argv[1]).read_bytes())
    executable = str(pathlib.Path(sys.argv[2]).resolve())
    cases = cases_from(expected)
    records = 0
    with tempfile.TemporaryDirectory(prefix='painter-selection-') as temporary:
        for case, captured in cases.items():
            # The trace reader consumes only SEL_MASK from this actual-old
            # record. Initial pixels and selection bytes are independently
            # reconstructed, and all output records are compared below.
            source = pathlib.Path(temporary) / f'{case}.tsv'
            source.write_bytes(captured)
            run = subprocess.run([executable, str(case), str(source)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
            if run.returncode:
                sys.stderr.buffer.write(run.stderr)
                raise SystemExit(f'Current selection case {case} exited {run.returncode}')
            if run.stderr:
                sys.stderr.buffer.write(run.stderr)
            if run.stdout != captured:
                old, new = captured.splitlines(), run.stdout.splitlines()
                mismatch = next((i for i, (a, b) in enumerate(zip(old, new)) if a != b), min(len(old), len(new)))
                def brief(lines):
                    if mismatch >= len(lines):
                        return '<missing>'
                    value = lines[mismatch]
                    if len(value) > 240:
                        return f'{value.split()[0].decode()} {len(value)} bytes, SHA256 {hashlib.sha256(value).hexdigest()}'
                    return value.decode()
                raise SystemExit(f'Case {case}, record {mismatch+1} differs: old={brief(old)}; current={brief(new)}')
            records += len(captured.splitlines())
    print(f'Exact actual-old selection Surface match: {len(cases)} cases, {records} records, {len(expected)} bytes')


if __name__ == '__main__':
    main()
