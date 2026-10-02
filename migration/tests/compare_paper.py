#!/usr/bin/env python3
"""Compare real native textured masks/strokes against the pinned old executable.

Untextured pressure masks intentionally retain GIMP3 arithmetic; they are not
part of this paper oracle. The C++ native test separately consumes all original
raw masks to exhaustively validate paper without a shared mask producer.
"""
import argparse
import gzip
import os
from pathlib import Path
import subprocess
p=argparse.ArgumentParser();p.add_argument('fixture',type=Path);p.add_argument('binary',type=Path);p.add_argument('--modes',action='store_true');a=p.parse_args()
env=dict(os.environ)
if a.modes:env['PAINTER_PAPER_MODES_FIXTURE']='1'
else:env.pop('PAINTER_PAPER_MODES_FIXTURE',None)
def normalized(raw):
    records=[]
    for row in raw.splitlines():
        fields=row.split()
        if not fields:continue
        if fields[0] in [b'PAPER_STROKE',b'PAPER_CAPTURE_COMPLETE'] or (fields[0]==b'PAPER_MASK' and fields[2]==b'1'):records.append(row)
    return b'\n'.join(records)+b'\n'
run=subprocess.run([str(a.binary)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
if run.returncode:print(run.stderr.decode());raise SystemExit(run.returncode)
expected=normalized(gzip.decompress(a.fixture.read_bytes()));actual=normalized(run.stdout)
if actual!=expected:
    left=expected.splitlines();right=actual.splitlines()
    mismatch=next((i for i,(x,y) in enumerate(zip(left,right)) if x!=y),min(len(left),len(right)))
    print('Paper mismatch at record',mismatch, 'expected',left[mismatch][:110] if mismatch<len(left) else '<end>', 'actual',right[mismatch][:110] if mismatch<len(right) else '<end>')
    raise SystemExit(1)
print(f'Exact old paper match: {len(expected)} bytes, {len(expected.splitlines())} records')
