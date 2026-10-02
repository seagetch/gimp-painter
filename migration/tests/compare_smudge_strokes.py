#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip
from pathlib import Path
import subprocess
import sys
expected = gzip.decompress(Path(sys.argv[1]).read_bytes())
run = subprocess.run(sys.argv[2:], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
actual = b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'SMUDGE ') or x == b'SMUDGE_CAPTURE_COMPLETE') + b'\n'
if run.returncode:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit(run.returncode)
if actual != expected:
    summary = []
    for a, b in zip(actual.splitlines(), expected.splitlines()):
        if a != b and b.split()[2] == b'finish':
            a_data, b_data = bytes.fromhex(a.split()[-1].decode()), bytes.fromhex(b.split()[-1].decode())
            summary.append((int(b.split()[1]), sum(x != y for x, y in zip(a_data, b_data))))
    print('Mismatched finish cases/count:', summary)
    for a, b in zip(actual.splitlines(), expected.splitlines()):
        if a != b:
            print('First mismatched record:', b' '.join(b.split()[:4]).decode())
            a_data, b_data = bytes.fromhex(a.split()[-1].decode()), bytes.fromhex(b.split()[-1].decode())
            print('Different bytes:', sum(x != y for x, y in zip(a_data, b_data)))
            print('First byte differences:', [(i, x, y) for i, (x, y) in enumerate(zip(a_data, b_data)) if x != y][:20])
            break
    raise SystemExit('Legacy Smudge full stroke mismatch')
print(f'Exact legacy Smudge stroke/Undo: {len(expected)} bytes, 193 records')
