#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
def no_env(v):
 if isinstance(v,dict):
  assert 'env' not in v
  for x in v.values():no_env(x)
 elif isinstance(v,list):
  for x in v:no_env(x)
for path in here.glob('*.json'):no_env(json.loads(path.read_text()))
m=json.loads((here/'report.json').read_text())
for path,sha in m['evidence_sha256'].items():assert hashlib.sha256((here/path).read_bytes()).hexdigest()==sha,path
counts={'gimp-painter-surface':19,'painter-tempbuf':6,'painter-mypaint-coordinate-boundaries':7,'projection-update-bounds':3,'painter-mypaint-tool':24,'painter-mypaint-snapshot-isolation':2}
rows=json.loads((here/'registered-native.json').read_text());assert len(rows)==len(counts)
for r in rows:
 name=r['name'].split(' / ')[-1]
 assert r['result']=='OK' and r['returncode']==0 and r['duration']<(120 if name=='painter-mypaint-tool' else 60) and '# SKIP' not in r['stdout']
 assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==counts[name]
 if name=='gimp-painter-surface':assert '/snapshot/native-start-handoff' in r['stdout']
mutations=json.loads((here/'mutation-results.json').read_text());assert {x['mutation'] for x in mutations}=={'alias-native-snapshot','capture-after-native-start'}
for r in mutations:
 marker={'alias-native-snapshot':'snapshot!=target','capture-after-native-start':'snapshot_bytes(target,probe)==snapshot_bytes(control,probe)'}[r['mutation']]
 assert r['compile_exit_code']==0 and r['test_exit_code']!=0 and r['result']=='FAIL' and marker in r['stdout']+r['stderr']
s=json.loads((here/'standalone.json').read_text());assert len(s['runs'])==2 and {r['sanitized'] for r in s['runs']}=={True,False}
for r in s['runs']:
 assert r['exit_code']==0 and r['duration']<60 and not r['stderr'].strip()
 assert len(re.findall(r'^ok \d+ /snapshot-isolation/',r['stdout'],re.M))==2
d=json.loads((here/'default-mutation.json').read_text())
assert d['compile_exit_code']==0 and d['exit_code']!=0 and d['duration']<60 and 'bytes(left,rect)==bytes(right,rect)' in d['stdout']+d['stderr']
assert d['original_sha256']==m['source_sha256'][d['path']] and d['mutated_sha256']!=d['original_sha256']
if a.check_source:
 for path,sha in {**m['source_sha256'],**s['source_sha256']}.items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
print('61 native TAP;3 actual production expected-red controls;existing snapshot2 normal+focusedASan PASS; no environment maps')
