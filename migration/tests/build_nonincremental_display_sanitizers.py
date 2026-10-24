#!/usr/bin/env python3
"""Bounded nonincremental display Surface/controller and native Tool instrumentation; caller holds build lock.

C++ retains the production no-RTTI ABI: vptr sanitizer is explicitly excluded.
No unrelated C++ registration or blocked UI callback units are rebuilt here.
"""
import argparse,hashlib,json,pathlib,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path,required=True);a=p.parse_args()
R=pathlib.Path(__file__).resolve().parents[2];B=a.build.resolve();O=B/'nonincremental-display-sanitizers';O.mkdir(exist_ok=True)
targets=['painter-nonincremental-display'];sources={'app/core/gimpimage-undo.c','app/core/gimpundopopguard.c','app/core/gimpundostack.c','app/core/gimpundo.c','app/core/gimpdrawableundo.c','app/core/gimpitemundo.c','app/core/gimpobject.c','app/paint/gimpmypaintcoreundo.cpp','app/paint/painter-mypaint-surface/paint-core.cpp','app/paint/painter-mypaint-surface/paint-core-pixels.cpp','app/paint/painter-mypaint/engine.cpp','app/paint/painter-mypaint/resource.cpp','app/tests/test-paired-stroke-undo.cpp'}
headers={'app/core/gimpimage-undo.h','app/core/gimpundo.h','app/core/gimpundostack.h','app/core/gimpundopopguard.h','app/paint/gimpmypaintcoreundo.h','app/paint/gimpmypaintcoreundo.hpp','app/paint/painter-mypaint-surface/paint-core.hpp','app/paint/painter-mypaint-surface/paint-core-pixels.hpp','app/paint/painter-mypaint/engine.hpp','app/paint/painter-mypaint/resource.hpp','app/paint/painter-mypaint/mapping.hpp','app/paint/painter-mypaint/legacy-brush.hpp','app/tests/test-gimp-painter-surface.cpp','app/paint/painter-mypaint/tests/test-checkpoint.cpp'}
headers.update('app/tests/'+x for x in ['test-painter-surface-layer-mask.inc','test-painter-surface-snapshot-handoff.inc','test-painter-surface-dirty-handoff.inc','test-painter-surface-undo-handoff.inc','test-painter-surface-commit-cancel.inc','test-painter-surface-small-boundaries.inc','test-painter-surface-undo-payload.inc'])
sources.update({'app/tools/gimppaintermybrushtool.cpp','app/paint/painter-mypaint-surface/gimp-painter-session.cpp','app/tests/test-painter-mypaint-tool.cpp'})
headers.update({'app/tools/gimppaintermybrushtool.hpp','app/tools/gimppaintermybrushtool.h','app/paint/painter-mypaint-surface/gimp-painter-session.hpp','app/paint/painter-mypaint-surface/gimp-painter-session.h','app/tests/test-painter-layer-mask.inc','app/tests/test-painter-layer-mask-visual.inc','app/tests/test-painter-projection-notify.inc'})
sources.discard('app/tests/test-paired-stroke-undo.cpp');sources.discard('app/tests/test-painter-mypaint-tool.cpp')
sources.update({'app/tests/test-painter-physical-gesture.cpp','app/tests/test-painter-physical-tool.cpp','app/paint/painter-mypaint-surface/gegl-surface.cpp','app/paint/painter-mypaint-surface/gimp-resources.cpp','app/paint/gimppaintcore.c','app/tools/gimptool.c'})
headers.update({'app/tests/test-paired-stroke-undo.cpp','app/tests/test-painter-mypaint-tool.cpp'})
sources={'app/tests/test-painter-nonincremental-display.cpp','app/paint/painter-mypaint-surface/gegl-surface.cpp','app/paint/painter-mypaint-surface/gimp-resources.cpp','app/paint/painter-mypaint-surface/paint-core.cpp','app/paint/painter-mypaint-surface/gimp-painter-session.cpp','app/tools/gimppaintermybrushtool.cpp'}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();hashes={n:sha(R/n) for n in sources|headers}
links={t:shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+t],cwd=B,text=True).strip().splitlines()[-1]) for t in targets}
linked=set();archive_members={}
for link in links.values():
 linked.update(str((B/x).resolve()) for x in link if x.endswith('.o'))
 for archive in {x for x in link if x.endswith('.a') and (B/x).resolve().is_relative_to(B)}:
  members=[str((B/x).resolve()) for x in subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines()];archive_members[archive]=members;linked.update(members)
entries=json.loads((B/'compile_commands.json').read_text());replacements={};commands=[];found=set();flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1']
for e in entries:
 source=(pathlib.Path(e['directory'])/e['file']).resolve()
 try:n=source.relative_to(R).as_posix()
 except ValueError:continue
 if n not in sources or str((B/e['output']).resolve()) not in linked:continue
 cmd=shlex.split(e['command']);clean=[];skip=False
 for v in cmd:
  if skip:skip=False;continue
  if v in ['-MF','-MT','-MQ']:skip=True;continue
  if v in ['-MD','-MMD']:continue
  clean.append(v)
 obj=O/(n.replace('/','_')+'.o');clean[clean.index('-o')+1]=str(obj);clean+=flags
 if source.suffix=='.cpp':clean+=['-fno-sanitize=vptr']
 print('compile',n,flush=True);subprocess.run(clean,cwd=B,check=True);commands.append(clean);replacements[str((B/e['output']).resolve())]=str(obj);found.add(n)
assert found==sources,(found,sources-found)
archives={}
for name,members in archive_members.items():
 if not any(x in replacements for x in members):continue
 dest=O/name.replace('/','_');dest.unlink(missing_ok=True);cmd=['ar','crsT',str(dest),*[replacements.get(x,x) for x in members]];subprocess.run(cmd,cwd=B,check=True);commands.append(cmd);archives[name]=str(dest)
executables={}
for t,link in links.items():
 exe=O/t;link[link.index('-o')+1]=str(exe);link=[archives.get(x,replacements.get(str((B/x).resolve()),x)) for x in link];link[1:1]=flags+['-fno-sanitize=vptr'];subprocess.run(link,cwd=B,check=True);commands.append(link);executables[t]={'path':str(exe),'sha256':sha(exe)}
changed=[n for n,h in hashes.items() if sha(R/n)!=h]
a.report.write_text(json.dumps({'instrumented_sources':sorted(sources),'sources_sha256':hashes,'changed_during_build':changed,'commands':commands,'executables':executables,'vptr_sanitizer':False,'leak_detection':False,'dependencies_instrumented':False,'whole_application_instrumented':False},indent=2)+'\n');assert not changed,changed
