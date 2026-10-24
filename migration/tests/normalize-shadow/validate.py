#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,tarfile,subprocess,sys
E=Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
manifest=json.loads((E/'artifact-sha256.json').read_text());assert set(manifest)=={str(p.relative_to(E))for p in E.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts}
for p,h in manifest.items():assert sha((E/p).read_bytes())==h,p
r=json.loads((E/'results.json').read_text());assert not r['changed_after_run']
expected=json.loads((E/'all-inputs.json').read_text());actual={};contents={}
with tarfile.open(E/'sources.tar.gz')as t:
 for f in t:
  assert f.isfile()and not Path(f.name).is_absolute()and '..'not in Path(f.name).parts and f.name not in actual;contents[f.name]=t.extractfile(f).read();actual[f.name]=sha(contents[f.name])
assert actual==expected
for p,h in r['source_sha256'].items():assert actual[p]==h,p
owned=['app/painter/filter-point-shadow.cpp','app/painter/filter-point-shadow.hpp','app/painter/tests/test-filter-normalize-shadow.cpp']
if '--check-source'in sys.argv:
 for p in owned:assert sha((R/p).read_bytes())==actual[p],p
for mode,row in r['runs'].items():
 assert row['exit_code']==0 and 'Normalize 13 ROI/vector/alias/spill/snapshot/domain scenes; 120 two-pass/zero-fill/flush fault points preserve publication'in row['stdout']
 assert 'Value Invert exact pinned C callback: all 16,777,216 RGB triples'in row['stdout']
 assert '-std=c++14'in row['compile']and '-fno-rtti'in row['compile']
assert '-fsanitize=address,undefined,float-cast-overflow'in r['runs']['sanitizer']['compile']and '-fsanitize=address,undefined,float-cast-overflow'in r['runs']['sanitizer']['oracle_compile']
assert len([a for a in r['runs']['sanitizer']['compile']if a.endswith('.cpp')])==6
m=json.loads((E/'mutations.json').read_text());assert len(m)==5
for row in m:assert row['exit_code']!=0 and row['stderr']
reg=json.loads((E/'regressions.json').read_text());assert set(reg)=={'color-exchange','colorify','video'}
for row in reg.values():assert row['exit_code']==0 and row['fixture_sha256']==actual[row['fixture']] and any(v.endswith('filter-normalize-raster.cpp')for v in row['command'])
h=json.loads((E/'headers.json').read_text());assert h['exit_code']==0 and h['translation_unit'].count('#include "filter-point-shadow.hpp"')==2 and h['translation_unit'].count('#include "filter-normalize-raster.hpp"')==2
base=json.loads((E/'baseline.json').read_text())
for p,info in base['files'].items():
 original=subprocess.check_output(['git','show',base['commit']+':'+p],cwd=R);assert sha(original)==info['sha256'];current=contents[p].decode()
 if p.endswith('.cpp'):
  current=current.replace('#include "filter-normalize-raster.hpp"\n','');start=current.index('FilterPointShadowOutcome filter_normalize_shadow_raster(');end=current.index('} // namespace GimpPainter',start);current=current[:start]+current[end:]
 else:
  start=current.index('/* Normalize uses the sole CropRaster');end=current.index('} // namespace GimpPainter',start);current=current[:start]+current[end:]
 assert current.encode()==original,'unrelated backend changed '+p
for name in ['normalize-raster','video-shadow']:
 for p,h in json.loads((E/(name+'-predecessor.json')).read_text()).items():assert sha((R/p).read_bytes())==h,p
 subprocess.run([sys.executable,'-B',str(E.parent/name/'validate.py')],check=True)
print('PASS Normalize soleCropRaster13 scenes/120 fault points normal+7-unit sanitizer;5 mutants;3 current route regressions; old backends unchanged')
