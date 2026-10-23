#!/usr/bin/env python3
"""Validate bounded JSON-owner receipts; optionally require current owner sources."""
import argparse,hashlib,json,pathlib,tarfile,re
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[3]
read=lambda n:json.loads((E/n).read_text())
normal=read('normal-results.json');san=read('asan-results.json')
assert len(normal)==len(san)==2
for rows in [normal,san]:
 for row in rows:
  assert row['returncode']==0,row['name']
  assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==(7 if 'json-resource' in row['name'] else 12),row['name']
  assert not re.search(r'ERROR: AddressSanitizer|runtime error:|SUMMARY: AddressSanitizer',row['stderr'])
manifest=read('source-manifest.json');build=read('asan-build.json')
assert len(build['instrumented_sources'])==9 and not build['changed_during_build']
assert not build['vptr_sanitizer'] and not build['leak_detection'] and not build['dependencies_instrumented']
with tarfile.open(E/'tested-owner-sources.tar.gz') as tar:
 assert set(tar.getnames())=={x['path'] for x in manifest}
 for row in manifest:
  assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
  if a.check_source:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
h=read('header-compile-result.json');assert h['status']=='PASS' and h['compiled_probes']==417
assert read('facade-header.json')['returncode']==0
c=read('source-contract.json');assert c['result']=='PASS' and len(c['negative_controls'])==55
assert sum(x['id'].startswith('config:json-resource:') for x in c['negative_controls'])==9
print('PASS: normal19 + selected sanitizer19; 417 headers; isolated source gate55 controls; archive/source identities')
