#!/usr/bin/env python3
import argparse,json,pathlib,hashlib,tarfile,re
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[3];read=lambda n:json.loads((E/n).read_text())
n=read('normal-results.json');assert len(n)==1 and n[0]['returncode']==0
s=read('asan-results.json');assert s['returncode']==0
for row in [n[0],s]:assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==2
assert int((E/'red-status.txt').read_text())!=0 and 'undefined reference' in (E/'expected-red.log').read_text()
m=read('native-mutation.json');assert m['returncode']!=0 and 'after' in m['stderr'] and 'NULL' in m['stderr']
h=read('header-compile-result.json');assert h['status']=='PASS' and h['compiled_probes']==419
b=read('asan-build.json');assert len(b['instrumented_sources'])==5 and not b['changed_during_build']
with tarfile.open(E/'tested-owner-sources.tar.gz') as tar:
 for row in read('source-manifest.json'):
  assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
  if a.check_source:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
print('PASS: original-symbol red; native2/sanitizer2; incorrect alias rejected;419 headers; bounded source identities')
