#!/usr/bin/env python3
"""Build focused Painter Smudge native GTK lifecycle instrumentation.

Run the binary on a real native display. This builder does not start GTK.
Hold /workspace/shared/gimp-painter-build.lock; ordinary objects are never replaced.
"""
import argparse, hashlib, json, shlex, subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'smudge-ui-sanitizers';out.mkdir(exist_ok=True)
common={'app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp'}
gate_source='app/paint/gimppainterpaintgate.cpp' if (root/'app/paint/gimppainterpaintgate.cpp').exists() else 'app/paint/gimppainterpaintgate.c'
sources=common|{'app/paint/painter-mypaint-surface/gimp-painter-session.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/tools/gimppaintersmudgetool.cpp','app/tools/gimppainttool.c','app/tools/gimpbrushtool.c','app/tools/gimp-tools.c','app/actions/tools-actions.c','app/core/gimpimage.c','app/file/file-save.c','app/plug-in/gimppluginprocedure.c','app/pdb/file-cmds.c','app/xcf/xcf.c','app/xcf/xcf-save.c','app/paint/gimppaintersmudge.cpp','app/paint/gimpfillbrush.cpp',gate_source,'app/paint/painter-mypaint-surface/legacy-mask-transform.cpp','app/paint/painter-mypaint-surface/legacy-generated-mask.cpp','app/paint/gimpbrushcore-loops.cc','app/paint/gimppaintcore.c','app/paint/gimpbrushcore.c','app/paint/gimppaintcore-loops.cc','app/core/gimpimage-undo.c','app/core/gimpitem.c','app/operations/layer-modes-legacy/gimpoperationpainterlegacy.c','app/operations/layer-modes/gimp-layer-modes.c','app/tests/test-painter-smudge-ui.cpp'}
headers={'app/paint/painter-mypaint-surface/gimp-painter-session.h','app/paint/painter-mypaint-surface/gimp-painter-session.hpp','app/paint/painter-mypaint-surface/gimp-painter-options.h','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','app/core/gimpimage.h','app/tools/gimppaintersmudgetool.h','app/paint/gimppaintersmudge.h','app/paint/gimpfillbrush.h','app/paint/gimppainterpaintgate.h','app/paint/gimppaintcore.h','app/paint/gimpbrushcore.h','app/paint/painter-smudge/legacy-pixels.hpp','app/painter/binding-store.hpp','app/painter/object-ref.hpp','app/painter/source.hpp','app/painter/connection.hpp'}
if (root/'app/paint/gimppainterpaintgate.hpp').exists():headers.add('app/paint/gimppainterpaintgate.hpp')
hashes={x:hashlib.sha256((root/x).read_bytes()).hexdigest() for x in sources|headers}
entries=json.loads((build/'compile_commands.json').read_text());objects={};originals={};commands=[];flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1']
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
archives=['app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a','app/plug-in/libappplug-in.a','app/file/libappfile.a','app/pdb/libappinternalprocs.a','app/pdb/libapppdb.a','app/xcf/libappxcf.a','app/tools/libapptools.a','app/actions/libappactions.a','app/paint/libapppaint.a','app/painter/libapppainter.a','app/core/libappcore.a','app/operations/layer-modes-legacy/libapplayermodeslegacy.a','app/operations/layer-modes/libapplayermodes.a'];replaced={}
for archive in archives:
 result=[]
 for member in subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines():
  source=originals.get(member)
  if source in common:continue
  result.append(objects[source] if source else str((build/member).resolve()))
 dest=out/Path(archive).name
 if dest.exists():dest.unlink()
 cmd=['ar','crsT',str(dest),*result];commands.append(cmd);subprocess.run(cmd,cwd=build,check=True);replaced[archive]=str(dest)
target='painter-smudge-ui';cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/target;cmd[cmd.index('-o')+1]=str(exe)
cmd=[objects[originals[x]] if x in originals and originals[x].startswith('app/tests/') else replaced.get(x,x) for x in cmd];cmd[1:1]=[*flags,*[objects[x] for x in sorted(common)]];commands.append(cmd);subprocess.run(cmd,cwd=build,check=True)
changed=[x for x,h in hashes.items() if hashlib.sha256((root/x).read_bytes()).hexdigest()!=h]
a.report.write_text(json.dumps({'scope':f'{len(sources)} focused units: native Smudge GTK controller/options/registration, common bridge, exact legacy raster, paint/Undo and mode dispatch; remaining host and dependencies uninstrumented; LSan disabled','sources_sha256':hashes,'changed_during_build':changed,'commands':commands,'executable':str(exe),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'run_status':'Not run by builder; requires native GTK display'},indent=2)+'\n')
assert not changed;print(exe)
