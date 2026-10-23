#!/usr/bin/env python3
import argparse,json,pathlib,hashlib,re,tarfile,struct,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[3];read=lambda n:json.loads((E/n).read_text())
# Immutable predecessor replay, deliberately not --check-source against new headers.
subprocess.run([sys.executable,str(E.parent/'preset-action-group-entry-08018/validate.py')],check=True)
n=read('normal-results.json');assert len(n)==1 and n[0]['returncode']==0
s=read('asan-results.json');assert s['returncode']==0
for row in [n[0],s]:assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==4
for row in read('ui-results.json'):
 assert row['returncode']==0
 assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==(17 if 'layer-presets-ui' in row['name'] else 24)
assert int((E/'red-status.txt').read_text())!=0 and 'undefined reference' in (E/'expected-red.log').read_text()
m=read('native-mutation.json');assert m['returncode']!=0 and 'GIMP_IS_STRING_ACTION (action)' in m['stderr']+m['stdout']
h=read('header-compile-result.json');assert h['status']=='PASS' and h['compiled_probes']==419
b=read('asan-build.json');assert len(b['instrumented_sources'])==6 and not b['changed_during_build']
with tarfile.open(E/'tested-owner-ui-sources.tar.gz') as tar:
 for row in read('source-manifest.json'):
  assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
  if a.check_source:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
data=(E/'original-dialog-entry-oncanvas-portrait.png').read_bytes();assert struct.unpack('>II',data[16:24])==(1040,1800)
print('PASS: immutable no-op replay; current native4/sanitizer4; UI17+24; wrong descriptor rejected;419 headers; source identities')
