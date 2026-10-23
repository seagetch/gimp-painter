#!/usr/bin/env python3
"""Original GtkWindow popup factory proof; not paused renderer/decorator integration."""
import argparse,hashlib,json,pathlib,re,tarfile,struct
P=pathlib.Path(__file__).resolve().parent;R=P.parents[3]
a=argparse.ArgumentParser();a.add_argument('--check-owned-source',action='store_true');a.add_argument('--check-source',action='store_true');args=a.parse_args()
def load(n):return json.loads((P/n).read_text())
def h(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def paths(r):return re.findall(r'^ok \d+ (/\S+)',r['stdout'],re.M)
red=load('expected-red.json');assert len(red)==1 and red[0]['returncode']!=0
assert "'gimp_popover_new' should not be NULL" in red[0]['stdout']+red[0].get('stderr','')
original=load('original-source.json');assert original['commit']=='afa43fae3e920210146abed514f136fd49f671b5'
for row in original['files'].values():assert h(P/row['artifact'])==row['sha256']
exclusion=load('paused-source-exclusion.json');assert exclusion['commit']=='513bec9490'
assert len(exclusion['source_sha256'])==5
with tarfile.open(P/'paused-exclusion-sources.tar.gz') as t:
 assert set(t.getnames())==set(exclusion['source_sha256'])
 for member in t.getmembers():assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==exclusion['source_sha256'][member.name]
normal=load('normal-results.json');san=load('sanitizer-results.json');assert len(normal)==len(san)==1
expected={'/native-popover/'+n for n in ['original-factory','invalid-views','mapped-grabs','signal-actions','signal-veto','key-bindings','button-contract','button-targets','map-reentry','cancel-reentry','insertion-reentry','typed-handles','factory-faults','actual-ui']}
for row in normal+san:
 assert row['returncode']==0 and len(paths(row))==14 and set(paths(row))==expected
 assert not re.search(r'^not ok\b|^ok .*#\s*SKIP|ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',row['stdout']+row.get('stderr',''),re.M|re.I)
build=load('sanitizer-build.json');assert not build['changed_during_build'] and build['leak_detection'] is False
for p in ['app/widgets/popupper.cpp','app/tests/test-native-popover-entry.c','app/tests/test-native-popover-entry.cpp']:assert p in build['instrumented_sources']
assert san[0]['timeout']==60 and san[0]['duration']<60
assert san[0]['executable_sha256']==build['executables']['native-popover-entry']['sha256']
for prefix in ['expected-red','normal','sanitizer','header']:
 seal=load(prefix+'-source-sha256.json')
 with tarfile.open(P/(prefix+'-tested-sources.tar.gz')) as t:
  assert set(t.getnames())==set(seal)
  for member in t.getmembers():assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==seal[member.name]
 if prefix!='expected-red':
  for name,digest in seal.items():
   if args.check_source or (args.check_owned_source and name in load('owned-source-paths.json')):assert h(R/name)==digest,name
for name,digest in load('normal-source-sha256.json').items():
 if name in build['sources_sha256']:assert digest==build['sources_sha256'][name]
assert all(r['exit_code']==0 for r in load('syntax-tests.json'))
for row in load('header-first-double.json'):
 assert row['exit_code']==0 and '-Werror' in row['command']
 assert ('-std=c11' if row['language']=='c' else '-std=c++14') in row['command']
 if row['language']=='cpp':assert '-fno-rtti' in row['command']
headers=load('header-probes.json');name='app/widgets/popupper.h'
assert {p['language'] for p in headers['probes'] if p.get('header')==name}=={'c','cpp'}
assert headers['source_sha256'][name]==load('normal-source-sha256.json')[name]
assert len(load('header-results.json'))==2 and all(r['returncode']==0 for r in load('header-results.json'))
registration=load('registration.json');assert registration['timeout']==60 and not registration['is_parallel']
for name in ['portrait-oncanvas.png','portrait-oncanvas-sanitizer.png']:
 data=(P/name).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack('>II',data[16:24])==(1080,1920)
for name in ['green-portrait.log','sanitizer-portrait.log']:assert '1080x1920x24' in (P/name).read_text()
allowed={'GIMP_TESTING_ABS_TOP_BUILDDIR','GIMP_TESTING_ABS_TOP_SRCDIR','GIMP_TESTING_PLUGINDIRS','LD_LIBRARY_PATH','UI_TEST','ASAN_OPTIONS','UBSAN_OPTIONS','GIMP_TEST_POPOVER_SCREENSHOT'}
def no_inherited_env(obj):
 if isinstance(obj,list):
  for value in obj:no_inherited_env(value)
 elif isinstance(obj,dict):
  if isinstance(obj.get('env'),dict):assert set(obj['env'])<=allowed
  for value in obj.values():no_inherited_env(value)
for file in P.glob('*.json'):no_inherited_env(json.loads(file.read_text()))
for name,digest in load('artifact-sha256.json').items():assert h(P/name)==digest,name
print('PASS original GtkWindow popover factory: native14/sanitizer14, first/double headers, actual portrait oncanvas popup; old Interface/decorator integration remains separate')
