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
rows=json.loads((here/'registered-native.json').read_text());assert len(rows)==5
counts={'gimp-painter-surface':18,'painter-tempbuf':6,'painter-mypaint-coordinate-boundaries':7,'projection-update-bounds':3,'painter-mypaint-tool':24}
for r in rows:
 name=r['name'].split(' / ')[-1]
 assert r['result']=='OK' and r['returncode']==0 and r['duration']<(120 if name=='painter-mypaint-tool' else 60) and '# SKIP' not in r['stdout'];assert len(re.findall(r'^ok \d+ /',r['stdout'],re.M))==counts[name]
 if name=='gimp-painter-surface':
  for case in ('native-sampler-precision','apply-read-raw-paint','independent-paint-target'):assert '/layer-mask/'+case in r['stdout']
mutations=json.loads((here/'mutation-results.json').read_text());assert {r['mutation'] for r in mutations}=={'ignore-apply-state','mask-as-selection','retain-stale-hot-tile'}
for r in mutations:
 text=r['stdout']+r['stderr'];marker={'ignore-apply-state':'assertion failed (gimp_pickable_get_opacity_at','mask-as-selection':"'reference==after' should be TRUE",'retain-stale-hot-tile':'assertion failed (projected == expected'}[r['mutation']]
 assert r['compile_exit_code']==0 and r['test_exit_code']!=0 and r['result']=='FAIL' and marker in text
probe=json.loads((here/'hot-tile-probe.json').read_text());assert probe['gegl_version']=='0.4.62' and len(probe['runs'])==2
for r in probe['runs']:assert r['exit_code']==0 and r['duration']<30 and 'flushed 255 0 0 255' in r['stdout'] and not r['stderr'].strip()
if args.check_source:
 for path,sha in {**m['source_sha256'],**probe['source_sha256']}.items():assert hashlib.sha256((root/path).read_bytes()).hexdigest()==sha,path
print('Layer-mask actual native58 TAP and3 expected-red production mutations verified; no raw environment')
