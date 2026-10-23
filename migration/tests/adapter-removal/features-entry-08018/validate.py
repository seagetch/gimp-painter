#!/usr/bin/env python3
import argparse,json,pathlib,hashlib,re,tarfile,struct,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[3];read=lambda n:json.loads((E/n).read_text())
subprocess.run([sys.executable,str(E.parent/'preset-prefs-entry-08018/validate.py')],check=True)
for file in ['normal-results.json','asan-results.json']:
 rows=read(file);assert len(rows)==2
 for row in rows:
  assert row['returncode']==0
  assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==(11 if 'feature-lifecycle' in row['name'] else 6)
counts={'layer-presets-ui':18,'painter-perspective-ui':24,'painter-http':18,'painter-http-ui':6}
rows=read('regression-results.json');assert len(rows)==4
for row in rows:
 name=row['name'].split(' / ')[-1];assert row['returncode']==0
 assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==counts[name]
mutations=read('native-mutations.json');assert len(mutations)==2
for row in mutations:
 assert row['returncode']!=0
 assert ('provider_calls' if row['mutation']=='duplicate-restore' else 'painter_httpd') in row['stderr']+row['stdout']
h=read('header-compile-result.json');assert h['status']=='PASS' and h['compiled_probes']==421
b=read('asan-build.json');assert len(b['instrumented_sources'])==14 and not b['changed_during_build']
assert not read('pre-extraction-symbol-audit.json')['original_entry_present']
assert 'GNU Image Manipulation Program version' in (E/'console-version.log').read_text()
assert 'httpd' not in read('disabled-compile.json')['undefined_symbols']
with tarfile.open(E/'tested-owner-ui-sources.tar.gz') as tar:
 for row in read('source-manifest.json'):
  assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
  if a.check_source:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
data=(E/'feature-prefs-oncanvas-portrait.png').read_bytes();assert struct.unpack('>II',data[16:24])==(1080,1920)
print('PASS: immutable prefs/dialog/no-op replay; native11+6/sanitizer11+6; UI18+24/HTTP18+6;2 mutants;421 headers;console/disabled-unit;source identities')
