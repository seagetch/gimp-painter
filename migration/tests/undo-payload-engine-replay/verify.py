#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert not r['whole_hunk_closure']and not r['rendered_ui_evidence']
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
 for n in ['app/paint/painter-mypaint/engine.cpp','app/paint/painter-mypaint/engine.hpp','app/paint/painter-mypaint/legacy-brush.hpp']:assert sha(subprocess.check_output(['git','show',r['engine_checkpoint']+':'+n],cwd=R))==r['source_sha256'][n]
x=[json.loads(s)for s in(E/'native.json').read_text().splitlines()];assert len(x)==1 and x[0]['result']=='OK'and'env'not in x[0]
for s in [x[0]['stdout'],(E/'asan-portrait.log').read_text()]:assert len(re.findall(r'^ok \d+ ',s,re.M))==24 and s.count('native Undo payload precision=')==32
for n in ['normal-portrait.log','asan-portrait.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected'in(E/n).read_text()
b=json.loads((E/'asan-build.json').read_text());assert len(b['instrumented_sources'])==11 and not b['changed_during_build'] and not b['vptr_sanitizer']and not b['leak_detection']
print('PASS current committed Engine replay:24 normal+11-unit ASan24;32 Undo payload cells;exact candidate helper checkpoint;no additional task closure')
