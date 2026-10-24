#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,tarfile,re,subprocess,sys,struct
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--root',type=pathlib.Path);a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=a.root.resolve() if a.root else E.parents[2];read=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda r:len(re.findall(r'^ok \d+',r['stdout'],re.M))
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
rows=read('normal-results.json');assert len(rows)==5 and all(r['returncode']==0 for r in rows) and sorted(count(r)for r in rows)==[8,14,18,24,27]
rows=read('asan-results.json');assert len(rows)==2 and sorted(count(r)for r in rows)==[8,14] and all(r['returncode']==0 for r in rows)
b=read('asan-build.json');assert len(b['instrumented_sources'])==23 and not b['changed_during_build'];assert {'app/paint/gimppaintcore.c','app/core/gimpdrawable.c','app/core/gimpitem.c','app/core/gimpviewable.c','app/core/gimplayer.c'}<=set(b['instrumented_sources'])
mutants=read('mutations.json');assert len(mutants)==6
for r in mutants:
 assert r['returncode']!=0
 expected='gegl_buffer_get_extent' if r['mutation']=='missing-null-guard' else 'gimp_tool_control_activate' if r['mutation']=='no-tool-retirement' else 'should be TRUE'
 assert expected in r['stdout']+r['stderr']
 provider='app/tools/gimppaintermybrushtool.cpp' if r['mutation']=='no-tool-retirement' else 'app/paint/painter-mypaint-surface/paint-core.cpp'
 assert r['production_sha256']==b['sources_sha256'][provider]
red=read('prestart-red.json');assert red['returncode']!=0 and "'rejected' should be TRUE" in red['stdout']+red['stderr']
order=read('setup-order.json');assert list(order['ordered_sites'].values())==sorted(order['ordered_sites'].values()) and len(order['ordered_sites'])==5
assert order['source_sha256']==b['sources_sha256']['app/paint/painter-mypaint-surface/paint-core.cpp']
for archive in ['prior-frozen-package.tar.gz','prior-13-plus-8-package.tar.gz']:
 with tarfile.open(E/archive) as old:
  pins=json.loads(old.extractfile('migration/tests/paint-geometry-conflict/artifact-sha256.json').read())
  for n,h in pins.items():assert sha(old.extractfile('migration/tests/paint-geometry-conflict/'+n).read())==h,n
rows=read('controller-results.json');assert len(rows)==8 and all(r['returncode']==0 for r in rows)
for r in rows:
 if r['kind'] in ['grow','shrink','offset']:
  lines=[x for x in r['stdout'].splitlines() if 'phase=cancel event=update' in x];assert lines and 'state_before=1' in lines[0] and 'pixels_before=0' in lines[0]
 if r['kind']=='offset':assert 'pixels_before=0' in next(x for x in r['stdout'].splitlines() if 'event=after-cancel' in x)
rows=read('tool-results.json');assert len(rows)==5 and all(r['returncode']==0 for r in rows)
for r in rows:
 if r['kind'] in ['grow','shrink','translate']:assert 'pending=0 baseline_pixels=1 depth=1' in next(x for x in r['stdout'].splitlines() if 'event=after-external' in x)
assert (E/'gegl-version.txt').read_text().strip()=='0.4.62'
rows=read('headers.json');assert len(rows)==2 and [r['includes']for r in rows]==[1,2] and all(r['returncode']==0 and '-std=c++14'in r['command']and '-fno-rtti'in r['command']for r in rows)
g=read('source-gate-current-policy.json');assert g['status']=='PASS'and not g['errors']
assert set(b['sources_sha256'])<={r['path']for r in read('source-manifest.json')}
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for r in read('source-manifest.json'):
  n=r['path'];archived=t.extractfile(n).read();assert sha(archived)==r['sha256']
  if 'source_mode'in r:assert re.fullmatch('[0-9a-f]{40}',r['baseline_commit']) and hashlib.sha1(b'blob '+str(len(archived)).encode()+b'\0'+archived).hexdigest()==r['baseline_blob']
  if n in b['sources_sha256']:assert r['sha256']==b['sources_sha256'][n]
  if a.check_source:
   if 'source_mode'in r:
    assert n=='app/core/gimpviewable.c';assert subprocess.check_output(['git','rev-parse',r['baseline_commit']+':'+n],cwd=R,text=True).strip()==r['baseline_blob'];data=subprocess.check_output(['git','show',r['baseline_commit']+':'+n],cwd=R)
   else:data=(R/n).read_bytes()
   assert sha(data)==r['sha256'],n
seal=read('held-source-seal.json');assert len(seal)==9
with tarfile.open(E/'held-baseline-sources.tar.gz')as t:
 for r in seal:assert sha(t.extractfile(r['path']).read())==r['tested']
for n in ['01-conflict-preserved.png','02-next-gesture-recovered.png']:
 data=(E/'screenshots'/n).read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n';assert struct.unpack('>II',data[16:24])==(1040,1800)
assert '1080x1920x24'in (E/'capture.log').read_text() and 'ok 1 /geometry/tool/offset'in(E/'capture.log').read_text()
for n,h in read('predecessor-pins.json').items():assert sha((R/n).read_bytes())==h,n
for d in ['paired-stroke-09011','paired-save-integration','paired-ui-default-directory']:subprocess.run([sys.executable,str(R/'migration/tests'/d/'validate.py')],check=True,capture_output=True,text=True)
print('PASS terminal geometry conflicts: native14+Tool8+69 regressions;23-unit ASan22;6 mutants;2 actual portrait screenshots; immutable predecessors; explicit committed held dependency')
