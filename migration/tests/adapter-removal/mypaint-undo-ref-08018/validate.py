#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,argparse,re,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[3];read=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda x:len(re.findall(r'^ok \d+',x['stdout'],re.M))
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
rows=read('normal-results.json');assert len(rows)==1 and rows[0]['returncode']==0 and count(rows[0])==3
x=read('asan-results.json');assert x['returncode']==0 and count(x)==3
b=read('asan-build.json');assert len(b['instrumented_sources'])==7 and not b['changed_during_build']
rows=read('regression-results.json');assert len(rows)==3 and sorted(count(x)for x in rows)==[16,17,24] and all(x['returncode']==0 for x in rows)
rows=read('headers.json');assert len(rows)==4
for x in rows:
 assert (x['returncode']!=0)==x['baseline']
 if x['language']=='cpp':assert '-fno-rtti' in x['command'] and '-std=c++14' in x['command']
 else:assert '-std=c11' in x['command']
 if x['baseline']:assert ('MypaintCoreUndoRef' if x['language']=='cpp' else 'GimpMypaintCoreUndo') in x['stderr']
rows=read('mutations.json');assert {x['name']for x in rows}=={'missing-retain','wrong-native-type'}
for x in rows:assert x['returncode']!=0 and x['header_sha256']==b['sources_sha256']['app/paint/gimpmypaintcoreundo.hpp']
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for x in read('source-manifest.json'):
  assert sha(t.extractfile(x['path']).read())==x['sha256']
  if x['path']in b['sources_sha256']:assert x['sha256']==b['sources_sha256'][x['path']]
  if a.check_source:assert sha((R/x['path']).read_bytes())==x['sha256'],x['path']
 assert 'class MypaintCoreUndoRef' in t.extractfile('app/paint/gimpmypaintcoreundo.hpp').read().decode()
 assert 'TypeTraits<GimpMypaintCoreUndo>' not in t.extractfile('app/paint/gimpmypaintcoreundo.cpp').read().decode()
for n,h in read('predecessor-pins.json').items():assert sha((R/n).read_bytes())==h,n
for path in ['native-stroke-undo-09011/verify.py','paired-stroke-09011/validate.py']:subprocess.run([sys.executable,str(R/'migration/tests'/path)],check=True,capture_output=True,text=True)
assert len(read('held-source-seal.json'))==8
print('PASS bounded Undo source/typed ownership:3native/ASan3,7units,2mutants,C11/C++14 red-green; immutable bridge/paired predecessors; native regression16/17/24')
