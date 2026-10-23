#!/usr/bin/env python3
"""Verify bounded point/precision refresh without promoting the generic parent."""
import argparse, hashlib, json, pathlib, re
P=pathlib.Path(__file__).resolve().parent
ROOT=P.parents[2]
parser=argparse.ArgumentParser();parser.add_argument('--check-source',action='store_true');args=parser.parse_args()
def load(p):
 a=json.loads((P/p).read_text())
 if isinstance(a,dict) and 'log_sha256' in a:
  log=P/p.replace('-resources.json','.log')
  if log.exists():
   assert hashlib.sha256(log.read_bytes()).hexdigest()==a['log_sha256'],str(log)
   a['stdout']=log.read_text()
 return a
def tap(text):return re.findall(r'^ok \d+ (/\S+)',text,re.M)
def success(a,count):
 assert a['exit_code']==0
 assert not a.get('sources_changed',[])
 assert not a.get('changed_after_compile',[])
 assert not a.get('changed_after_run',[])
 assert not a.get('skipped_lines',[])
 assert not a.get('sanitizer_diagnostics',[])
 if 'executed_paths'in a:
  assert len(a['executed_paths'])==count
  assert sorted(a['executed_paths'])==sorted(a['selected_test_paths'])
  assert a['passed']
 else:
  assert len(tap(a['stdout']))==count
  assert not re.search(r'^not ok\b|^ok .*#\s*SKIP',a['stdout'],re.M|re.I)
  assert not re.search(r'ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',a.get('stderr',''))
for path,count in [('normal-resources.json',1),('core-normal-resources.json',9),('final/asan-resources.json',1),('final/large-asan-direct-resources.json',1),('final/core-asan-resources.json',9),('final/kernels-sanitizers.json',6),('final/scheduler-sanitizers.json',39)]:success(load(path),count)
build=load('final/large-asan-build-and-test.json');success(build,1)
assert len(build['instrumented_sources'])==20 and len(build['rtti_compatibility_only_sources'])==50
assert build['leak_detection'] is False
assert load('final/core-asan-resources.json')['watchdog_seconds']==180
normal=load('independent-normal.json');assert len(normal)==2
for a,n in zip(normal,[6,39]):success(a,n)
fixture=ROOT/'migration/fixtures/legacy-filter-points/fixtures.tsv'
rows=[line.split()for line in fixture.read_text().splitlines()if line and not line.startswith('#')]
assert len(rows)==40 and len({r[0]for r in rows})==3
for r in rows:
 for name in r[-2:]:assert (fixture.parent/name).is_file()
if args.check_source:
 for path in ['final/large-asan-build-and-test.json','final/kernels-sanitizers.json','final/scheduler-sanitizers.json']:
  for name,digest in load(path)['source_sha256'].items():
   assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
ui=load('ui-bound-acceptance.json')
assert ui['normal']['returncode']==ui['sanitizer']['returncode']==0
assert ui['sanitizer']['passed']==70 and ui['sources_unchanged_since_normal']
for name in ui['required_paths']:
 assert name in tap(ui['normal']['stdout']) and name in tap(ui['sanitizer_stdout']),name
for name,digest in ui['receipt_sha256'].items():
 assert hashlib.sha256((ROOT/ui['receipt_directory']/name).read_bytes()).hexdigest()==digest,name
if args.check_source:
 for name,digest in ui['source_sha256'].items():
  assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
accepted=load('acceptance.json')
assert accepted['status']=='PASS_BOUNDED_REFRESH'
for name,digest in accepted['fixture_sha256'].items():
 assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
for name,digest in load('artifact-sha256.json').items():
 assert hashlib.sha256((P/name).read_bytes()).hexdigest()==digest,name
print('PASS: unchanged8193-square,9point/native/spill cases,6kernels/39scheduler;20-unit bounded sanitizer, not general108-route acceptance')
