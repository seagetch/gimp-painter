#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare the independent old Gray-u8 sessions, retaining runtime diagnostics."""
import gzip
import pathlib
import subprocess
import sys

expected = gzip.decompress(pathlib.Path(sys.argv[1]).read_bytes())
run = subprocess.run(sys.argv[2:], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
actual = b"\n".join(x for x in run.stdout.splitlines() if x.startswith(b"GRAY_SESSION_")) + b"\n"
if run.returncode:
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit(run.returncode)
if actual != expected:
    left, right = expected.splitlines(), actual.splitlines()
    mismatch = next((i for i, pair in enumerate(zip(left, right)) if pair[0] != pair[1]), min(len(left), len(right)))
    print("Old Gray full-session oracle differs at record", mismatch + 1)
    for label, rows in [("expected", left), ("actual", right)]:
        print(label, rows[mismatch][:160] if mismatch < len(rows) else b"<missing>")
    sys.stderr.buffer.write(run.stderr)
    raise SystemExit(1)
print(f"Exact warmed old Gray full-session match: {len(actual)} bytes, {len(actual.splitlines())} records; 48 scenarios, 144 Gray snapshots")
for line in run.stdout.splitlines():
    if not line.startswith(b"GRAY_SESSION_"):
        print(line.decode(errors="replace"))
if run.stderr:
    sys.stderr.buffer.write(run.stderr)
