#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,re,argparse,struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--root',type=pathlib.Path);a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=a.root.resolve()if a.root else E.parents[2];j=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda r:len(re.findall(r'^ok \d+',r['stdout'],re.M))
for n,h in j('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
scope=j('scope.json');assert scope['task']=='21.014/ordinary-physical-gesture'and scope['original_task']=='21.014'and scope['original_criterion']=='一筆一 Undo で開始前へ戻る';assert scope['original_closed']is False and scope['ordinary_undo_entries']==1 and scope['save_then_continuation_entries']==2 and scope['save_decision'].startswith('pending user choice')
expected={'painter-physical-gesture':7,'painter-physical-tool':8,'painter-mypaint-tool':27,'gimp-painter-surface':24,'gimp-painter-session':10,'gimp-painter-batch':13,'paired-stroke-undo':18,'paint-geometry-conflict':14,'paint-geometry-tool':8,'painter-mypaint-legacy-session':0,'painter-mypaint-legacy-rgb-session':0,'painter-mypaint-legacy-gray-session':0}
rows=j('normal-results.json');assert len(rows)==12 and {r['name'].split(' / ')[-1]for r in rows}==set(expected)
for r in rows:assert r['returncode']==0 and count(r)==expected[r['name'].split(' / ')[-1]]
for n,records in [('painter-mypaint-legacy-session',129),('painter-mypaint-legacy-rgb-session',385),('painter-mypaint-legacy-gray-session',193)]:assert f'{records} records'in next(r['stdout']for r in rows if r['name'].endswith(' / '+n))
b=j('asan-build.json');assert len(b['instrumented_sources'])==20 and not b['changed_during_build'];assert {'app/paint/painter-mypaint-surface/paint-core.cpp','app/tools/gimppaintermybrushtool.cpp','app/paint/painter-mypaint-surface/gimp-painter-session.cpp'}<=set(b['instrumented_sources']);rows=j('asan-results.json');assert len(rows)==2 and sorted(count(r)for r in rows)==[7,8]and all(r['returncode']==0 for r in rows)
headers=j('headers.json');assert len(headers)==2 and {r['header']for r in headers}=={'paint-core.hpp','gimp-painter-session.hpp'}and all(r['returncode']==0 and '-fno-rtti'in r['command']and '-fsyntax-only'in r['command']for r in headers)
manifest=j('source-manifest.json');assert len(manifest)==62 and len({r['path']for r in manifest})==62 and set(b['sources_sha256'])<={r['path']for r in manifest}
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for r in manifest:
  data=t.extractfile(r['path']).read();assert sha(data)==r['sha256'],r['path']
  if r['path']in b['sources_sha256']:assert r['sha256']==b['sources_sha256'][r['path']]
  if a.check_source:assert sha((R/r['path']).read_bytes())==r['sha256'],r['path']
 core=t.extractfile('app/paint/painter-mypaint-surface/paint-core.cpp').read().decode();assert 'void begin_gesture()'in core and 'if(batch&&!gesture)'in core
 facade=t.extractfile('app/paint/painter-mypaint-surface/gimp-painter-session.hpp').read().decode();assert 'extern "C"'not in facade and 'void begin_gesture() const;'in facade
rows=j('mutations.json');assert len(rows)==4 and {r['mutation']for r in rows}=={'logical-split-publishes','press-resets-warmed-evaluator','cancel-commits-gesture','release-does-not-seal'}
for r in rows:assert r['returncode']!=0 and ('assertion failed'in r['stdout']+r['stderr']or 'should be TRUE'in r['stdout']+r['stderr'])and r['production_sha256']in b['sources_sha256'].values()
old=(E/'baseline-traces.log').read_text();new=(E/'candidate-traces.log').read_text();get=lambda s:re.findall(r'^# PIX 0 .*$',s,re.M);assert len(get(old))==360 and get(old)==get(new)
for floating in [0,1]:
 oldhash=re.search(rf'^# FINAL 0 {floating} ([a-f0-9]{{64}}) depth=2$',old,re.M);newhash=re.search(rf'^# FINAL 0 {floating} ([a-f0-9]{{64}}) depth=1$',new,re.M);assert oldhash and newhash and oldhash[1]==newhash[1]
save=(E/'save-baseline/current.log').read_text();assert 'REAL_SAVE_XCF_REOPEN bytes_match=1'in save and 'SAVE_AFTER active=1 pending=0 depth=2'in save and 'HELD_MOTION changed=1 active=1'in save
stack=(E/'settings-halt-stack.txt').read_text();assert all(s in stack for s in ['clear_input','cancel_observed','image_dirty','gimp_image_undo_publish_prepared','PaintCore::configure'])
for n,h in j('golden-input-pins.json').items():assert sha((R/n).read_bytes())==h,n
seal=j('excluded-provider-seal.json');assert len(seal)==10
with tarfile.open(E/'excluded-provider-baselines.tar.gz')as t:
 for r in seal:assert sha(t.extractfile(r['path']).read())==r['tested']
assert '1080x1920x24'in(E/'native.log').read_text()
for name in ['01-before.png','02-released.png','03-undo.png','04-redo.png']:
 data=(E/'screenshots'/name).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack('>II',data[16:24])==(1040,1800)
print('PASS bounded ordinary gesture:129 native cases,20-unit ASan15,4 mutants,3 old golden corpora; Save decision and original21.014 remain open')
