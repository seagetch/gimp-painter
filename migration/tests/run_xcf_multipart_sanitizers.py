#!/usr/bin/env python3
"""Build/run multipart XCF and typed-provenance focused instrumentation.

Hold /workspace/shared/gimp-painter-build.lock and rebuild normal targets first.
No shared production object/archive is replaced; this is not whole-GIMP or LSan.
"""
import argparse, hashlib, io, json, os, shlex, subprocess, tarfile
from pathlib import Path
from painter_sanitizer_scope import bridge_rtti_sources
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);p.add_argument('--run',action='store_true');p.add_argument('--seal-inputs',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'xcf-multipart-sanitizers';out.mkdir(exist_ok=True)
entries=json.loads((build/'compile_commands.json').read_text())
compiled={}
for e in entries:
 source=(Path(e['directory'])/e['file']).resolve()
 try:relative=source.relative_to(root).as_posix()
 except ValueError:continue
 # Several compatibility/filter sources also have standalone test copies.
 # Instrument the real native archive member, never whichever entry is last.
 module=next((m for m in ['core','painter','xcf'] if relative.startswith('app/'+m+'/') and '/tests/' not in relative),None)
 if module and not e['output'].startswith('app/'+module+'/libapp'+module+'.a.p/'):continue
 compiled[source]=e
filter_candidates={p.relative_to(root).as_posix() for p in (root/'app/painter').glob('filter*.cpp')}
unregistered={source for source in filter_candidates if (root/source).resolve() not in compiled}
unregistered_headers={str(Path(source).with_suffix('.hpp')) for source in unregistered}
targets=['painter-xcf-adversarial','painter-provenance','painter-xcf-roundtrip']
sources={'app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp',
 'app/core/gimpobject.c','app/core/gimpdrawablefilter.c','app/core/gimp-painter-provenance.cpp','app/core/gimpitem.c','app/core/gimpimage-duplicate.c','app/core/gimpclonelayer.cpp','app/core/gimpfilterlayer.cpp',
 'app/tests/test-painter-xcf-argument-resources.cpp',
 'app/xcf/xcf.c','app/xcf/xcf-load.c','app/xcf/xcf-read.c','app/xcf/xcf-seek.c','app/xcf/xcf-save.c','app/xcf/xcf-write.c',
 'app/xcf/painter-xcf-storage.cpp','app/xcf/painter-xcf-multipart.cpp','app/xcf/painter-xcf-transport.cpp',
 'app/xcf/painter-xcf-preserve.cpp','app/xcf/painter-xcf-load.cpp','app/xcf/painter-xcf-compat.cpp','app/xcf/painter-xcf-arguments.cpp',
 *(filter_candidates-unregistered),
 *{'app/tests/test-'+x+'.c' for x in targets}}
headers={'app/tests/test-painter-xcf-multipart.inc','app/tests/test-painter-xcf-multipart-adversarial.inc','app/xcf/painter-xcf-storage.hpp','app/xcf/painter-xcf-multipart.hpp',
 'app/tests/test-painter-xcf-fields.inc','app/tests/test-painter-xcf-active.inc','app/core/gimpdrawablefilter.h','app/core/gimpobject.h','app/core/gimp-painter-provenance.h','app/core/gimp-painter-provenance-private.h',
 'app/core/core-types.h','app/core/core-enums.h','app/core/gimpimage.h','app/core/gimpimage-private.h','app/core/gimpclonelayer.h','app/core/gimpfilterlayer.h','app/core/gimpfilterlayer-arguments.hpp',
 'app/xcf/xcf-private.h','app/xcf/xcf.h','app/xcf/painter-xcf-preserve.h','app/xcf/painter-xcf-load.h','app/xcf/painter-xcf-arguments.hpp',
 *{p.relative_to(root).as_posix() for p in (root/'app/painter').glob('*.hpp') if p.relative_to(root).as_posix() not in unregistered_headers},'migration/tests/painter_sanitizer_scope.py','migration/tests/run_xcf_multipart_sanitizers.py'}
instrumented=set(sources);rtti_candidates=bridge_rtti_sources(root,build)
rtti_only={source for source in rtti_candidates if (root/source).resolve() in compiled}-instrumented;sources|=rtti_only
missing=sorted(source for source in sources if (root/source).resolve() not in compiled)
if missing:raise RuntimeError('Required sanitizer sources are not registered in this build: '+', '.join(missing))
headers|={str(Path(x).with_suffix(suffix)) for x in rtti_only for suffix in ['.h','.hpp'] if (root/Path(x).with_suffix(suffix)).exists()}
def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
hashes={x:digest(root/x) for x in sources|headers}
seal=None
if a.seal_inputs:
 with tarfile.open(a.seal_inputs,'x:gz') as archive:
  for name in sorted(hashes):
   data=(root/name).read_bytes()
   if hashlib.sha256(data).hexdigest()!=hashes[name]:raise RuntimeError('Source changed before sealing: '+name)
   item=tarfile.TarInfo(name);item.size=len(data);item.mode=0o644
   archive.addfile(item,io.BytesIO(data))
  data=(build/'compile_commands.json').read_bytes();item=tarfile.TarInfo('build-input/compile_commands.json');item.size=len(data);item.mode=0o644
  archive.addfile(item,io.BytesIO(data))
 seal={'path':str(a.seal_inputs),'sha256':digest(a.seal_inputs),'selected_source_header_count':len(hashes),'compile_database_sha256':hashlib.sha256(data).hexdigest()}
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
report={'verification':'Fresh source-sealed native multipart/provenance execution; published fixtures reused in place; ordinary upstream and dependency objects remain uninstrumented','scope':f'{len(instrumented)} instrumented native provenance/XCF/layer/serialization units and {len(rtti_only)} RTTI-only compatibility units; remaining host/dependencies uninstrumented; LSan disabled','instrumented_sources':sorted(instrumented),'rtti_compatibility_only_sources':sorted(rtti_only),'sources_sha256':hashes,'changed_during_build':changed,'compile_commands_sha256':hashlib.sha256(json.dumps(commands,sort_keys=True).encode()).hexdigest(),'executables':executables,'results':{}}
report['excluded_unregistered_sources']=sorted(unregistered)
report['excluded_unregistered_headers']=sorted(unregistered_headers)
report['excluded_nonproduction_rtti_sources']=sorted(rtti_candidates-instrumented-rtti_only)
report['compile_time_input_seal']=seal
a.report.write_text(json.dumps(report,indent=2)+'\n')
if changed:
 invalid=sources if any(x not in sources for x in changed) else set(changed)
 for source in invalid:(out/(Path(source).name+'.o')).with_suffix('.key').unlink(missing_ok=True)
 raise RuntimeError('Source changed during build: '+', '.join(changed))
if a.run:
 env=dict(os.environ);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GSETTINGS_BACKEND':'memory','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
 runtime_libraries=sorted({str(path.parent) for path in build.glob('lib*/*.so')})
 env['LD_LIBRARY_PATH']=':'.join(runtime_libraries+[env.get('LD_LIBRARY_PATH','')])
 report['runtime_library_directories']=runtime_libraries
 for target,item in executables.items():
  result=subprocess.run([item['path']],cwd=build,env=env,capture_output=True,text=True)
  report['results'][target]={'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
  report['changed_during_run']=[x for x,h in hashes.items() if digest(root/x)!=h]
  a.report.write_text(json.dumps(report,indent=2)+'\n');print(target,result.returncode,flush=True)
  if result.returncode or report['changed_during_run']:raise RuntimeError('Failed run or changed sources: '+target)
