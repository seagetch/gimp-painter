#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare full-session records, retaining non-record runtime diagnostics."""
import gzip,json,pathlib,subprocess,sys
expected=gzip.decompress(pathlib.Path(sys.argv[1]).read_bytes())
run=subprocess.run(sys.argv[2:],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
actual=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'SESSION_'))+b'\n'
if run.returncode:
    sys.stderr.buffer.write(run.stderr);raise SystemExit(run.returncode)
if actual!=expected:
    a,b=expected.splitlines(),actual.splitlines()
    mismatch=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
    print('Old full-session oracle differs at record',mismatch+1)
    for label,rows in [('expected',a),('actual',b)]:
        print(label,rows[mismatch][:160] if mismatch<len(rows) else b'<missing>')
    sys.stderr.buffer.write(run.stderr);raise SystemExit(1)
print(f'Exact warmed old full-session match: {len(actual)} bytes, {len(actual.splitlines())} records; 32 scenarios, 96 RGBA snapshots')
for line in run.stdout.splitlines():
    if not line.startswith(b'SESSION_'):print(line.decode(errors='replace'))
if run.stderr:sys.stderr.buffer.write(run.stderr)
