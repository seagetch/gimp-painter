#!/usr/bin/env python3
from pathlib import Path
import hashlib,json,tarfile,argparse,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'results.json').read_text());assert not r['changed_after_run'];assert set(r['runs'])=={'normal-matrix','normal-contracts','sanitizer-matrix','sanitizer-contracts'}
with tarfile.open(E/'sources.tar.gz')as t:
 assert set(t.getnames())==set(r['source_sha256'])
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
for name,row in r['runs'].items():
 assert row['exit_code']==0 and not row['stderr'] and '-std=c++14'in row['compile'] and '-fno-rtti'in row['compile']
 assert ('13708800 independent'in row['stdout'])if name.endswith('matrix')else('product28 invalid-input'in row['stdout']and'54 exact/partial aliases'in row['stdout'])
 if name.startswith('sanitizer'):assert '-fsanitize=address,undefined'in row['compile']
assert {p['include_count']for p in r['header_probes']}=={1,2}
for p in r['header_probes']:assert p['returncode']==0 and '-std=c++14'in p['command']and'-fno-rtti'in p['command']
m=json.loads((E/'mutations.json').read_text());assert {x['name']for x in m}=={'discard-origin','ignore-rotation','wrong-saturation','missing-pattern-admission','erase-alpha'}
for x in m:assert x['returncode']!=0 and x['source_sha256']==r['source_sha256']['app/painter/filter-video-kernel.hpp']
f=json.loads((E.with_name('video-foundation')/'results.json').read_text())
for name in ['compare.c','reference-patterns.h']:assert r['source_sha256']['migration/tests/video-foundation/'+name]==f['source_sha256'][name]
subprocess.run(['python3','-B',str(E.with_name('video-foundation')/'validate.py')],check=True)
print('PASS immutable Video kernel normal/sanitizer13708800 comparisons,28 input guards,54 aliases,5 mutants,native14 first/double headers; native route pending')
