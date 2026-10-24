#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'results.json').read_text());assert len(r['source_sha256'])==31 and not r['changed_after_run'] and set(r['runs'])=={'normal','sanitizer'}
with tarfile.open(E/'sources.tar.gz')as t:
 assert set(t.getnames())==set(r['source_sha256'])
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
for name,row in r['runs'].items():
 assert row['exit_code']==0 and '-fno-rtti'in row['compile'] and '-fno-sanitize=vptr'in row['compile']
 for text in ['Exchange90 geometry/options/layout scenes','Exchange parameter snapshot survives','Exchange original RGB3/RGBA4 row limits','Exchange 130 raw/shadow','Red Eye:117440512','Value Invert exact pinned C callback: all 16,777,216'] :assert text in row['stdout'],text
assert '-fsanitize=address,undefined,float-cast-overflow'in r['runs']['sanitizer']['compile']
m=json.loads((E/'mutations.json').read_text());assert {x['name']for x in m}=={'unrelated-area-limit','raw-mutable-options','shadow-mutable-options'}
for x in m:assert x['returncode']!=0 and x['source_sha256']==r['source_sha256'][x['path']]
h=json.loads((E/'headers.json').read_text());assert h['returncode']==0 and '-fno-rtti'in h['command']and '-std=c++14'in h['command']
subprocess.run(['python3','-B',str(R/'migration/tests/color-exchange-kernel/validate.py')],check=True)
print('PASS ColorExchange raw/ROI90 scenes,130 I/O fault points,5-unit sanitizer and complete point regressions; native owner route pending')
