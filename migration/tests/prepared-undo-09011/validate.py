#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text());count=lambda x:len(re.findall(r'^ok \d+',x['stdout'],re.M))
rows=read('normal-results.json');assert len(rows)==1 and rows[0]['returncode']==0 and count(rows[0])==12
x=read('asan-results.json');assert x['returncode']==0 and count(x)==12
build=read('asan-build.json');assert len(build['instrumented_sources'])==6 and not build['changed_during_build']
rows=read('regression-results.json');assert len(rows)==3 and sorted(count(x) for x in rows)==[9,18,24] and all(x['returncode']==0 for x in rows)
rows=read('header-results.json');assert len(rows)==2 and all(x['returncode']==0 for x in rows)
x=read('header-compile-result.json');assert x['status']=='PASS' and x['compiled_probes']==427
rows=read('native-mutations.json');assert len(rows)==2
for x in rows:assert x['returncode']!=0 and 'should be NULL' in x['stdout']+x['stderr'] and x['production_sha256']==build['sources_sha256']['app/core/gimpimage-undo.c']
assert 'undefined reference' in (E/'missing-symbol.log').read_text() and 'gimp_image_undo_push_prepared' in (E/'missing-symbol.log').read_text()
assert 'should be NULL' in (E/'dangling-return-red.log').read_text()
assert '1080x1920x24' in (E/'regressions.log').read_text() and 'empty authority rejected' in (E/'mutations.log').read_text()
with tarfile.open(E/'tested-sources.tar.gz') as t:
 for x in read('source-manifest.json'):
  assert hashlib.sha256(t.extractfile(x['path']).read()).hexdigest()==x['sha256']
  if x['path'] in build['sources_sha256']:assert x['sha256']==build['sources_sha256'][x['path']]
  if a.check_source:assert hashlib.sha256((R/x['path']).read_bytes()).hexdigest()==x['sha256'],x['path']
print('PASS: prepared Undo12 normal/6-unit ASan12;427 headers;2 mutants;portrait UI18+24/paint9;causal red and exact source archive')
