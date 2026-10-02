#!/usr/bin/env python3
"""Focused owned native Stroke/Path/Boundary sanitizers. Hold the shared build lock.
Build private archives, replacing every flattened alias; leave production untouched.
"""
import argparse,gzip,hashlib,json,os,shlex,subprocess,tarfile
from pathlib import Path
from painter_sanitizer_scope import bridge_rtti_sources,CXX_SUFFIXES
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'owned-generic-sanitizers';out.mkdir(exist_ok=True)
instrumented=set('''app/painter/binding-store.cpp app/painter/gimp-painter-binding.cpp app/painter/gimp-painter-error.cpp
app/paint/gimpfillbrush.cpp app/paint/gimppaintersmudge.cpp app/paint/gimppaintcore-stroke.c app/paint/gimppaintcore.c app/paint/gimpbrushcore.c app/paint/gimpbrushcore-loops.cc app/paint/gimppaintcore-loops.cc app/paint/painter-bounded-fill/search.cpp
app/core/gimpimage.c app/core/gimpimage-undo.c app/core/gimpitem.c app/core/gimpdrawable.c app/core/gimpdata.c app/core/gimpbrush.c app/core/gimpcontext.c
app/vectors/gimppath.c app/vectors/gimpstroke.c app/vectors/gimpbezierstroke.c app/core/gimpboundary.c app/core/gimpcoords-interpolate.c
app/tests/test-gimp-fill-brush.cpp app/tests/test-gimp-painter-smudge.cpp app/tests/painter-fill-brush-trace.cpp app/tests/painter-smudge-trace.cpp'''.split())
headers=set('''app/paint/painter-native-stroking.hpp app/paint/gimpfillbrush.h app/paint/gimppaintersmudge.h app/paint/gimppaintcore-stroke.h app/paint/gimppaintcore.h app/paint/gimpbrushcore.h app/paint/gimppaintoptions.h app/core/gimpimage.h app/core/gimpbrush.h app/core/gimpbrush-private.h app/painter/binding-store.hpp app/painter/object-ref.hpp app/painter/connection.hpp app/paint/painter-smudge/legacy-pixels.hpp app/paint/painter-bounded-fill/search.hpp app/tests/test-painter-owned-strokes.inc app/tests/painter-owned-stroke-trace.h'''.split())
rtti=bridge_rtti_sources(root,build)-instrumented;wanted=instrumented|rtti
hashes={x:hashlib.sha256((root/x).read_bytes()).hexdigest() for x in wanted|headers}
report={'scope':'Owned Fill/independent Smudge actual native generic entrypoints, transaction, callbacks, native interpolation and old generic pixel comparison; remaining host/dependencies not instrumented','sanitizers':['address','undefined','float-cast-overflow'],'leak_detection':False,'instrumented_sources':sorted(instrumented),'rtti_compatibility_only_sources':sorted(rtti),'commands':[],'runs':[]}
entries=json.loads((build/'compile_commands.json').read_text());replacements={};compiled=set();flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1']
for e in entries:
 source=(Path(e['directory'])/e['file']).resolve()
 if not source.is_relative_to(root):continue
 relative=source.relative_to(root).as_posix()
 if relative not in wanted:continue
 cmd=shlex.split(e['command']);clean=[];skip=False
 for arg in cmd:
  if skip:skip=False;continue
  if arg in ['-MF','-MQ','-MT']:skip=True;continue
  if arg in ['-MD','-MMD']:continue
  clean.append(arg)
 obj=out/(relative.replace('/','_')+'.o');clean[clean.index('-o')+1]=str(obj)
 if relative in instrumented:clean+=flags
 if source.suffix in CXX_SUFFIXES:clean+=['-frtti']
 report['commands'].append(clean);subprocess.run(clean,cwd=build,check=True);compiled.add(relative);replacements[str((build/e['output']).resolve())]=str(obj)
assert compiled==wanted,(wanted-compiled)
private={}
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GIMP3_DIRECTORY':str(build/'app/tests/gimpdir-output'),'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
for target in ['gimp-fill-brush','gimp-painter-smudge','painter-fill-brush-trace','painter-smudge-trace']:
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/target;link[link.index('-o')+1]=str(exe)
 for archive in sorted(set(x for x in link if x.endswith('.a') and (build/x).resolve().is_relative_to(build))):
  if archive in private:continue
  members=[str((build/x).resolve()) for x in subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines()]
  if not any(x in replacements for x in members):continue
  dest=out/archive.replace('/','_');dest.unlink(missing_ok=True);command=['ar','crsT',str(dest),*[replacements.get(x,x) for x in members]]
  report['commands'].append(command);subprocess.run(command,cwd=build,check=True);private[archive]=str(dest)
 link=[private.get(x,replacements.get(str((build/x).resolve()),x)) for x in link];link[1:1]=flags
 report['commands'].append(link);subprocess.run(link,cwd=build,check=True)
 cases=[(None,[])]
 if target.endswith('trace'):cases=[(None,[]),(None,['async' if 'fill' in target else 'owned'])]+[(route,[]) for route in ['raw','path','boundary']]
 for route,extra in cases:
  runenv=dict(env)
  if route:runenv['PAINTER_OWNED_GENERIC_ROUTE']=route
  run=subprocess.run([str(exe),*extra],cwd=build,env=runenv,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  record={'target':target,'generic_route':route,'args':extra,'exit_code':run.returncode,'stderr':run.stderr.decode(),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
  if target.endswith('trace') and run.returncode==0:
   family='fill-brush' if 'fill' in target else 'smudge';prefix=b'BRUSH ' if family=='fill-brush' else b'SMUDGE '
   actual=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(prefix) or x.endswith(b'_CAPTURE_COMPLETE'))+b'\n'
   fixture=root/'migration/fixtures'/('legacy-owned-generic/'+family+'-'+route+'.tsv.gz' if route else 'legacy-'+family+'/pixels.tsv.gz')
   expected=gzip.decompress(fixture.read_bytes());record.update(exact=actual==expected,bytes=len(actual),records=len(actual.splitlines()))
  else:record['stdout']=run.stdout.decode()
  report['runs'].append(record);print(target,route,extra,run.returncode,record.get('exact'),flush=True)
changed=[x for x,h in hashes.items() if hashlib.sha256((root/x).read_bytes()).hexdigest()!=h]
report.update(sources_sha256=hashes,changed_during_run=changed)
a.report.write_text(json.dumps(report,indent=2)+'\n')
if not changed:
 with tarfile.open(a.report.with_suffix('.sources.tar.gz'),'w:gz') as archive:
  for name in sorted(hashes):archive.add(root/name,arcname=name)
assert not changed,changed
assert all(r['exit_code']==0 and r.get('exact',True) for r in report['runs'])
