#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--check-committed-engine-checkpoint',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();assert not(a.check_source and a.check_committed_engine_checkpoint)
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='09.016' and r['original_source_duties']==69 and r['whole_hunk_status']=='TODO'
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
  if a.check_committed_engine_checkpoint:
   c=r['concurrent_engine_checkpoint'];data=subprocess.check_output(['git','show',c['commit']+':'+n],cwd=R)if n in c['paths']else(R/n).read_bytes();assert sha(data)==h,n
rows=[json.loads(s)for s in (E/'native.json').read_text().splitlines()];assert len(rows)==3 and all(x['result']=='OK'for x in rows);assert sum(len(re.findall(r'^ok \d+ ',x['stdout'],re.M))for x in rows)==37
native=next(x for x in rows if x['name'].endswith(' / gimp-painter-surface'));assert native['stdout'].count('tiny=')==72
s=json.loads((E/'sanitizer.json').read_text());assert len(s)==2 and all(x['exit_code']==0 for x in s);assert sum(len(re.findall(r'^ok \d+ ',x['stdout'],re.M))for x in s)==7;assert s[0]['stdout'].count('tiny=')==72
assert len(re.findall(r'^ok \d+ ',(E/'read-asan.log').read_text(),re.M))==8
b=json.loads((E/'sanitizer-build.json').read_text());assert len(b['instrumented_sources'])==14 and not b['changed_during_build'] and not b['vptr_sanitizer'] and not b['leak_detection']
for name in ['native-mutant','stride-mutant']:
 m=json.loads((E/(name+'.json')).read_text());assert m['exit_code']!=0 and m['rejected'] and 'not ok'in(E/(name+'.log')).read_text()
rows=[json.loads(s)for s in(E/'restored.json').read_text().splitlines()];assert len(rows)==1 and rows[0]['result']=='OK' and len(re.findall(r'^ok \d+ ',rows[0]['stdout'],re.M))==1
assert not r['production_changed'] and not r['rendered_UI_evidence']
print('09.016 PASS:37 native TAP,14-unit ASan7/read-ASan8,2 mutants;69 source-specific duties remain explicitly owned')
if a.check_committed_engine_checkpoint:print('Engine verified at explicit immutable committed checkpoint, NOT current in-progress Engine sources')
