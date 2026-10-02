#!/usr/bin/env python3
"""Exact native pre-refactor interpolation coordinates/state over pause budgets."""
import gzip, hashlib, subprocess, sys
from pathlib import Path
expected=gzip.decompress(Path(sys.argv[1]).read_bytes())
for budget in [0,1,2,7,31,4096]:
    p=subprocess.run([sys.argv[2],str(budget)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if p.returncode:
        sys.stderr.buffer.write(p.stderr);raise SystemExit(p.returncode)
    actual=b'\n'.join(line for line in p.stdout.splitlines() if line.startswith(b'INTERP '))+b'\n'
    if actual!=expected:
        old=expected.splitlines();new=actual.splitlines()
        for index,(a,b) in enumerate(zip(old,new)):
            if a!=b:
                print('budget',budget,'first differing record',index,'\nexpected',a.decode(),'\nactual',b.decode());break
        print('lengths',len(expected),len(actual));raise SystemExit(1)
    print('budget',budget,'exact',len(actual),'bytes',len(actual.splitlines()),'records',hashlib.sha256(actual).hexdigest())
