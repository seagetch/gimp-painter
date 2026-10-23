#!/usr/bin/env python3
"""Replay immutable receipts, optionally bind current source; no runtime claims."""
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='09.014' and r['native_tap_count']==8 and r['actual_production_mutants_rejected']==4
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
rows=[json.loads(s)for s in (E/'native.json').read_text().splitlines()];assert len(rows)==2
assert all(x['result']=='OK' for x in rows);assert sum(len(re.findall(r'^ok \d+ ',x['stdout'],re.M))for x in rows)==8
s=json.loads((E/'sanitizer.json').read_text());assert s['exit_code']==0 and len(re.findall(r'^ok \d+ ',s['stdout'],re.M))==1
assert 'context placement' in s['stdout'] and len(re.findall('context placement',s['stdout']))==16
assert len(re.findall(r'^ok \d+ ',(E/'asan.log').read_text(),re.M))==3
b=json.loads((E/'sanitizer-build.json').read_text());assert len(b['instrumented_sources'])==11 and not b['changed_during_build'] and not b['vptr_sanitizer'] and not b['leak_detection']
m=json.loads((E/'mutations.json').read_text());assert len(m)==3 and all(x['exit_code']!=0 and x['rejected']for x in m)
for x in m:assert 'assertion failed' in (E/(x['name']+'.log')).read_text()
m=json.loads((E/'native-mutant.json').read_text());assert m['exit_code']!=0 and m['rejected'] and 'assertion failed' in (E/'native-mutant.log').read_text()
assert all(json.loads(s)['result']=='OK'for s in (E/'restored.json').read_text().splitlines())
assert not r['production_changed'] and not r['rendered_UI_evidence'] and not r['whole_application']
print('09.014 PASS: 8 native TAP, standalone ASan3/native selected ASan1, 4 production mutants; original byte Gray policy retained')
