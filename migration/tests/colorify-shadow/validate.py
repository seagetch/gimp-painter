#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,argparse
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
h=json.loads((E/'source-sha256.json').read_text())
with tarfile.open(E/'sources.tar.gz') as t:
 assert set(t.getnames())==set(h)
 for n,v in h.items():
  assert sha(t.extractfile(n).read())==v,n
  if a.check_source:assert sha((R/n).read_bytes())==v,n
for filename,needles in [('results.json',['Colorify120 geometry','Colorify134217728','Colorify 130 raw/shadow','Colorify immutable snapshot','Red Eye:117440512','Value Invert exact pinned C callback: all 16,777,216']),('exchange-regression.json',['Exchange90 geometry','Exchange 130 raw/shadow','Exchange parameter snapshot survives'])]:
 r=json.loads((E/filename).read_text());assert not r['changed_after_run'] and set(r['runs'])=={'normal','sanitizer'}
 for n,v in r['source_sha256'].items():assert h[n]==v
 for row in r['runs'].values():
  assert row['exit_code']==0 and not row['stderr'] and '-std=c++14'in row['compile'] and '-fno-rtti'in row['compile'] and '-fno-sanitize=vptr'in row['compile']
  for needle in needles:assert needle in row['stdout'],needle
 assert '-fsanitize=address,undefined,float-cast-overflow'in r['runs']['sanitizer']['compile']
m=json.loads((E/'mutations.json').read_text());assert {x['name'] for x in m}=={'skip-area-limit','skip-colorify-pixel'}
for x in m:assert x['returncode']!=0 and x['source_sha256']==h[x['path']]
p=json.loads((E/'headers.json').read_text());assert p['returncode']==0 and '-std=c++14'in p['command'] and '-fno-rtti'in p['command']
for n,v in json.loads((E/'owned-sha256.json').read_text()).items():assert h[n]==v
print('PASS Colorify raw/ROI120 scenes,134217728 comparisons,130 fault positions; prior point and Exchange normal/sanitizer regressions; native admission pending')
