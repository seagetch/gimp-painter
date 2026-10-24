#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,re,subprocess,tempfile,shutil,argparse
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--root',type=pathlib.Path);a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=a.root.resolve()if a.root else E.parents[2];j=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest();count=lambda r:len(re.findall(r'^ok \d+',r['stdout'],re.M))
for n,h in j('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
rows=j('normal-results.json');assert len(rows)==4 and all(r['returncode']==0 for r in rows)and sorted(count(r)for r in rows)==[5,11,18,18]
s=j('asan-results.json');assert s['returncode']==0 and count(s)==5;b=j('asan-build.json');assert len(b['instrumented_sources'])==14 and not b['changed_during_build']
assert {'app/core/core-enums.c','app/core/gimpimage-undo.c','app/paint/gimpmypaintcoreundo.cpp'}<=set(b['instrumented_sources'])
controls=j('mutations.json');assert len(controls)==3
for r in controls:
 expected={'missing-appended-group':'gimp_image_undo_type_is_group','ordinary-action-alias':'GIMP_UNDO_MYPAINT','lost-group-nick':'v->value_nick'}[r['mutation']];assert r['returncode']!=0 and expected in r['stdout']+r['stderr'];assert r['production_sha256']in b['sources_sha256'].values()
g=j('generation.json');assert g['status']=='PASS'and g['generated_sha256']==[b['sources_sha256']['app/core/core-enums.c']]*2 and g['header_sha256']==b['sources_sha256']['app/core/core-enums.h'];assert 'no work to do'in g['noop_stdout'];assert g['appended']=={'GIMP_UNDO_GROUP_MYPAINT':110,'GIMP_UNDO_MYPAINT':111}
v=j('undo-enum-baseline-values.json');assert len(v)==112 and v['GIMP_UNDO_CANT']==109 and v['GIMP_UNDO_GROUP_LAST']==48 and v['GIMP_UNDO_PAINT']==98
with tarfile.open(E/'tested-sources.tar.gz')as t:
 manifest=j('source-manifest.json');assert set(b['sources_sha256'])<={r['path']for r in manifest}
 for r in manifest:
  data=t.extractfile(r['path']).read();assert sha(data)==r['sha256']
  if r['path']in b['sources_sha256']:assert r['sha256']==b['sources_sha256'][r['path']]
  if a.check_source:assert sha((R/r['path']).read_bytes())==r['sha256'],r['path']
 baseline=t.extractfile('app/tests/undo-enum-baseline.inc').read().decode()
 for n,value in v.items():assert f'static_assert({n}=={value},'in baseline
 assert baseline.count('g_enum_get_value_by_name')==110
seal=j('excluded-provider-seal.json');assert len(seal)==10
with tarfile.open(E/'excluded-provider-baselines.tar.gz')as t:
 for row in seal:assert sha(t.extractfile(row['path']).read())==row['tested']
for n,h in j('predecessor-pins.json').items():assert sha((R/n).read_bytes())==h,n
with tempfile.TemporaryDirectory(prefix='undo-enum-predecessor-')as d:
 d=pathlib.Path(d)
 with tarfile.open(E/'enum-generation-predecessor-sources.tar.gz')as t:
  for m in t:
   assert m.isfile()and not pathlib.Path(m.name).is_absolute()and '..'not in pathlib.Path(m.name).parts;p=d/m.name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(t.extractfile(m).read())
 shutil.copytree(R/'migration/tests/enum-generation',d/'migration/tests/enum-generation',dirs_exist_ok=True,ignore=shutil.ignore_patterns('__pycache__'));subprocess.run(['python3',str(d/'tools/check_painter_enum_generation.py')],check=True,capture_output=True,text=True)
assert j('source-gate.json')['status']=='PASS'
assert 'Compiling C object'in(E/'two-object-rebuild.log').read_text()and 'Compiling C++ object'in(E/'two-object-rebuild.log').read_text()
assert (E/'two-objects-before.sha256').read_bytes()==(E/'two-objects-after.sha256').read_bytes()
print('PASS distinct Undo IDs:112 old IDs/110 metadata;native52;14-unit ASan5;3 mutants;generation+noop;immutable predecessor')
