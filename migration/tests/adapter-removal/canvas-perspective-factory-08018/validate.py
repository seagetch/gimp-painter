#!/usr/bin/env python3
"""Validate bounded checked-native canvas factory; never silently pass the UI failure."""
import argparse,hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent
R=P.parents[3]
a=argparse.ArgumentParser();a.add_argument('--check-source',action='store_true');args=a.parse_args()
def load(n):return json.loads((P/n).read_text())
def paths(s):return re.findall(r'^ok \d+ (/\S+)',s,re.M)
def clean(s):
 assert not re.search(r'^not ok\b|^ok .*#\s*SKIP|ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',s,re.M|re.I)
expected={f'/painter-canvas-factory/stage{s}/caller{c}/kind{k}' for s in range(3)for c in range(2)for k in range(1,5)}
expected|={'/painter-canvas-factory/typed-handle','/painter-canvas-factory/shell-init-failure','/painter-canvas-factory/c-signature-and-rejection'}
red=load('expected-red.json');assert len(red)==1 and red[0]['returncode']!=0
assert "'result' should be NULL" in red[0]['stdout']+red[0]['stderr']
normal=load('normal-first-results.json')
f=next(x for x in normal if x['name'].endswith('/ painter-canvas-factory'))
assert f['returncode']==0 and set(paths(f['stdout']))==expected
clean(f['stdout']+f['stderr'])
u=next(x for x in normal if x['name'].endswith('/ painter-perspective-ui'))
assert u['returncode']!=0 and 'gimp_tool_group_child_add' in u['stdout']+u['stderr']
assert len(paths(u['stdout']))==7 # retained historical failure, NOT aggregate acceptance
san=load('sanitizer-results.json');assert len(san)==1
assert san[0]['name']=='painter-canvas-factory' and san[0]['returncode']==0
assert san[0]['timeout']==60 and set(paths(san[0]['stdout']))==expected
clean(san[0]['stdout'])
build=load('sanitizer-build.json');assert not build['changed_during_build']
assert build['leak_detection'] is False
assert build['executables']['painter-canvas-factory']['sha256']==san[0]['executable_sha256']
normal_seal=load('normal-source-checkpoint.json')['source_sha256']
with tarfile.open(P/'normal-tested-sources.tar.gz')as t:
 assert set(t.getnames())==set(normal_seal)
 for member in t.getmembers():assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==normal_seal[member.name]
final_normal=load('final/normal-results.json')
final_san=load('final/sanitizer-results.json')
assert len(final_normal)==len(final_san)==2
for rows in [final_normal,final_san]:
 for row in rows:
  count=27 if row['name'].endswith('painter-canvas-factory') else 24
  assert row['returncode']==0 and len(paths(row['stdout']))==count
  if count==27:assert set(paths(row['stdout']))==expected
  clean(row['stdout']+row.get('stderr',''))
final_build=load('final/sanitizer-build.json')
assert len(final_build['instrumented_sources'])==47
assert len(final_build['rtti_compatibility_only_sources'])==41
assert not final_build['changed_during_build']
for row in final_san:
 assert row['timeout']==60
 assert row['executable_sha256']==final_build['executables'][row['name']]['sha256']
final_seal=load('final/tested-source-sha256.json')
with tarfile.open(P/'final/tested-sources.tar.gz')as t:
 assert set(t.getnames())==set(final_seal)
 for member in t.getmembers():assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==final_seal[member.name]
if args.check_source:
 for name,digest in final_seal.items():
  assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
binding=load('final/tool-reset-binding.json')
for name,digest in binding['receipt_sha256'].items():
 assert hashlib.sha256((R/binding['receipt_directory']/name).read_bytes()).hexdigest()==digest,name
for row in load('syntax-checks.json'):assert row['exit_code']==0 and not row['stderr']
assert 'PASS' in (P/'header-check.log').read_text()
assert load('source-disposition.json')['parent_complete'] is False
for name,digest in load('artifact-sha256.json').items():
 assert hashlib.sha256((P/name).read_bytes()).hexdigest()==digest,name
print('PASS factory27 + repaired full UI24 normal/union ASan; historical failure retained; canonical legacy API PARTIAL')
