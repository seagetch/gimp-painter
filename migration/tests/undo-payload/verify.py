#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--check-committed-engine-checkpoint',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();assert not(a.check_source and a.check_committed_engine_checkpoint)
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='09.017' and r['original_source_duties']==36 and r['whole_hunk_status']=='TODO'
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
  if a.check_committed_engine_checkpoint:
   c=r['concurrent_engine_checkpoint'];data=subprocess.check_output(['git','show',c['commit']+':'+n],cwd=R)if n in c['paths']else(R/n).read_bytes();assert sha(data)==h,n
rows=[json.loads(s)for s in (E/'native.json').read_text().splitlines()];assert len(rows)==3 and all(x['result']=='OK'for x in rows);assert sum(len(re.findall(r'^ok \d+ ',x['stdout'],re.M))for x in rows)==38
native=next(x for x in rows if x['name'].endswith(' / gimp-painter-surface'));assert native['stdout'].count('native Undo payload precision=')==32
s=json.loads((E/'sanitizer.json').read_text());assert s['exit_code']==0 and len(re.findall(r'^ok \d+ ',s['stdout'],re.M))==1 and s['stdout'].count('native Undo payload precision=')==32
b=json.loads((E/'sanitizer-build.json').read_text());assert len(b['instrumented_sources'])==11 and not b['changed_during_build'] and not b['vptr_sanitizer'] and not b['leak_detection']
mutants=json.loads((E/'mutations.json').read_text());assert len(mutants)==2
for m in mutants:assert m['exit_code']!=0 and m['rejected'] and 'not ok'in(E/(m['name']+'.log')).read_text()
rows=[json.loads(s)for s in(E/'restored.json').read_text().splitlines()];assert len(rows)==1 and rows[0]['result']=='OK' and len(re.findall(r'^ok \d+ ',rows[0]['stdout'],re.M))==1
assert not r['production_changed'] and not r['rendered_UI_evidence']
assert not r['engine_stroke_replay_proven']
print('09.017 PASS:38 native TAP,11-unit ASan1,32 native payload cases/3cycles,2 mutants;36 original duties preserved')
if a.check_committed_engine_checkpoint:print('Engine verified at explicit immutable committed checkpoint, NOT current in-progress Engine sources')
