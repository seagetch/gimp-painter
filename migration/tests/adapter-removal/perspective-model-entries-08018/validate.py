#!/usr/bin/env python3
"""Validate the bounded perspective model C operations, not legacy binary layout."""
import argparse, hashlib, json, pathlib, re, tarfile
P=pathlib.Path(__file__).resolve().parent
R=P.parents[3]
a=argparse.ArgumentParser(); a.add_argument('--check-source',action='store_true'); args=a.parse_args()
def load(n): return json.loads((P/n).read_text())
def paths(row): return re.findall(r'^ok \d+ (/\S+)',row['stdout'],re.M)
red=load('expected-red.json'); assert len(red)==1 and red[0]['returncode']!=0
assert "Painter property GimpPerspectiveGuide.id set failed" in red[0]['stdout']+red[0]['stderr']
probes=load('signature-probes.json'); assert len(probes)==2 and all(r['exit_code']==0 for r in probes)
normal=load('normal-results.json'); san=load('sanitizer-results.json')
assert len(normal)==len(san)==2
sets={}
for rows in [normal,san]:
 for row in rows:
  name=row['name'].split(' / ')[-1]; count=34 if name=='painter-perspective' else 24
  assert row['returncode']==0 and len(paths(row))==len(set(paths(row)))==count
  assert not re.search(r'^not ok\b|^ok .*#\s*SKIP|ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',row['stdout']+row.get('stderr',''),re.M|re.I)
  if name in sets: assert sets[name]==set(paths(row))
  sets[name]=set(paths(row))
expected={f'/perspective/factory/stage{s}/caller{c}/kind{k}' for s in range(2) for c in range(2) for k in range(1,5)}
expected|={'/perspective/factory/facade-faults','/perspective/factory/c-operation-contract'}
assert {p for p in sets['painter-perspective'] if '/factory/' in p}==expected
build=load('sanitizer-build.json'); assert not build['changed_during_build']
assert build['leak_detection'] is False
assert len(build['instrumented_sources'])==47 and len(build['rtti_compatibility_only_sources'])==41
for row in san:
 assert row['timeout']==60
 assert row['executable_sha256']==build['executables'][row['name']]['sha256']
assert any('-std=c11' in cmd and any(a.endswith('test-painter-perspective.c') for a in cmd) for cmd in build['commands'])
for prefix in ['normal','sanitizer','expected-red']:
 seal=load(prefix+'-source-sha256.json')
 with tarfile.open(P/(prefix+'-tested-sources.tar.gz')) as t:
  assert set(t.getnames())==set(seal)
  for member in t.getmembers(): assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==seal[member.name]
for name,digest in load('normal-source-sha256.json').items():
 if name in build['sources_sha256']: assert digest==build['sources_sha256'][name],name
 if args.check_source: assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
assert load('source-disposition.json')['legacy_layout_abi_claim'] is False
for name,digest in load('artifact-sha256.json').items(): assert hashlib.sha256((P/name).read_bytes()).hexdigest()==digest,name
print('PASS model C operations and failed-factory admission: normal34+UI24 and bounded sanitizer34+UI24; layout ABI excluded')
