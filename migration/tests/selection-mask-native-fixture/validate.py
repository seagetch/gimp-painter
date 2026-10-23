#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,subprocess
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
d=json.loads((here/'full-diagnostic.json').read_text());assert any(r['result']=='FAIL' and 'stored=0.25000000903060116' in r['stdout'] and 'assertion failed (rgba[i+3] == expected' in r['stdout']+r['stderr'] for r in d)
h=json.loads((here/'head-native.json').read_text());data=subprocess.check_output(['git','show',h['baseline_commit']+':'+h['harness_path']],cwd=root);assert hashlib.sha256(data).hexdigest()==h['harness_sha256']
assert len(h['runs'])==1
for r in h['runs']:
 assert r['result']=='OK' and r['returncode']==0 and r['duration']<60
 assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==18
rows=json.loads((here/'repeated-native.json').read_text());assert len(rows)==10
for r in rows:
 assert r['result']=='OK' and r['returncode']==0 and r['duration']<60 and '# SKIP' not in r['stdout']
 assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==19
if a.check_source:
 for path,sha in m['source_sha256'].items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
assert h['corrected_fixture_sha256']==m['source_sha256']['app/tests/test-painter-selection-mask.inc']
print('Native scalar fixture repair: committed18 TAP +10x19 TAP PASS; prior stored-value failure retained; tolerance unchanged')
