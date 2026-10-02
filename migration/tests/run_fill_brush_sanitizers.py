#!/usr/bin/env python3
"""Focused native Fill transaction + exact old stroke tests; shared build lock required."""
import argparse,gzip,hashlib,json,os,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args();root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'fill-brush-sanitizers';out.mkdir(exist_ok=True)
common={'app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp'}
sources=common|{'app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/paint/painter-mypaint-surface/gimp-painter-session.cpp','app/paint/gimpfillbrush.cpp','app/paint/painter-bounded-fill/search.cpp','app/paint/gimppaintcore.c','app/paint/gimpbrushcore.c','app/paint/gimppaintcore-loops.cc','app/core/gimpimage-undo.c','app/core/gimpimage.c','app/core/gimpitem.c','app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c','app/operations/layer-modes/gimp-layer-modes.c','app/tests/test-gimp-fill-brush.cpp','app/tests/painter-fill-brush-trace.cpp','app/tests/painter-brush-interpolation-trace.cpp'}
headers={'app/paint/gimpfillbrush.h','app/paint/gimppaintcore.h','app/paint/gimpbrushcore.h','app/core/gimpimage.h','app/paint/painter-bounded-fill/search.hpp','app/painter/binding-store.hpp','app/painter/object-ref.hpp'}
hashes={x:hashlib.sha256((root/x).read_bytes()).hexdigest() for x in sources|headers};entries=json.loads((build/'compile_commands.json').read_text());objects={};originals={};commands=[];flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1']
for source in sorted(sources):
 e=next(e for e in entries if (Path(e['directory'])/e['file']).resolve()==root/source);cmd=shlex.split(e['command']);clean=[];skip=False
 for arg in cmd:
  if skip:skip=False;continue
  if arg in ['-MF','-MQ','-MT']:skip=True;continue
  if arg in ['-MD','-MMD']:continue
  clean.append(arg)
 obj=out/(Path(source).name+'.o');clean[clean.index('-o')+1]=str(obj);clean+=flags
 if source.endswith(('.cpp','.cc')):clean+=['-frtti']
 commands.append(clean);subprocess.run(clean,cwd=build,check=True);objects[source]=str(obj);originals[e['output']]=source
archives=['app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a','app/paint/libapppaint.a','app/paint/painter-bounded-fill/libpainter-bounded-fill.a','app/painter/libapppainter.a','app/core/libappcore.a','app/operations/layer-modes-legacy/libapplayermodeslegacy.a','app/operations/layer-modes/libapplayermodes.a'];replaced={}
for archive in archives:
 members=subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines();result=[]
 for member in members:
  source=originals.get(member)
  if source in common:continue
  result.append(objects[source] if source else str((build/member).resolve()))
 dest=out/Path(archive).name
 if dest.exists():dest.unlink()
 cmd=['ar','crsT',str(dest),*result];commands.append(cmd);subprocess.run(cmd,cwd=build,check=True);replaced[archive]=str(dest)
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GIMP3_DIRECTORY':str(build/'app/tests/gimpdir-output'),'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'};runs=[]
for target in ['gimp-fill-brush','painter-fill-brush-trace','painter-brush-interpolation-trace']:
 cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/target;cmd[cmd.index('-o')+1]=str(exe);cmd=[objects[originals[x]] if x in originals and originals[x].startswith('app/tests/') else replaced.get(x,x) for x in cmd];cmd[1:1]=[*flags,*[objects[x] for x in sorted(common)]];commands.append(cmd);subprocess.run(cmd,cwd=build,check=True)
 for extra in ([[],['queued'],['async']] if target=='painter-fill-brush-trace' else [['0'],['1'],['2'],['7'],['31'],['4096']] if target=='painter-brush-interpolation-trace' else [[]]):
  run=subprocess.run([str(exe),*extra],cwd=build,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE);record={'test':target,'args':extra,'exit_code':run.returncode,'stderr':run.stderr.decode()}
  if target=='painter-brush-interpolation-trace' and run.returncode==0:
   normalized=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'INTERP '))+b'\n';expected=gzip.decompress((root/'migration/fixtures/native-brush-interpolation/trace.tsv.gz').read_bytes());assert normalized==expected
   record['stdout']=f'Exact pre-refactor native interpolation match: {len(expected)} bytes, 8883 records'
  elif target=='painter-fill-brush-trace' and run.returncode==0:
   normalized=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'BRUSH ') or x==b'FILL_BRUSH_CAPTURE_COMPLETE')+b'\n';expected=gzip.decompress((root/'migration/fixtures/legacy-fill-brush/pixels.tsv.gz').read_bytes());assert normalized==expected
   record['stdout']=f'Exact native old stroke/Undo match: {len(expected)} bytes, 37 records'
  else:record['stdout']=run.stdout.decode()
  runs.append(record)
  if run.returncode:break
changed=[x for x,h in hashes.items() if hashlib.sha256((root/x).read_bytes()).hexdigest()!=h]
a.report.write_text(json.dumps({'scope':'18 focused units (including registered MyPaint option/session RTTI owners): Fill search/controller, native paint/Undo, common bridge and mode dispatch; remaining host/dependencies uninstrumented; LSan disabled','commands':commands,'sources_sha256':hashes,'changed_during_run':changed,'runs':runs},indent=2)+'\n');print([(r['test'],r['args'],r['exit_code']) for r in runs]);assert not changed;assert all(r['exit_code']==0 for r in runs)
