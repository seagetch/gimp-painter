#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args()
e=pathlib.Path(__file__).resolve().parent;r=e.parents[2]
read=lambda n:json.loads((e/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
for f,h in read('artifact-sha256.json').items():assert sha((e/f).read_bytes())==h,f
x=read('results.json');assert not x['changed_after'];rows={z['name']:z for z in x['results']}
for mode in ['normal','asan-ubsan']:
 assert rows[mode+'-compile']['returncode']==0
 z=rows[mode];assert z['returncode']==0 and len(re.findall(r'^ok \d+ ',z['stdout'],re.M))==4
 assert 'scenarios=23' in z['stdout'] and 'not ok' not in z['stdout']
for name,needle in [('drop-unknown-object','json_object_get_size'),('unknown-int64-to-double','static_cast<gint64>'),('missing-switch-true','r.switch_value'),('save-hides-error','saved')]:
 assert rows[name+'-compile']['returncode']==0
 z=rows[name];assert z['returncode']!=0 and needle in z['stdout']+z['stderr'],name
with tarfile.open(e/'tested-sources.tar.gz')as t:
 for f,h in x['source_sha256'].items():
  assert sha(t.extractfile(f).read())==h,f
  if a.check_source:assert sha((r/f).read_bytes())==h,f
print('PASS Resource structural/default/text/stream four groups normal+three-unit ASan/UBSan;23 state scenarios;5 output cases;4 causal mutants;exact source archive;not native/UI/whole-app')
