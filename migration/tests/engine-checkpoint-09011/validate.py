#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text())
rows=read('normal-results.json');assert len(rows)==4 and all(x['returncode']==0 for x in rows)
for x in rows:
 if x['name'].endswith(('checkpoint','resource')):assert len(re.findall(r'^ok \d+',x['stdout'],re.M))==9
 elif x['name'].endswith('legacy-engine'):assert '5637911 bytes, 33768 records' in x['stdout'] and '27392 dabs, 3172 samples' in x['stdout']
 else:assert '1125707 bytes, 81420 records' in x['stdout']
x=read('asan-results.json');assert x['returncode']==0 and len(re.findall(r'^ok \d+',x['stdout'],re.M))==9
x=read('asan-build.json');assert len(x['instrumented_sources'])==3 and not x['changed_during_build']
rows=read('native-mutations.json');assert len(rows)==2
for x in rows:assert x['returncode']!=0 and 'void equal(' in x['stderr']+x['stdout']
x=read('cpp-boundary.json');assert x['returncode']==0 and '-fno-rtti' in x['command'] and '-std=c++14' in x['command'];assert x['probe'].count('#include "engine.hpp"')==2
x=read('header-compile-result.json');assert x['status']=='PASS' and x['compiled_probes']==423
assert len(read('state-audit.json')['fields'])==13
with tarfile.open(E/'tested-sources.tar.gz') as t:
 for x in read('source-manifest.json'):
  assert hashlib.sha256(t.extractfile(x['path']).read()).hexdigest()==x['sha256']
  if a.check_source:assert hashlib.sha256((R/x['path']).read_bytes()).hexdigest()==x['sha256'],x['path']
print('PASS: immutable checkpoint9 normal/sanitizer;2mutants;full state audit;exact legacy engine/decoder traces;C++14 noRTTI boundary;source identities')
