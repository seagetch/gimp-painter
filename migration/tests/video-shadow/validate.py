#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,tarfile,argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
h=json.loads((E/'source-sha256.json').read_text())
with tarfile.open(E/'sources.tar.gz')as t:
 assert set(t.getnames())==set(h)
 for n,v in h.items():
  assert sha(t.extractfile(n).read())==v,n
  if a.check_source:assert sha((R/n).read_bytes())==v,n
for n,needles in [('results.json',['Video1440 geometry','Video18 raw','Video 130 raw/shadow','Video options and ROI snapshots','Red Eye:117440512','Value Invert exact pinned C callback: all 16,777,216']),('colorify-regression.json',['Colorify120 geometry','Colorify134217728','Colorify 130 raw/shadow']),('exchange-regression.json',['Exchange90 geometry','Exchange 130 raw/shadow'])]:
 j=json.loads((E/n).read_text());assert set(j['runs'])=={'normal','sanitizer'}and not j['changed_after_run']
 for p,v in j['source_sha256'].items():assert h[p]==v
 for row in j['runs'].values():
  assert row['exit_code']==0 and not row['stderr']and '-std=c++14'in row['compile']and'-fno-rtti'in row['compile']and'-fno-sanitize=vptr'in row['compile']
  for needle in needles:assert needle in row['stdout'],needle
 assert '-fsanitize=address,undefined,float-cast-overflow'in j['runs']['sanitizer']['compile']
m=json.loads((E/'mutations.json').read_text());assert {x['name']for x in m}=={'crop-phase-reset','chunk-phase-reset','area-limit-removed'}
for x in m:assert x['returncode']!=0 and x['source_sha256']==h[x['path']]
p=json.loads((E/'headers.json').read_text());assert p['returncode']==0 and '-std=c++14'in p['command']and'-fno-rtti'in p['command']
for n,v in json.loads((E/'owned-sha256.json').read_text()).items():assert h[n]==v
subprocess.run(['python3','-B',str(E.with_name('video-kernel')/'validate.py')],check=True)
print('PASS Video raw/ROI1440+18 scenes,130 fault positions,3 mutants; normal/sanitizer priorpoint+Colorify+Exchange regressions; native admission pending')
