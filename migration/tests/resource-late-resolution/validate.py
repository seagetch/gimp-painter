#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,re,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();e=pathlib.Path(__file__).resolve().parent;r=e.parents[2]
read=lambda n:json.loads((e/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
for f,h in read('artifact-sha256.json').items():assert sha((e/f).read_bytes())==h,f
x=read('results.json');assert x['provider_base']=='20b2204b11' and not x['linked_inputs_changed']
assert len(x['linked_inputs_sha256'])==1322 and len(x['runtime_dependency_sha256'])==127
rows={z['name']:z for z in x['results']}
for mode,count in [('normal',1),('asan',8),('mutant-brush',2),('mutant-paper',2)]:
 compiles=[z for z in x['results']if z['name'].startswith(mode+'-compile-')];assert len(compiles)==count
 for z in compiles:assert z['returncode']==0
 assert rows[mode+'-link']['returncode']==0
 z=rows[mode+'-run']
 if mode in ['normal','asan']:assert z['returncode']==0 and len(re.findall(r'^ok \d+ ',z['stdout'],re.M))==2
 else:assert z['returncode']!=0 and ('effective_brush' if mode=='mutant-brush' else 'effective_texture')in z['stderr']
seal=read('clean-provider-seal.json');assert len(seal['nine_held_paths_match_committed_bytes'])==9 and seal['selected_provider_and_test_sources_unchanged']
assert read('filter-dependency-source-before.json')['sha256']==read('filter-dependency-source-after.json')['sha256']
with tarfile.open(e/'tested-provider-sources.tar.gz')as t:
 for f,h in x['source_sha256'].items():
  assert sha(t.extractfile(f).read())==h,f
  if a.check_source:assert sha((r/f).read_bytes())==h,f
print('PASS native2/scoped8-unit sanitizer2;2 causal lookup mutants;late-resolution/retention/flags;1322linked+127runtime unchanged;held9 excluded;not Preview/UI/fullstroke/wholeHEAD')
