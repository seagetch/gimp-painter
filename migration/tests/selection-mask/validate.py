#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args();here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
def no_env(v):
 if isinstance(v,dict):
  assert 'env' not in v
  for x in v.values():no_env(x)
 elif isinstance(v,list):
  for x in v:no_env(x)
for path in here.glob('*.json'):no_env(json.loads(path.read_text()))
m=json.loads((here/'report.json').read_text())
for path,sha in m['evidence_sha256'].items():assert hashlib.sha256((here/path).read_bytes()).hexdigest()==sha,path
rows=json.loads((here/'registered-native.json').read_text());assert len(rows)==3
counts={'gimp-painter-surface':15,'painter-tempbuf':6,'painter-mypaint-coordinate-boundaries':7}
for r in rows:
 name=r['name'].split(' / ')[-1]
 assert r['result']=='OK' and r['returncode']==0 and r['duration']<60 and '# SKIP' not in r['stdout'];assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==counts[name]
 if name=='gimp-painter-surface':
  for case in ('core-signed-offsets','empty-unrestricted','native-handoff-coverage'):assert '/selection/'+case in r['stdout']
mutations=json.loads((here/'mutation-results.json').read_text());assert {r['mutation'] for r in mutations}=={'offset-disconnected','fractional-coverage-ignored'}
for r in mutations:
 text=r['stdout']+r['stderr'];marker="'reference==after' should be TRUE" if r['mutation']=='offset-disconnected' else 'assertion failed (rgba[i+3] == expected'
 assert r['compile_exit_code']==0 and r['test_exit_code']!=0 and r['result']=='FAIL' and marker in text
if args.check_source:
 for path,sha in m['source_sha256'].items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
print('Selection actual native28 TAP and2 expected-red production mutations verified; no raw environment')
