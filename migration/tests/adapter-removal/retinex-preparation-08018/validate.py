#!/usr/bin/env python3
"""Verify bounded checked Retinex preparation, not the historical void setter."""
import argparse,hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent;R=P.parents[3]
a=argparse.ArgumentParser();a.add_argument('--check-source',action='store_true');a.add_argument('--check-owned-source',action='store_true');args=a.parse_args()
def load(n):return json.loads((P/n).read_text())
def paths(r):return re.findall(r'^ok \d+ (/\S+)',r['stdout'],re.M)
red=load('expected-red.json');assert len(red)==1 and red[0]['returncode']!=0
assert 'Unsupported legacy preparation route; only plug-in-blinds and plug-in-small-tiles are accepted' in red[0]['stdout']+red[0].get('stderr','')
original=load('original-source.json');assert original['commit']=='afa43fae3e920210146abed514f136fd49f671b5'
for row in original['files'].values():assert hashlib.sha256((P/row['artifact']).read_bytes()).hexdigest()==row['sha256']
for filename in ['syntax-tests.json','production-syntax.json']:
 assert all(r['exit_code']==0 for r in load(filename))
normal=load('normal-results.json');san=load('sanitizer-results.json');assert len(normal)==len(san)==1
sets=[]
for row in normal+san:
 assert row['returncode']==0 and len(paths(row))==len(set(paths(row)))==53
 assert not re.search(r'^not ok\b|^ok .*#\s*SKIP|ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',row['stdout']+row.get('stderr',''),re.M|re.I)
 assert row['stdout'].count('Actual old SmallTiles Filter scene ')==50
 assert row['stdout'].count('Actual old Retinex Filter scene ')==12
 sets.append(set(paths(row)))
assert sets[0]==sets[1]
prior=json.loads((P.parent/"small-tiles-preparation-08018/normal-results.json").read_text())
assert set(paths(prior[0])) <= sets[0] and len(paths(prior[0]))==44
assert {p for p in sets[0] if '/small-tiles/' in p}=={'/filter-entry-contracts/small-tiles/'+s for s in ['zero','shapes','owned-conversions','ignored-context','invalid-scalars','missing-provider','actual-old-live']}
assert {'/filter-entry/cpp/small-tiles-handles','/filter-entry/cpp/small-tiles-reentry','/filter-entry/cpp/retinex-handles','/filter-entry/cpp/retinex-reentry'}<=sets[0]
assert {p for p in sets[0] if '/retinex/' in p}=={'/filter-entry-contracts/retinex/'+s for s in ['zero','shapes','owned-conversions','ignored-context','invalid-scalars','missing-provider','actual-old-live']}
build=load('sanitizer-build.json');assert not build['changed_during_build'];assert build['leak_detection'] is False
for path in ['app/core/gimpfilterlayer.cpp','app/core/gimpfilterparametereditor.cpp','app/tests/test-filter-entry-contracts.c','app/tests/test-filter-entry-contracts.cpp']:
 assert path in build['instrumented_sources']
assert san[0]['timeout']==120 and san[0]['duration']<120
assert san[0]['executable_sha256']==build['executables']['filter-entry-contracts']['sha256']
assert any('run_small_tiles_entry_fixture_test.py' in s for s in san[0]['command'])
for prefix in ['normal','sanitizer','expected-red']:
 seal=load(prefix+'-source-sha256.json')
 with tarfile.open(P/(prefix+'-tested-sources.tar.gz')) as t:
  assert set(t.getnames())==set(seal)
  for member in t.getmembers():assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==seal[member.name]
 if prefix!='expected-red':
  for name,digest in seal.items():
   if args.check_owned_source and name in load('owned-source-paths.json'):assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
for name,digest in load('normal-source-sha256.json').items():
 if name in build['sources_sha256']:assert digest==build['sources_sha256'][name],name
 if args.check_source or (args.check_owned_source and name in load('owned-source-paths.json')):assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
registration=load('registration.json');assert registration['timeout']==120 and not registration['is_parallel']
assert any('run_small_tiles_entry_fixture_test.py' in c for c in registration['cmd'])
assert all(r['exit_code']==r['expected_exit'] for r in load('wrapper-controls.json'))
fixture=load('fixture-provenance.json');assert fixture['verified_member_count']==724 and fixture['new_legacy_capture'] is False
assert len(fixture['live_inputs'])==82
assert fixture['newly_executed_live_output_count']==12 and fixture['newly_executed_helper_output_count']==0
assert hashlib.sha256((R/fixture['archive']).read_bytes()).hexdigest()==fixture['archive_sha256']
allowed={'GIMP_TESTING_ABS_TOP_BUILDDIR','GIMP_TESTING_ABS_TOP_SRCDIR','GIMP_TESTING_PLUGINDIRS','LD_LIBRARY_PATH','UI_TEST','ASAN_OPTIONS','UBSAN_OPTIONS'}
def no_inherited_env(obj):
 if isinstance(obj,list):
  for value in obj:no_inherited_env(value)
 elif isinstance(obj,dict):
  if isinstance(obj.get('env'),dict):assert set(obj['env'])<=allowed
  for value in obj.values():no_inherited_env(value)
for file in P.glob('*.json'):no_inherited_env(json.loads(file.read_text()))
for name,digest in load('artifact-sha256.json').items():assert hashlib.sha256((P/name).read_bytes()).hexdigest()==digest,name
print('PASS checked Retinex preparation: native53 and bounded sanitizer53; original Retinex6 scenes/12 outputs plus SmallTiles25/50; historical void setter and general procedure admission remain open')
