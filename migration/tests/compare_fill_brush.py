#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
import gzip,subprocess,sys
from pathlib import Path
expected=gzip.decompress(Path(sys.argv[1]).read_bytes())
run=subprocess.run(sys.argv[2:],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
if run.returncode:
 sys.stderr.buffer.write(run.stderr);raise SystemExit(run.returncode)
actual=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'BRUSH ') or x==b'FILL_BRUSH_CAPTURE_COMPLETE')+b'\n'
if actual!=expected:
 a,b=expected.splitlines(),actual.splitlines();index=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)));print('Fill Brush oracle differs at record',index,'expected/actual counts',len(a),len(b));raise SystemExit(1)
print(f'Exact real legacy Fill Brush match: {len(actual)} bytes, 12 scenarios, 36 finish/Undo/Redo snapshots')
