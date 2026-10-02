#!/usr/bin/env python3
"""Build focused model/image/Undo/overlay/tool sanitizer binaries without replacing normal objects.
Caller must hold /tmp/gimp-painter-build.lock. Dependencies/upstream except image adapter are not instrumented.
"""
import argparse,json,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
b=a.build.resolve();root=Path(__file__).resolve().parents[2];out=b/'perspective-sanitizers';out.mkdir(exist_ok=True)
flags=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-O1'];tests=['painter-perspective','painter-perspective-ui','painter-perspective-events','painter-navigation-events']
wanted={'app/display/gimpdisplayshell-tool-events.c','app/display/gimpdisplayshell-handlers.c','app/display/gimpdisplayshell-autoscroll.c','app/display/gimpdisplayshell.c','app/display/gimpmotionbuffer.c','app/display/gimpstatusbar.c','app/tools/gimptool.c','app/core/gimpperspectiveguide.cpp','app/core/gimpperspectiveguideundo.cpp','app/core/gimpimage-perspective-guide.c','app/core/gimpimage.c','app/display/gimpcanvasperspectiveguide.cpp','app/tools/gimpperspectiveguidetool.cpp','app/tools/gimp-tools.c','app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp','app/painter/filter-scheduler.cpp','app/painter/filter-edge.cpp','app/painter/filter-gauss.cpp'}|{'app/tests/test-'+t+'.c' for t in tests}
report={'scope':'Perspective model, common bridge (all archive members rebuilt for consistent RTTI), image owner, Undo, canvas overlay, editing tool, event routing/statusbar, motion buffer, base tool and model/UI/event/navigation tests; other upstream/dependencies uninstrumented','sanitizers':['address','undefined'],'leak_detection':False,'sources':[],'commands':[]};replacements={}
for e in json.loads((b/'compile_commands.json').read_text()):
 source=(Path(e['directory'])/e['file']).resolve()
 try: rel=source.relative_to(root).as_posix()
 except ValueError: continue
 if rel not in wanted:continue
 cmd=shlex.split(e['command']);clean=[];skip=False
 for arg in cmd:
  if skip:skip=False;continue
  if arg in ('-MF','-MQ','-MT'):skip=True;continue
  if arg in ('-MD','-MMD'):continue
  clean.append(arg)
 obj=out/(source.name+'.o');clean[clean.index('-o')+1]=str(obj);clean+=flags
 if source.suffix=='.cpp':clean+=['-frtti']
 subprocess.run(clean,cwd=b,check=True);report['sources'].append(rel);report['commands'].append(clean);replacements[e['output']]=str(obj)
if set(report['sources'])!=wanted:raise RuntimeError('Missing sources: '+repr(wanted-set(report['sources'])))
archives={}
for archive in ['app/core/libappcore.a','app/display/libappdisplay.a','app/tools/libapptools.a','app/painter/libapppainter.a']:
 members=subprocess.check_output(['ar','t',archive],cwd=b,text=True).splitlines();target=out/Path(archive).name
 if target.exists():target.unlink()
 cmd=['ar','crsT',str(target)]+[replacements.get(m,str((b/m).resolve())) for m in members]
 subprocess.run(cmd,cwd=b,check=True);report['commands'].append(cmd);archives[archive]=str(target)
report['executables']=[]
for test in tests:
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+test],cwd=b,text=True).strip().splitlines()[-1]);exe=b/'app/tests'/(test+'-asan');link[link.index('-o')+1]=str(exe)
 link=[replacements.get(x,archives.get(x,x)) for x in link];link[1:1]=flags;subprocess.run(link,cwd=b,check=True);report['commands'].append(link);report['executables'].append(str(exe))
a.report.write_text(json.dumps(report,indent=2)+'\n');print('\n'.join(report['executables']))
