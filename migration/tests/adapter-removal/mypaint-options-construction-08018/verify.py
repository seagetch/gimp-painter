#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[3];sha=lambda b:hashlib.sha256(b).hexdigest();read=lambda p:json.loads((E/p).read_text())
m=read('source-manifest.json')
with tarfile.open(E/'tested-inputs.tar.gz') as t:
 for path,h in m['input_sha256'].items():assert sha(t.extractfile(path).read())==h,path
if a.check_owned_source:
 for path,h in m['owned_source_sha256'].items():assert sha((R/path).read_bytes())==h,path
assert len(m['held_providers'])==8
for row in [read('normal.json')[0],read('asan-results.json')]:
 assert row['returncode']==0
 assert re.search(r'^1\.\.9$',row['stdout'],re.M)
 assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==9
 assert not re.search(r'AddressSanitizer|runtime error:',row['stdout']+(row['stderr'] or ''))
b=read('asan-build.json');assert len(b['instrumented_sources'])==4;assert not b['changed_during_build']
for path,h in b['sources_sha256'].items():assert h==m['input_sha256'][path]
assert read('post-test-inputs.json')['all_recorded_inputs_unchanged'] is True
for name in ['wrong-name','publish-failed']:
 row=read('mutant-'+name+'.json');assert row['returncode']!=0
 assert 'assertion failed' in row['stdout']+row['stderr'] or 'should be nullptr' in row['stdout']+row['stderr']
for name in ['normal-portrait.log','asan-portrait.log']:
 assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/name).read_text()
r=read('report.json');assert r['normal_cases']==r['sanitizer_cases']==9;assert r['rendered_ui_claim'] is False
for path,h in read('evidence-manifest.json').items():assert sha((E/path).read_bytes())==h,path
print('PASS: original options construction9 + selected sanitizer9; two compiled mutants; selected-source seals')
