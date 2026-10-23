#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,tarfile,re
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];a=argparse.ArgumentParser();a.add_argument('--check-owned-source',action='store_true');args=a.parse_args()
def load(n):return json.loads((P/n).read_text())
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
expected={'/tool-options-retirement/'+n for n in ['remove','thaw','readd','live-bogus-rejected','retired-deserialization-rejected','device-snapshot-any-tool']}
for n in ['normal-results.json','asan-results.json']:
 rows=load(n);assert len(rows)==1
 for r in rows:
  assert r['returncode']==0 and set(re.findall(r'^ok \d+ (/\S+)',r['stdout'],re.M))==expected
  assert not re.search(r'^not ok|#\s*SKIP|ERROR: AddressSanitizer|runtime error:',r['stdout']+r.get('stderr',''),re.M)
red=load('expected-red-results.json');assert red[0]['returncode']!=0 and 'gimp_tool_options_check_tool_info' in red[0]['stdout']+red[0].get('stderr','')
b=load('asan-build.json');assert len(b['instrumented_sources'])==10 and not b['changed_during_build'] and not b['whole_application_instrumented'] and not b['leak_detection']
assert load('asan-results.json')[0]['executable_sha256']==b['executables']['tool-options-retirement']['sha256']
seal=load('source-sha256.json')
with tarfile.open(P/'tested-sources.tar.gz') as t:
 assert set(t.getnames())==set(seal)
 for m in t.getmembers():assert hashlib.sha256(t.extractfile(m).read()).hexdigest()==seal[m.name]
for p,h in b['sources_sha256'].items():assert seal[p]==h
if args.check_owned_source:
 for p in ['app/core/gimptooloptions.c','app/tests/test-tool-options-retirement.c','migration/tests/build_tool_options_retirement_sanitizers.py']:assert sha(R/p)==seal[p],p
mutants=load('native-mutations.json');assert {m['mutation'] for m in mutants}=={'forget-retired-identity','force-unbound-fallback','accept-malformed-property'}
for m in mutants:assert m['returncode']!=0 and m['production_sha256']==seal['app/core/gimptooloptions.c'] and m['mutant_sha256']!=m['production_sha256']
assert len(load('paused-source-exclusion.json'))==5
for n in ['green-portrait.log','asan-portrait.log']:assert '1080x1920x24' in (P/n).read_text()
for p in P.glob('*.json'):
 def check(x):
  if isinstance(x,dict):
   assert 'env' not in x
   for v in x.values():check(v)
  if isinstance(x,list):
   for v in x:check(v)
 check(json.loads(p.read_text()))
for p,h in load('artifact-sha256.json').items():assert sha(P/p)==h,p
print('PASS retained/unbound options native6 + bounded sanitizer6; three real mutants, strict diagnostics retained')
