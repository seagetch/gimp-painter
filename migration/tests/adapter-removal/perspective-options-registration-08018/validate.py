#!/usr/bin/env python3
"""Bounded restored options identity; no historical registration/layout equivalence."""
import argparse,hashlib,json,pathlib,re,tarfile
P=pathlib.Path(__file__).resolve().parent;R=P.parents[3]
a=argparse.ArgumentParser();a.add_argument('--check-source',action='store_true');a.add_argument('--check-owned-source',action='store_true');args=a.parse_args()
def load(n):return json.loads((P/n).read_text())
def paths(row):return re.findall(r'^ok \d+ (/\S+)',row['stdout'],re.M)
red=load('expected-red.json');assert len(red)==1 and red[0]['returncode']!=0
assert '("GimpToolOptions" == "GimpPerspectiveGuideOptions")' in red[0]['stdout']+red[0].get('stderr','')
original=load('original-source.json');assert original['commit']=='afa43fae3e920210146abed514f136fd49f671b5'
old=(P/'original-tool-source.cpp.txt').read_bytes();assert hashlib.sha256(old).hexdigest()==original['sha256']
assert b'GType   gimp_perspective_guide_options_get_type (void)' in old
assert original['legacy_layout_abi_claim'] is False
probes=load('signature-probes.json');assert {x['language'] for x in probes}=={'c','cpp'}
for q in probes:
 assert q['exit_code']==0 and '-Werror' in q['command']
 suffix='c' if q['language']=='c' else 'cpp'
 assert hashlib.sha256((P/('signature-probe.'+suffix)).read_bytes()).hexdigest()==q['source_sha256']
normal=load('normal-results.json');san=load('sanitizer-results.json');assert len(normal)==len(san)==2
sets={}
for rows in [normal,san]:
 for row in rows:
  name=row['name'].split(' / ')[-1];count=39 if name=='painter-perspective' else 24
  assert row['returncode']==0 and len(paths(row))==len(set(paths(row)))==count
  assert not re.search(r'^not ok\b|^ok .*#\s*SKIP|ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',row['stdout']+row.get('stderr',''),re.M|re.I)
  if name in sets:assert sets[name]==set(paths(row))
  sets[name]=set(paths(row))
assert {p for p in sets['painter-perspective'] if '/options/' in p}=={'/perspective/options/'+p for p in ['registered-type','inherited-properties','config-roundtrip','registration-callback','typed-handles']}
build=load('sanitizer-build.json');assert not build['changed_during_build'];assert build['leak_detection'] is False
for path in ['app/tools/gimpperspectiveguidetool.cpp','app/core/gimptooloptions.c','app/core/gimpcontext.c','app/tools/gimptool.c']:
 assert path in build['instrumented_sources']
for row in san:
 assert row['timeout']==60 and row['duration']<60
 assert row['executable_sha256']==build['executables'][row['name']]['sha256']
for prefix in ['normal','sanitizer','expected-red']:
 seal=load(prefix+'-source-sha256.json')
 with tarfile.open(P/(prefix+'-tested-sources.tar.gz')) as t:
  assert set(t.getnames())==set(seal)
  for member in t.getmembers():assert hashlib.sha256(t.extractfile(member).read()).hexdigest()==seal[member.name]
for name,digest in load('normal-source-sha256.json').items():
 if name in build['sources_sha256']:assert digest==build['sources_sha256'][name],name
 if args.check_source or (args.check_owned_source and name in load('owned-source-paths.json')):assert hashlib.sha256((R/name).read_bytes()).hexdigest()==digest,name
headers=load('header-results.json');assert len(headers)==2 and all(r['returncode']==0 for r in headers)
for name,digest in load('artifact-sha256.json').items():assert hashlib.sha256((P/name).read_bytes()).hexdigest()==digest,name
print('PASS restored native options identity: normal39+UI24 and bounded sanitizer39+UI24; historical registration/layout equivalence excluded')
