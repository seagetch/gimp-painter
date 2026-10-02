#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip, pathlib, subprocess, sys
expected = gzip.decompress(pathlib.Path(sys.argv[1]).read_bytes())
run = subprocess.run(sys.argv[2:], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
if run.stdout != expected:
    a, b = expected.splitlines(), run.stdout.splitlines()
    mismatch = next((i for i,(x,y) in enumerate(zip(a,b)) if x != y), min(len(a),len(b)))
    print(f'Oracle differs at line {mismatch+1}; expected {a[mismatch:mismatch+1]!r}, actual {b[mismatch:mismatch+1]!r}')
    raise SystemExit(1)
print(f'Exact pinned-runtime trace match: {len(expected)} bytes, {len(expected.splitlines())} records')
if run.stderr: print(run.stderr.decode())
