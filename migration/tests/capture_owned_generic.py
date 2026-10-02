#!/usr/bin/env python3
"""Compile unchanged pinned legacy generic entrypoints; capture independent family oracles.
Source /workspace/shared/gimp-legacy-build/env.sh and hold the shared build lock.
No source/archive replacement or inherited-environment serialization.
"""
import gzip,hashlib,json,os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];old=root.parent/'gimp-painter-legacy';out=root/'migration/fixtures/legacy-owned-generic'
commit='afa43fae3e920210146abed514f136fd49f671b5'
sources=['app/paint/gimppaintcore-stroke.c','app/paint/gimpsmudge.c','app/tools/gimpbucketfillbrushtool.cpp']
for source in sources:assert (old/source).read_bytes()==subprocess.check_output(['git','-C',str(old),'show',commit+':'+source])
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True))
cflags=Path(os.environ['LEGACY_BUILD_ROOT'],'gimp-cflags.txt').read_text().strip()
plan=subprocess.check_output(['make','-n','-W','test-xcf.o','test-xcf','CFLAGS='+cflags,'CXXFLAGS=-g -O2 -include type_traits','LIBS=../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0'],cwd=old/'app/tests',text=True)
line=next(x for x in plan.splitlines() if '--mode=link' in x);line=line[line.index('/bin/bash'):]
report={'commit':commit,'scope':'Real legacy generic Stroke, Vectors and Boundary, two disconnected image-space subpaths; Fill 12 RGB/selection/rate/erase and Smudge 48 Y/YA/RGB/RGBA/blending/rate/dynamics scenes per route; old compatibility environment unchanged; no GUI claim','old_sources_sha256':{p:hashlib.sha256((old/p).read_bytes()).hexdigest() for p in sources},'runs':[]}
report['legacy_archives_sha256']={name:hashlib.sha256((old/name).read_bytes()).hexdigest() for name in ['app/paint/libapppaint.a','app/core/libappcore.a','app/tools/libapptools.a']}
with tempfile.TemporaryDirectory(prefix='owned-generic-oracle-') as tmp:
 for family,prefix,records in [('fill-brush',b'BRUSH ',37),('smudge',b'SMUDGE ',193)]:
  work=Path(tmp);obj=work/(family+'.o');binary=work/family
  command=[os.environ.get('CC','cc'),'-std=gnu99','-g','-O2','-I'+str(old),'-I'+str(old/'app'),'-I'+str(old/'app/tests'),'-I'+str(root/'app/tests'),*flags,'-c',str(out/('capture-'+family+'.c')),'-o',str(obj)]
  build=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/(family+'-build.log')).write_bytes(build.stdout);build.check_returncode()
  link=line.replace('-o test-xcf test-xcf.o',f'-o {shlex.quote(str(binary))} {shlex.quote(str(obj))}')
  build=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  with (out/(family+'-build.log')).open('ab') as f:f.write(build.stdout)
  build.check_returncode()
  for route in ['raw','path','boundary']:
   run=subprocess.run([str(binary)],env={**os.environ,'PAINTER_OWNED_GENERIC_ROUTE':route},stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
   normalized=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(prefix) or x.endswith(b'_CAPTURE_COMPLETE'))+b'\n'
   assert run.returncode==0,(family,route,run.stderr.decode());assert len(normalized.splitlines())==records
   with gzip.GzipFile(out/(family+'-'+route+'.tsv.gz'),'wb',mtime=0) as f:f.write(normalized)
   (out/(family+'-'+route+'.stderr')).write_bytes(run.stderr)
   report['runs'].append({'family':family,'route':route,'exit_code':run.returncode,'records':records,'bytes':len(normalized),'sha256':hashlib.sha256(normalized).hexdigest(),'binary_sha256':hashlib.sha256((work/'.libs'/family).read_bytes()).hexdigest()})
report['capture_sources_sha256']={str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [out/'capture-fill-brush.c',out/'capture-smudge.c',root/'app/tests/painter-owned-stroke-trace.h']}
(out/'runtime.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['runs'],indent=2))
