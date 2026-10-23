#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,tarfile,re
P=pathlib.Path(__file__).resolve().parent;R=P.parents[3];a=argparse.ArgumentParser();a.add_argument('--check-owned-source',action='store_true');args=a.parse_args()
def load(n):return json.loads((P/n).read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
expected={'/tool-registration/'+n for n in ['transfer','cpp-transfer','invalid','duplicate','observer-removes','observer-registers','observer-detaches','observer-last-owner','native-catalog','native-standard-removed']}
for name in ['normal-results.json','sanitizer-results.json']:
 rows=load(name);assert len(rows)==1
 for r in rows:
  assert r['returncode']==0 and set(re.findall(r'^ok \d+ (/\S+)',r['stdout'],re.M))==expected
  assert not re.search(r'^not ok|#\s*SKIP|ERROR: AddressSanitizer|runtime error:',r['stdout']+r.get('stderr',''),re.M)
assert load('expected-red.json')[0]['returncode']!=0
b=load('sanitizer-build.json');assert len(b['instrumented_sources'])==12 and not b['changed_during_build'] and not b['leak_detection'] and not b['vptr_sanitizer']
for prefix in ['normal','sanitizer','header']:
 seal=load(prefix+'-source-sha256.json')
 with tarfile.open(P/(prefix+'-tested-sources.tar.gz')) as t:
  assert set(t.getnames())==set(seal)
  for m in t.getmembers():assert hashlib.sha256(t.extractfile(m).read()).hexdigest()==seal[m.name]
 if args.check_owned_source:
  for n,h in seal.items():
   if n in load('owned-source-paths.json'):assert sha(R/n)==h,n
for n,h in b['sources_sha256'].items():assert load('normal-source-sha256.json')[n]==h
mutants=load('native-mutations.json');assert {m['mutation'] for m in mutants}=={'retain-transferred-reference','omit-native-registration'}
for m in mutants:assert m['returncode']!=0 and m['production_sha256']==load('normal-source-sha256.json')['app/tools/gimp-tools.c']
for n in ['native-catalog.tsv','sanitizer-native-catalog.tsv']:assert len((P/n).read_text().splitlines())==50
assert all(r['returncode']==0 for r in load('header-results.json'))
for r in load('header-first-double.json'):assert r['exit_code']==0 and '-Werror' in r['command']
assert {r['language'] for r in load('header-probes.json')['probes'] if r.get('header')=='app/tools/gimp-tools.h'}=={'c','cpp'}
with tarfile.open(P/'paused-exclusion-sources.tar.gz') as t:
 rows=load('paused-source-exclusion.json');assert len(rows)==5
 for r in rows:assert hashlib.sha256(t.extractfile(r['path']).read()).hexdigest()==r['sha256'] and r['baseline']=='77bcdf2b7f'
for n in ['green-portrait.log','sanitizer-portrait.log']:assert '1080x1920x24' in (P/n).read_text()
for p in P.rglob('*.json'):
 def check(x):
  if isinstance(x,dict):
   assert 'env' not in x
   for v in x.values():check(v)
  if isinstance(x,list):
   for v in x:check(v)
 check(json.loads(p.read_text()))
for p,h in load('artifact-sha256.json').items():assert sha(P/p)==h,p
print('PASS original consuming tool registration native10/selected sanitizer10, native catalog50, two compiled mutants, strict headers')
