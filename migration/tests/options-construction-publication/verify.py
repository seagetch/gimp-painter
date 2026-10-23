#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();read=lambda p:json.loads((E/p).read_text())
for phase in ['red','green']:
 m=read(phase+'/source-manifest.json')
 with tarfile.open(E/phase/'tested-inputs.tar.gz') as t:
  for path,h in m['input_sha256'].items():assert sha(t.extractfile(path).read())==h,path
 assert len(m['held_providers'])==8
m=read('green/source-manifest.json')
if a.check_owned_source:
 for path,h in m['owned_source_sha256'].items():assert sha((R/path).read_bytes())==h,path
red=read('red/normal.json');assert red[0]['returncode']==0;assert red[1]['returncode']!=0
assert 'ok 1 /options-construction-reentry/info-lease-finalizes-app' in red[1]['stdout']
assert 'ok 2 /options-construction-reentry/app-lease-releases-info' in red[1]['stdout']
assert 'ok 3 /options-construction-reentry/native-selection-closes-app' in red[1]['stdout']
assert "'options' should be nullptr" in red[1]['stdout']
assert 'last-info-lease-disposes-options' in red[1]['stdout']
for rows in [read('green/normal.json'),read('green/asan-results.json')]:
 assert len(rows)==2
 for row,count in zip(rows,[9,4]):
  assert row['returncode']==0
  assert re.search(r'^1\.\.'+str(count)+'$',row['stdout'],re.M)
  assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==count
  assert not re.search(r'AddressSanitizer|runtime error:',row['stdout']+(row['stderr'] or ''))
b=read('green/asan-build.json');assert len(b['instrumented_sources'])==5;assert not b['changed_during_build']
for path,h in b['sources_sha256'].items():assert h==m['input_sha256'][path]
assert read('green/post-test-inputs.json')['all_recorded_inputs_unchanged'] is True
for name in ['normal-portrait.log','asan-portrait.log']:
 assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/'green'/name).read_text()
for path,h in read('evidence-manifest.json').items():assert sha((E/path).read_bytes())==h,path
print('PASS: final constructor normal13 + selected sanitizer13; last-info-lease options disposal red/green; selected source seals')
