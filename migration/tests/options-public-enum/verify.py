#!/usr/bin/env python3
"""Verify immutable public-enum successor evidence, not whole-application parity."""
import argparse, hashlib, json, pathlib, re, tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
read=lambda p:json.loads((E/p).read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
m=read('integration/source-overlay.json');assert len(m['held_exact_base'])==12
assert m['base']=='20b2204b11df079c306a680b41ac623f3be75c76'
with tarfile.open(E/'integration/tested-inputs.tar.gz') as t:
 for path,h in {**m['owned_sha256'],**m['shared_sha256']}.items():assert sha(t.extractfile(path).read())==h,path
 enum=t.extractfile('libgimpbase/gimpbaseenums.c').read().decode();opt=t.extractfile('app/paint/painter-mypaint-surface/gimp-painter-options.cpp').read().decode();compat=t.extractfile('app/paint/gimpmypaintoptions.h').read().decode()
 assert enum.count('g_enum_register_static (\"GimpMypaintBrushMode\"')==1 and 'legacy_brush_mode_type' not in opt and 'GimpMypaintBrushMode;' not in compat
 assert 'GIMP_TYPE_MYPAINT_BRUSH_MODE' in opt
if a.check_owned_source:
 for path,h in m['owned_sha256'].items():assert sha((R/path).read_bytes())==h,path
objects=read('integration/no-op-objects.json');assert objects['all_unchanged'] and objects['compile_link_no_op'] and len(objects['objects_libraries_binaries_sha256'])>1000
noop=read('integration/no-op-source.json');assert noop['source_unchanged'] and noop['compile_link_no_op']
assert not re.search(r'Compiling |Linking |Generating.*(enum|pdb)',(E/'integration/no-op-build.log').read_text())
post=read('integration/source-after.json');assert post['all_source_content_unchanged'] and post['generated_enum_matches_private_generator']
assert post['generated_enum_sha256']==m['owned_sha256']['libgimpbase/gimpbaseenums.c']
normal=read('integration/normal.json');assert len(normal)==4
for row in normal:assert row['returncode']==0 and row['result']=='OK',row['name']
for key,count in [('mypaint-enum-public',2),('mypaint-options-mode',6),('gimp-painter-options',24)]:
 row=next(r for r in normal if r['name'].endswith(key));assert re.search(r'^1\.\.'+str(count)+'$',row['stdout'],re.M)
assert ' T gimp_mypaint_brush_mode_get_type' in (E/'integration/base-symbol.txt').read_text()
assert ' U gimp_mypaint_brush_mode_get_type' in (E/'integration/libgimp-symbol.txt').read_text()
h=read('integration/headers.json');assert h['status']=='PASS' and h['routes']>=42 and h['probes']>=88
b=read('sanitizer/build.json');assert b['sanitized'] and not b['whole_application_instrumented'] and not b['vptr_instrumented'] and not b['leak_detection']
assert len([p for p in b['owned_sources'] if p.endswith(('.c','.cpp'))])==7
with tarfile.open(E/'sanitizer/tested-inputs.tar.gz') as t:
 for path,h in b['owned_sources'].items():assert sha(t.extractfile(path).read())==h,path
for path,h in m['owned_sha256'].items():
 if path in b['owned_sources']:assert b['owned_sources'][path]==h,path
assert b['dependency_objects_unchanged'] and read('sanitizer/post-inputs.json')['dependency_objects_unchanged']
rows=read('sanitizer/results.json');assert len(rows)==3
for row in rows:
 assert row['returncode']==0
 assert not re.search(r'ERROR: AddressSanitizer|runtime error:',row['stdout']+row['stderr'])
for key,count in [('mypaint-enum-public',2),('mypaint-options-mode',6)]:
 row=next(r for r in rows if r['name']==key);assert re.search(r'^1\.\.'+str(count)+'$',row['stdout'],re.M)
for path in ['integration/normal-portrait.log','sanitizer/run.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected' in (E/path).read_text()
g=read('generator/reproducibility.json');assert g['exact_original_registrar_body'] and g['private_generation_runs']==2 and not g['unrelated_enum_churn'];assert g['generated_sha256']==post['generated_enum_sha256']
pg=read('generator/pdb-reproducibility.json');assert len(pg['runs'])==2 and pg['runs'][0]==pg['runs'][1] and pg['unrelated_header_unchanged']
with tarfile.open(E/'generator/pdb-inputs.tar.gz') as t:
 for path,h in pg['input_sha256'].items():assert sha(t.extractfile(path).read())==h,path
for path in ['pdb/enums.pl','libgimp/gimpenums.c.tail']:assert pg['runs'][0][path]==m['owned_sha256'][path]
for path,h in read('evidence-manifest.json').items():assert sha((E/path).read_bytes())==h,path
print('PASS: sole public enum registration, real Meson library/app linkage; C11 standalone + C++14 two orders + mode6; bounded sanitizer successor')
