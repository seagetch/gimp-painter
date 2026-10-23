#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text());count=lambda x:len(re.findall(r'^ok \d+',x['stdout'],re.M))
rows=read('normal-results.json');assert len(rows)==5 and all(x['returncode']==0 for x in rows)
for x in rows:
 if x['name'].endswith('prepared-state'):assert count(x)==4
 elif x['name'].endswith(('checkpoint','resource')):assert count(x)==9
 elif x['name'].endswith('legacy-engine'):assert '5637911 bytes, 33768 records' in x['stdout']
 else:assert '1125707 bytes, 81420 records' in x['stdout']
x=read('asan-results.json');assert x['returncode']==0 and count(x)==4
build=read('asan-build.json');assert len(build['instrumented_sources'])==3 and not build['changed_during_build']
rows=read('native-mutations.json');assert len(rows)==2
for x in rows:assert x['returncode']!=0 and ('void equal(' in x['stdout']+x['stderr'] or 'should be FALSE' in x['stdout']+x['stderr'])
with tarfile.open(E/'tested-sources.tar.gz') as t:
 for x in read('source-manifest.json'):
  assert hashlib.sha256(t.extractfile(x['path']).read()).hexdigest()==x['sha256']
  if x['path'] in build['sources_sha256']:assert x['sha256']==build['sources_sha256'][x['path']]
  if a.check_source:assert hashlib.sha256((R/x['path']).read_bytes()).hexdigest()==x['sha256'],x['path']
subprocess.run(['python3',str(E.parent/'engine-checkpoint-09011/validate.py')],check=True)
print('PASS: prepared evaluator4 normal/ASan4;2 mutants;historical Resource pair/noalloc exchange;checkpoint9/resource9/full engine+decoder regressions;immutable predecessor')
