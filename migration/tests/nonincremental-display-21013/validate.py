#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,tarfile,re,struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--root',type=pathlib.Path);a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=a.root or E.parents[2];j=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda r:len(re.findall(r'^ok \d+',r['stdout'],re.M))
for n,h in j('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
s=j('scope.json');assert s['task']=='21.013/live-nonincremental-composition'and s['criterion']=='作業中の見た目と確定結果が整合'and not s['original_closed']and not s['production_changed']and s['stroke_opacity']==.37 and s['display']==[1080,1920]
rows=j('normal-results.json');assert len(rows)==4 and all(r['returncode']==0 for r in rows);ui=next(r for r in rows if r['name'].endswith(' / painter-nonincremental-display'));assert count(ui)==1
for n,c in [('painter-mypaint-legacy-session',129),('painter-mypaint-legacy-rgb-session',385),('painter-mypaint-legacy-gray-session',193)]:assert f'{c} records'in next(r['stdout']for r in rows if r['name'].endswith(' / '+n))
for name,rgb in [('01-before','0,0,255'),('02-live','94,0,161'),('03-cancel','0,0,255'),('04-live-again','94,0,161'),('05-released','94,0,161'),('06-undo','0,0,255'),('07-redo','94,0,161')]:
 assert re.search(r'RAW_CANVAS '+name+r'\.png portrait=1080x1920 oncanvas=1 .*rgb='+rgb,ui['stdout'])
 assert f'composed canvas/sample {name}.png pixel={rgb},255'in ui['stdout']
 b=(E/'screenshots'/(name+'.png')).read_bytes();assert b[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack('>II',b[16:24])==(1080,1920)
b=j('asan-build.json');assert len(b['instrumented_sources'])==6 and not b['changed_during_build'];assert 'app/paint/painter-mypaint-surface/gegl-surface.cpp'in b['instrumented_sources'];assert not b['whole_application_instrumented'];asan=j('asan-results.json');assert len(asan)==1 and asan[0]['returncode']==0 and count(asan[0])==1
manifest=j('source-manifest.json');assert len(manifest)==45 and len({r['path']for r in manifest})==45
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for r in manifest:
  assert sha(t.extractfile(r['path']).read())==r['sha256'],r['path']
  if r['path']in b['sources_sha256']:assert r['sha256']==b['sources_sha256'][r['path']]
  if a.check_source:assert sha((R/r['path']).read_bytes())==r['sha256'],r['path']
 fixture=t.extractfile('app/tests/test-painter-nonincremental-display.cpp').read().decode();assert fixture.index('ProjectionNotify::capture(s,name)')<fixture.index('auto bytes=s.pixels()');assert 'g_assert_true(projected==bytes)'in fixture
mutants=j('mutations.json');assert {r['mutation']for r in mutants}=={'stroke-opacity-ignored','floating-not-composed'}and all(r['returncode']!=0 and ('assertion failed'in r['stderr']+r['stdout']or 'should be TRUE'in r['stderr']+r['stdout'])for r in mutants)
seal=j('excluded-provider-seal.json');assert len(seal)==11
with tarfile.open(E/'excluded-provider-baselines.tar.gz')as t:
 for r in seal:assert sha(t.extractfile(r['path']).read())==r['tested']
for n,h in j('golden-input-pins.json').items():assert sha((R/n).read_bytes())==h,n
assert '1080x1920x24'in(E/'native.log').read_text()
print('PASS bounded nonincremental composition:1 native +6-unit ASan1,2 mutants,3 immutable golden corpora; original21.013 open')
