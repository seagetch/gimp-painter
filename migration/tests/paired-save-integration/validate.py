#!/usr/bin/env python3
import pathlib,json,hashlib,re,tarfile,argparse,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda x:len(re.findall(r'^ok \d+',x['stdout'],re.M))
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
rows=read('normal-results.json');assert len(rows)==5 and sorted(count(x)for x in rows)==[1,16,18,24,27]and all(x['returncode']==0 for x in rows)
rows=read('final-asan-results.json');assert sorted(count(x)for x in rows)==[18,27]and all(x['returncode']==0 for x in rows)
b=read('final-asan-build.json');assert len(b['instrumented_sources'])==16 and not b['changed_during_build'];assert 'app/tools/gimppaintermybrushtool.cpp'in b['instrumented_sources']
rows=read('red-results.json');assert len(rows)==1 and rows[0]['returncode']!=0 and 'should be FALSE'in rows[0]['stdout'];assert 'gimp_tool_button_release'in (E/'second-red.log').read_text()
rows=read('query-mutations.json');assert len(rows)==2
for x in rows:assert x['returncode']!=0 and 'should be FALSE'in x['stdout']+x['stderr']and x['production_sha256']==b['sources_sha256']['app/paint/painter-mypaint-surface/paint-core.cpp']
x=read('tool-mutation.json')[0];assert x['returncode']!=0 and 'gimp_tool_button_release'in x['stdout']+x['stderr']and x['production_sha256']==b['sources_sha256']['app/tools/gimppaintermybrushtool.cpp']
rows=read('headers.json');assert len(rows)==4 and all((x['returncode']!=0)==x['baseline']for x in rows)
for x in rows:assert ('-std=c++14'in x['command'] and '-fno-rtti'in x['command'])if x['language']=='cpp'else '-std=c11'in x['command']
x=read('marker-order.json');assert x['source_sha256']==b['sources_sha256']['app/core/gimpimage-undo.c'];assert list(x['ordered_sites'].values())==sorted(x['ordered_sites'].values());assert x['both_undo_and_redo_share_preflight']
x=read('source-gate.json');assert x['status']=='PASS'and not x['errors']
rows=read('facade-replay/normal-results.json');assert len(rows)==1 and rows[0]['returncode']==0 and count(rows[0])==3
x=read('facade-replay/asan-results.json');assert x['returncode']==0 and count(x)==3
assert len(read('facade-replay/asan-build.json')['instrumented_sources'])==7
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for row in read('source-manifest.json'):
  n=row['path'];assert sha(t.extractfile(n).read())==row['sha256']
  if n in b['sources_sha256']:assert row['sha256']==b['sources_sha256'][n]
  if a.check_source:assert sha((R/n).read_bytes())==row['sha256'],n
seal=read('held-source-seal.json');assert len(seal)==9
with tarfile.open(E/'held-baseline-sources.tar.gz')as t:
 for x in seal:assert sha(t.extractfile(x['path']).read())==x['tested']
for n,h in read('predecessor-pins.json').items():assert sha((R/n).read_bytes())==h,n
for path in ['paired-stroke-09011/validate.py','adapter-removal/mypaint-undo-ref-08018/validate.py','paired-ui-default-directory/validate.py']:
 subprocess.run([sys.executable,str(R/'migration/tests'/path)],check=True,capture_output=True,text=True)
print('PASS corrected save/history integration: tool27+paired18 ASan16units; normal24/27/16/18/UI1;3causal mutants;facade3+ASan3;source/order/exact-site and immutable predecessor proofs; external geometry parent open')
