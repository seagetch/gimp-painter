#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text());count=lambda x:len(re.findall(r'^ok \d+',x['stdout'],re.M))
rows=read('normal-results.json');assert len(rows)==2 and sorted(count(x) for x in rows)==[11,12] and all(x['returncode']==0 for x in rows)
x=read('asan-results.json');assert x['returncode']==0 and count(x)==11
build=read('asan-build.json');assert len(build['instrumented_sources'])==6 and not build['changed_during_build']
rows=read('regression-results.json');assert len(rows)==5 and sorted(count(x) for x in rows)==[0,0,18,24,24] and all(x['returncode']==0 for x in rows)
x=read('header-compile-result.json');assert x['status']=='PASS' and x['compiled_probes']==431
rows=read('native-mutations.json');assert len(rows)==3
for x in rows:assert x['returncode']!=0 and ('should be TRUE' in x['stdout']+x['stderr'] or 'should be FALSE' in x['stdout']+x['stderr']) and x['production_sha256']==build['sources_sha256']['app/core/gimpimage-undo.c']
assert '1080x1920x24' in (E/'regressions.log').read_text()
with tarfile.open(E/'tested-sources.tar.gz') as t:
 for x in read('source-manifest.json'):
  assert hashlib.sha256(t.extractfile(x['path']).read()).hexdigest()==x['sha256']
  if x['path'] in build['sources_sha256']:assert x['sha256']==build['sources_sha256'][x['path']]
  if a.check_source:assert hashlib.sha256((R/x['path']).read_bytes()).hexdigest()==x['sha256'],x['path']
print('PASS: optional native pop guard11/ASan11;3 mutants;prepared12/Surface24/UI18+24/header431;source archive')
