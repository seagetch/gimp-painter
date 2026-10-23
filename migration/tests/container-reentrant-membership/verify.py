#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='08.018/container-reentrant-membership' and not r['whole_hunk_closure'] and not r['rendered_ui_evidence']
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
for n in ['normal-portrait.log','asan-portrait.log']:
 s=(E/n).read_text();assert '1080x1920x24, authenticated accepted, empty authority rejected'in s
 assert len(re.findall(r'^ok \d+ ',s,re.M))==13 and 'not ok'not in s,n
 for case in ['weak-readd','reorder-remove','callback-freeze-later-thaw','nested-does-not-hide-missing']:assert case in s
b=json.loads((E/'asan-build.json').read_text());assert len(b['instrumented_sources'])==4 and not b['changed_during_build'] and b['mutant']is None and not b['leak_detection']
assert sha((E/'original-container.c.txt').read_bytes())==r['original_source_sha256'] and sha((E/'original-red-test.c.txt').read_bytes())==r['red_test_sha256']
assert r['red_exit_code']!=0 and 'removed <= s->count - pos'in(E/'red.log').read_text()
assert len(r['mutations'])==3
for m in r['mutations']:assert m['exit_code']!=0 and 'not ok'in(E/m['log']).read_text()
print('PASS container membership:13 native +4-unit ASan13,3 causal mutations, preserved missing-chain diagnostics and model/weak semantics')
