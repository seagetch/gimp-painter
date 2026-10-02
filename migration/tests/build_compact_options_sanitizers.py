#!/usr/bin/env python3
"""Build focused compact tool-options native GTK lifecycle instrumentation.

Run the binary on a real native display. This builder does not start GTK.
Hold /workspace/shared/gimp-painter-build.lock; ordinary objects are never replaced.
"""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
from painter_sanitizer_scope import bridge_rtti_sources
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'compact-options-sanitizers';out.mkdir(exist_ok=True)
common={'app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp'}
sources=common|{'app/widgets/gimppaintercompactoptions.cpp','app/display/gimppaintercanvasui.cpp','app/widgets/gimptooloptionseditor.c','app/widgets/gimppropwidgets.c','app/widgets/gimpviewablebox.c','app/widgets/gimpviewablebutton.c','app/widgets/gimpbrusheditor.c','app/widgets/gimpdynamicseditor.c','app/widgets/gimpdataeditor.c','app/tools/gimppaintoptions-gui.c','app/core/gimpcontext.c','app/core/gimptooloptions.c','app/core/gimppainterprofile.cpp','app/config/gimpcoreconfig.c','app/actions/view-commands.c','app/tests/test-painter-compact-options.c'}
headers={'app/config/gimpcoreconfig.h','app/core/gimppainterprofile.h','app/core/gimp.h','app/widgets/gimppaintercompactoptions.h','app/display/gimppaintercanvasui.h','app/core/gimptooloptions.h','app/painter/binding-store.hpp','app/painter/object-ref.hpp','app/painter/connection.hpp','app/painter/boundary.hpp'}

instrumented=set(sources)
rtti_only=bridge_rtti_sources(root,build)-instrumented
sources|=rtti_only
headers|={str(Path(x).with_suffix(suffix)) for x in rtti_only for suffix in ['.h','.hpp'] if (root/Path(x).with_suffix(suffix)).exists()}
headers|={'app/paint/painter-mypaint-surface/paint-core.cpp','migration/tests/painter_sanitizer_scope.py'}
hashes={x:hashlib.sha256((root/x).read_bytes()).hexdigest() for x in sources|headers}
entries=json.loads((build/'compile_commands.json').read_text());objects={};originals={};commands=[];flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1']
for source in sorted(sources):
 e=next(e for e in entries if (Path(e['directory'])/e['file']).resolve()==root/source);cmd=shlex.split(e['command']);clean=[];skip=False
 for arg in cmd:
  if skip:skip=False;continue
  if arg in ['-MF','-MQ','-MT']:skip=True;continue
  if arg in ['-MD','-MMD']:continue
  clean.append(arg)
 obj=out/(Path(source).name+'.o');clean[clean.index('-o')+1]=str(obj)
 if source not in rtti_only:clean+=flags
 if source.endswith(('.cpp','.cc')):clean+=['-frtti']
 commands.append(clean)
 # Cache only exact commands, source/header inputs and the freshly rebuilt
 # ordinary object (which carries Meson's full transitive dependency check).
 key=hashlib.sha256(json.dumps({'command':clean,'source':hashes[source],
   'headers':{h:hashes[h] for h in headers},
   'ordinary':hashlib.sha256((build/e['output']).read_bytes()).hexdigest()},sort_keys=True).encode()).hexdigest()
 keyfile=obj.with_suffix('.key')
 if not (obj.exists() and keyfile.exists() and keyfile.read_text()==key):
  subprocess.run(clean,cwd=build,check=True);keyfile.write_text(key)
 objects[source]=str(obj);originals[e['output']]=source
target='painter-compact-options';cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/target;cmd[cmd.index('-o')+1]=str(exe)
# Replace every Meson thin-archive alias, including link_whole flattening.
absolute={str((build/key).resolve()):objects[value] for key,value in originals.items()}
for archive in sorted({x for x in cmd if x.endswith('.a') and (build/x).resolve().is_relative_to(build)}):
 members=[str((build/x).resolve()) for x in subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines()]
 if not any(x in absolute for x in members):continue
 dest=out/archive.replace('/','_')
 if dest.exists():dest.unlink()
 arcmd=['ar','crsT',str(dest),*[absolute.get(x,x) for x in members]];commands.append(arcmd);subprocess.run(arcmd,cwd=build,check=True)
 cmd=[str(dest) if x==archive else x for x in cmd]
cmd=[objects[originals[x]] if x in originals and originals[x].startswith('app/tests/') else x for x in cmd];cmd[1:1]=flags;commands.append(cmd);subprocess.run(cmd,cwd=build,check=True)
changed=[x for x,h in hashes.items() if hashlib.sha256((root/x).read_bytes()).hexdigest()!=h]
a.report.write_text(json.dumps({'scope':f'{len(instrumented)} instrumented units and {len(rtti_only)} RTTI-only compatibility units: native compact grouping, GTK ownership/reparenting, Canvas integration, property/resource controls and canonical option models; remaining host and dependencies uninstrumented; LSan disabled','instrumented_sources':sorted(instrumented),'rtti_compatibility_only_sources':sorted(rtti_only),'sources_sha256':hashes,'changed_during_build':changed,'commands':commands,'executable':str(exe),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'run_status':'Not run by builder; requires native GTK display'},indent=2)+'\n')
if changed:
 # Never reuse an object under its starting hash after a concurrent source
 # edit. A changed header invalidates every candidate translation unit.
 invalid = sources if any(x not in sources for x in changed) else set(changed)
 for source in invalid:
  (out / (Path(source).name + '.o')).with_suffix('.key').unlink(missing_ok=True)
 raise RuntimeError('Source changed during build: '+', '.join(changed))
print(exe)
