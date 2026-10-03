#!/usr/bin/env python3
"""Build/run reconstructed adversarial XCF recovery focused instrumentation.

Hold /workspace/shared/gimp-painter-build.lock and rebuild normal targets first.
No shared production object/archive is replaced; this is not whole-GIMP or LSan.
"""
import argparse, hashlib, json, os, shlex, subprocess
from pathlib import Path
from painter_sanitizer_scope import bridge_rtti_sources
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--run',action='store_true');a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'xcf-adversarial-rtti-sanitizers';out.mkdir(exist_ok=True)
entries=json.loads((build/'compile_commands.json').read_text())
compiled={(Path(e['directory'])/e['file']).resolve():e for e in entries}
filter_candidates={p.relative_to(root).as_posix() for p in (root/'app/painter').glob('filter*.cpp')}
unregistered={source for source in filter_candidates if (root/source).resolve() not in compiled}
unregistered_headers={str(Path(source).with_suffix('.hpp')) for source in unregistered}
targets=['painter-xcf-adversarial']
sources={'app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp',
 'app/core/gimpobject.c','app/core/gimpdrawablefilter.c','app/core/gimp-painter-provenance.cpp','app/core/gimpitem.c','app/core/gimpimage-duplicate.c','app/core/gimpclonelayer.cpp','app/core/gimpfilterlayer.cpp',
 'app/xcf/xcf.c','app/xcf/xcf-load.c','app/xcf/xcf-read.c','app/xcf/xcf-seek.c','app/xcf/xcf-save.c','app/xcf/xcf-write.c',
 'app/xcf/painter-xcf-preserve.cpp','app/xcf/painter-xcf-load.cpp','app/xcf/painter-xcf-compat.cpp','app/xcf/painter-xcf-arguments.cpp',
 *(filter_candidates-unregistered),
 *{'app/tests/test-'+x+'.c' for x in targets}}
headers={'app/tests/test-painter-xcf-fields.inc','app/tests/test-painter-xcf-active.inc','app/core/gimpdrawablefilter.h','app/core/gimpobject.h','app/core/gimp-painter-provenance.h','app/core/gimp-painter-provenance-private.h',
 'app/core/core-types.h','app/core/core-enums.h','app/core/gimpimage.h','app/core/gimpimage-private.h','app/core/gimpclonelayer.h','app/core/gimpfilterlayer.h','app/core/gimpfilterlayer-arguments.hpp',
 'app/xcf/xcf-private.h','app/xcf/xcf.h','app/xcf/painter-xcf-preserve.h','app/xcf/painter-xcf-load.h','app/xcf/painter-xcf-arguments.hpp',
 *{p.relative_to(root).as_posix() for p in (root/'app/painter').glob('*.hpp') if p.relative_to(root).as_posix() not in unregistered_headers},'migration/tests/painter_sanitizer_scope.py','migration/tests/run_painter_xcf_adversarial_sanitizers.py'}
instrumented=set(sources);rtti_only=bridge_rtti_sources(root,build)-instrumented;sources|=rtti_only
missing=sorted(source for source in sources if (root/source).resolve() not in compiled)
if missing:raise RuntimeError('Required sanitizer sources are not registered in this build: '+', '.join(missing))
headers|={str(Path(x).with_suffix(suffix)) for x in rtti_only for suffix in ['.h','.hpp'] if (root/Path(x).with_suffix(suffix)).exists()}
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
hashes={x:digest(root/x) for x in sources|headers}
objects={};originals={};commands=[];flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1']
for source in sorted(sources):
 e=compiled[(root/source).resolve()];cmd=shlex.split(e['command']);clean=[];skip=False
 for arg in cmd:
  if skip:skip=False;continue
  if arg in ['-MF','-MQ','-MT']:skip=True;continue
  if arg in ['-MD','-MMD']:continue
  clean.append(arg)
 obj=out/(Path(source).name+'.o');clean[clean.index('-o')+1]=str(obj)
 if source not in rtti_only:clean+=flags
 if source.endswith(('.cpp','.cc')):clean+=['-frtti']
 commands.append(clean)
 key=hashlib.sha256(json.dumps({'command':clean,'source':hashes[source],'headers':{h:hashes[h] for h in headers},'ordinary':digest(build/e['output'])},sort_keys=True).encode()).hexdigest()
 keyfile=obj.with_suffix('.key')
 if not (obj.exists() and keyfile.exists() and keyfile.read_text()==key):
  print('compile',source,flush=True);subprocess.run(clean,cwd=build,check=True);keyfile.write_text(key)
 objects[source]=str(obj);originals[e['output']]=source
absolute={str((build/key).resolve()):objects[value] for key,value in originals.items()}
archives={};executables={}
for target in targets:
 cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/target;cmd[cmd.index('-o')+1]=str(exe)
 for archive in sorted({x for x in cmd if x.endswith('.a') and (build/x).resolve().is_relative_to(build)}):
  if archive not in archives:
   members=[str((build/x).resolve()) for x in subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines()]
   if not any(x in absolute for x in members):archives[archive]=None;continue
   dest=out/archive.replace('/','_')
   if dest.exists():dest.unlink()
   arcmd=['ar','crsT',str(dest),*[absolute.get(x,x) for x in members]];commands.append(arcmd);subprocess.run(arcmd,cwd=build,check=True);archives[archive]=str(dest)
  if archives[archive]:cmd=[archives[archive] if x==archive else x for x in cmd]
 cmd=[objects[originals[x]] if x in originals and originals[x].startswith('app/tests/') else x for x in cmd];cmd[1:1]=flags;commands.append(cmd);subprocess.run(cmd,cwd=build,check=True)
 executables[target]={'path':str(exe),'sha256':digest(exe)}
changed=[x for x,h in hashes.items() if digest(root/x)!=h]
report={'verification':'New execution after runtime restoration; initial mixed-RTTI failure retained separately; no missing historic logs reused','scope':f'{len(instrumented)} instrumented native provenance/XCF/layer/serialization units and {len(rtti_only)} RTTI-only compatibility units; remaining host/dependencies uninstrumented; LSan disabled','instrumented_sources':sorted(instrumented),'rtti_compatibility_only_sources':sorted(rtti_only),'sources_sha256':hashes,'changed_during_build':changed,'commands':commands,'executables':executables,'results':{}}
report['excluded_unregistered_sources']=sorted(unregistered)
report['excluded_unregistered_headers']=sorted(unregistered_headers)
a.report.write_text(json.dumps(report,indent=2)+'\n')
if changed:
 invalid=sources if any(x not in sources for x in changed) else set(changed)
 for source in invalid:(out/(Path(source).name+'.o')).with_suffix('.key').unlink(missing_ok=True)
 raise RuntimeError('Source changed during build: '+', '.join(changed))
if a.run:
 env=dict(os.environ);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GSETTINGS_BACKEND':'memory','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
 for target,item in executables.items():
  result=subprocess.run([item['path']],cwd=build,env=env,capture_output=True,text=True)
  report['results'][target]={'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
  report['changed_during_run']=[x for x,h in hashes.items() if digest(root/x)!=h]
  a.report.write_text(json.dumps(report,indent=2)+'\n');print(target,result.returncode,flush=True)
  if result.returncode or report['changed_during_run']:raise RuntimeError('Failed run or changed sources: '+target)
