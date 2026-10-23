#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile,subprocess,sys,struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda x:len(re.findall(r'^ok \d+',x['stdout'],re.M))
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
rows=read('normal-results.json');assert len(rows)==2 and sorted(count(x) for x in rows)==[17,24] and all(x['returncode']==0 for x in rows)
x=read('asan-results.json');assert x['returncode']==0 and count(x)==17
build=read('asan-build.json');assert len(build['instrumented_sources'])==13 and not build['changed_during_build']
rows=read('regression-results.json');assert len(rows)==4 and sorted(count(x) for x in rows)==[11,18,18,24] and all(x['returncode']==0 for x in rows)
rows=read('engine-results.json');assert len(rows)==6 and all(x['returncode']==0 for x in rows)
for x in rows:
 if x['name'].endswith('historical-install'):assert count(x)==6
 elif x['name'].endswith('prepared-state'):assert count(x)==4
 elif x['name'].endswith(('checkpoint','resource')):assert count(x)==9
 elif x['name'].endswith('legacy-engine'):assert '5637911 bytes, 33768 records' in x['stdout']
 else:assert '1125707 bytes, 81420 records' in x['stdout']
rows=read('native-mutations.json');assert len(rows)==5
for x in rows:assert x['returncode']!=0 and ('should be TRUE' in x['stdout']+x['stderr'] or 'should be FALSE' in x['stdout']+x['stderr'] or 'assertion failed' in x['stdout']+x['stderr']) and x['production_sha256']==build['sources_sha256']['app/paint/painter-mypaint-surface/paint-core.cpp']
x=read('failure-observer-red.json');assert x['returncode']!=0 and 'state_expected' in x['stderr'];assert x['source_sha256']['app/paint/painter-mypaint-surface/paint-core.cpp']==sha((E/'rollback-before.cpp').read_bytes())
rows=[json.loads(line) for line in (E/'cancel-observer-red.json').read_text().splitlines() if line.strip()];assert any(x['returncode']!=0 and 'state_expected' in x['stdout']+x['stderr'] for x in rows)
x=read('header.json');assert x['returncode']==0 and '-fno-rtti' in x['command'] and '-std=c++14' in x['command']
rows=read('ui-results.json');assert len(rows)==1 and rows[0]['returncode']==0 and count(rows[0])==1
rectangles=re.findall(r'rendered image rectangle ([^:]+): (\d+,\d+ \d+x\d+)',rows[0]['stdout']);assert len(rectangles)==4 and len(set(x[1] for x in rectangles))==1
assert '1080x1920x24' in (E/'ui-tests.log').read_text()
for n in ['00-baseline.png','01-painted.png','02-undo.png','03-redo.png']:
 b=(E/'screenshots'/n).read_bytes();assert b[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack('>II',b[16:24])==(1040,1800)
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for row in read('source-manifest.json'):
  n=row['path'];assert sha(t.extractfile(n).read())==row['sha256'],n
  if n in build['sources_sha256']:assert row['sha256']==build['sources_sha256'][n],n
  if a.check_source:assert sha((R/n).read_bytes())==row['sha256'],n
 fixture=t.extractfile('app/tests/test-paired-stroke-ui.cpp').read().decode();assert 'visible_after!=visible_before' in fixture and '==visible_before' in fixture and '==visible_after' in fixture and 'gimp_painter_canvas_ui_get_visible' in fixture
 production=t.extractfile('app/paint/painter-mypaint-surface/paint-core.cpp').read().decode();assert '131072' not in production and 'resource_before->storage_bytes()' in production
seal=read('held-source-seal.json');assert len(seal)==8
with tarfile.open(E/'held-baseline-sources.tar.gz')as t:
 for x in seal:assert sha(t.extractfile(x['path']).read())==x['tested']
audit=read('opaque-memory-audit.json');assert audit['opaque_allocations']=={'GRand':2500,'JsonNode':32,'JsonObject':56,'JsonArray':24};assert audit['allowances']['JSON_per_node_including_container_capacity_slack']==512
for n,h in read('predecessor-pins.json').items():assert sha((R/n).read_bytes())==h,n
for n in ['native-stroke-undo-09011/verify.py','prepared-pixels-09011/verify.py','engine-historical-09011/validate.py']:
 subprocess.run([sys.executable,str(R/'migration/tests'/n)],check=True,capture_output=True,text=True)
print('PASS: bounded fixed-bounds paired17/ASan17; Surface24;5mutants; rollback observers/memory; legacy traces; rendered UI1/4PNGs; source and immutable predecessor proofs; expanded geometry parent remains open')
