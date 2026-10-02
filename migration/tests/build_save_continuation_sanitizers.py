#!/usr/bin/env python3
"""Build focused owned Save continuation native GTK instrumentation.

Run the binary on a real native display. This builder does not start GTK.
Hold /workspace/shared/gimp-painter-build.lock; ordinary objects are never replaced.
"""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
from painter_sanitizer_scope import bridge_rtti_sources
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'save-continuation-sanitizers';out.mkdir(exist_ok=True)
common={'app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp'}
sources=common|{'app/paint/gimppaintersmudge.cpp','app/paint/gimppainterpaintgate.cpp','app/paint/painter-mypaint-surface/legacy-mask-transform.cpp','app/paint/painter-mypaint-surface/legacy-generated-mask.cpp','app/widgets/gimpfiledialog.c','app/paint/painter-mypaint-surface/gimp-painter-session.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/tools/gimpfillbrushtool.cpp','app/tools/gimppainttool.c','app/tools/gimpbrushtool.c','app/tools/gimp-tools.c','app/actions/tools-actions.c','app/core/gimpimage.c','app/file/file-save.c','app/plug-in/gimppluginprocedure.c','app/pdb/file-cmds.c','app/xcf/xcf.c','app/xcf/xcf-save.c','app/paint/gimpfillbrush.cpp','app/paint/painter-bounded-fill/search.cpp','app/paint/gimppaintcore.c','app/paint/gimpbrushcore.c','app/paint/gimppaintcore-loops.cc','app/core/gimpimage-undo.c','app/core/gimpitem.c','app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c','app/operations/layer-modes/gimp-layer-modes.c','app/dialogs/file-save-deferred.cpp','app/dialogs/file-save-dialog.c','app/actions/file-commands.c','app/tests/test-painter-save-continuation.cpp'}
headers={'app/paint/painter-mypaint-surface/paint-core.hpp','app/paint/gimppaintersmudge.h','app/paint/gimppainterpaintgate.h','app/paint/gimppainterpaintgate.hpp','app/paint/painter-smudge/legacy-pixels.hpp','app/paint/painter-mypaint-surface/legacy-mask-transform.hpp','app/paint/painter-mypaint-surface/legacy-generated-mask.hpp','app/widgets/gimpfiledialog.h','app/dialogs/file-save-deferred.h','app/dialogs/file-save-dialog.h','app/tests/test-painter-fill-brush-ui.cpp','app/paint/painter-mypaint-surface/gimp-painter-session.h','app/paint/painter-mypaint-surface/gimp-painter-session.hpp','app/paint/painter-mypaint-surface/gimp-painter-options.h','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/core/gimpimage.h','app/tools/gimpfillbrushtool.h','app/paint/gimpfillbrush.h','app/paint/gimppaintcore.h','app/paint/gimpbrushcore.h','app/paint/painter-bounded-fill/search.hpp','app/painter/binding-store.hpp','app/painter/object-ref.hpp','app/painter/source.hpp','app/painter/connection.hpp'}
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
target='painter-save-continuation';cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/target;cmd[cmd.index('-o')+1]=str(exe)
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
a.report.write_text(json.dumps({'scope':f'{len(instrumented)} instrumented units and {len(rtti_only)} RTTI-only compatibility units: owned Save continuation, native command/dialog completion, Fill owner lifecycle, common bridge, search and paint/Undo; remaining host and dependencies uninstrumented; LSan disabled','instrumented_sources':sorted(instrumented),'rtti_compatibility_only_sources':sorted(rtti_only),'sources_sha256':hashes,'changed_during_build':changed,'commands':commands,'executable':str(exe),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'run_status':'Not run by builder; requires native GTK display'},indent=2)+'\n')
assert not changed;print(exe)
