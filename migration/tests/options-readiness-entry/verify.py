#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();read=lambda p:json.loads((E/p).read_text())
for phase in ['red','reviewed-red','pre-adoption-red','green']:
 m=read(phase+'/source-manifest.json')
 assert m['base']=='2ef2dd32eeb7201e8e0318f169c6005a21c10410'
 assert len(m['held_exact_base'])==12
 with tarfile.open(E/phase/'tested-inputs.tar.gz') as t:
  for path,h in m['input_sha256'].items(): assert sha(t.extractfile(path).read())==h,path
m=read('green/source-manifest.json')
if a.check_owned_source:
 for path,h in m['owned_sha256'].items(): assert sha((R/path).read_bytes())==h,path
reds=[read('red/result.json'),*read('reviewed-red/blanket-red-results.json'),read('pre-adoption-red/result.json')]
for i,row in enumerate(reds):
 assert row['returncode']!=0
 assert ('toggle.calls == 0' if i==3 else 'options_readiness_c_consumer(o) == expected') in row['stdout']
 assert ('(2 == 0)' if i==3 else '(0 == 1)') in row['stdout']
rows=read('green/normal.json');assert len(rows)==5
counts={r['name'].split(' / ')[-1]:r for r in rows}
for name,n in [('gimp-painter-options',24),('mypaint-options-mode',6),('mypaint-enum-public',2),('mypaint-options-readiness',20)]:
 row=counts[name];assert row['returncode']==0 and row['result']=='OK';assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==n
assert counts['mypaint-enum-library-only']['returncode']==0
row=read('green/asan-results.json');assert row['returncode']==0;assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==20
assert not re.search(r'ERROR: AddressSanitizer|runtime error:',row['stdout']+(row['stderr'] or ''))
b=read('green/asan-build.json');assert len(b['instrumented_sources'])==5
for path,h in b['instrumented_sources'].items():assert h==m['input_sha256'][path]
assert read('green/asan-post.json')['dependency_objects_unchanged']
assert read('green/final-input-seal.json')['all_recorded_inputs_unchanged']
assert read('green/source-after.json')['all_source_unchanged']
h=read('green/headers.json');assert h['status']=='PASS' and h['routes']==42 and h['probes']==88
for name in ['normal-portrait.log','asan-portrait.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/'green'/name).read_text()
o=read('legacy-source/oracle.json');assert [x['ready'] for x in o['trace']]==[False,False,True,True,True,True]
for path,h in o['input_sha256'].items():assert sha((E/'legacy-source'/path).read_bytes())==h
for path,h in read('evidence-manifest.json').items():assert sha((E/path).read_bytes())==h,path
print('PASS: readiness normal53 + selected5-unit ASan20 + header88; five causal reds; exact2ef overlay; source-derived old oracle')
