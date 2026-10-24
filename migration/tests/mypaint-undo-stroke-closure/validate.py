#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,re,subprocess,argparse,csv
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--root',type=pathlib.Path);a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=a.root.resolve()if a.root else E.parents[2];j=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda r:len(re.findall(r'^ok \d+',r['stdout'],re.M))
for n,h in j('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
rows=j('normal-results.json');assert len(rows)==1 and rows[0]['returncode']==0 and count(rows[0])==16
s=j('asan-results.json');assert s['returncode']==0 and count(s)==16;b=j('asan-build.json');assert len(b['instrumented_sources'])==9 and not b['changed_during_build']
accept=j('acceptance.json');assert accept['task']=='09.011/mypaint-undo-stroke'and accept['criterion']=='旧 pop の空処理を引き継がず Undo/Redo で画素と stroke 状態が往復し、pointer を一度だけ破棄';assert '21.014'in accept['not_closed']
rows=list(csv.reader((E/'original-eight-duties.tsv').open(),delimiter='\t'));expected={'01.002/'+x for x in ['000134','000135','000137','000138','000142','000143','000939','000940']};assert len(rows)==8 and {r[2]for r in rows}==expected and set(accept['hunk_ids'])==expected
original=list(csv.reader((R/'migration/inventory/hunk-wbs.tsv').open(),delimiter='\t'));assert all(r in original for r in rows)
with tarfile.open(E/'tested-sources.tar.gz')as t:
 manifest=j('source-manifest.json');assert set(b['sources_sha256'])<={r['path']for r in manifest}
 for r in manifest:
  data=t.extractfile(r['path']).read();assert sha(data)==r['sha256']
  if r['path']in b['sources_sha256']:assert r['sha256']==b['sources_sha256'][r['path']]
  if a.check_source:assert sha((R/r['path']).read_bytes())==r['sha256'],r['path']
 assert '"undo-type",GIMP_UNDO_MYPAINT'in t.extractfile('app/paint/gimpmypaintcoreundo.cpp').read().decode()
seal=j('excluded-provider-seal.json');assert len(seal)==10
with tarfile.open(E/'excluded-provider-baselines.tar.gz')as t:
 for row in seal:assert sha(t.extractfile(row['path']).read())==row['tested']
for n,h in j('predecessor-pins.json').items():assert sha((R/n).read_bytes())==h,n
for d,v in j('predecessor-validators.json').items():subprocess.run(['python3',str(R/'migration/tests'/d/v)],check=True,capture_output=True,text=True)
assert '1080x1920x24'in(E/'native.log').read_text()
print('PASS original8 Undo duties: current bridge16/9-unit ASan16; immutable paired/geometry/enum/facade proofs;21.014 not closed')
