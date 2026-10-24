from pathlib import Path
import subprocess,shlex,json,hashlib
W=Path('/workspace/shared/gimp-clean-aggregate');R=W/'source';B=W/'build';O=Path('/workspace/shared/options-readiness/green/asan-private');O.mkdir(exist_ok=True)
flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1'];sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();commands=[];inputs={};replacements={};objects={}
def run(cmd):commands.append(cmd);subprocess.run(cmd,cwd=B,check=True)
entries=json.loads((B/'compile_commands.json').read_text())
for source in ['app/core/gimpcontext.c','app/painter/binding-store.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/tests/test-mypaint-options-readiness.cpp','app/tests/test-mypaint-options-readiness-c.c']:
 e=next(e for e in entries if e['file'].endswith(source));cmd=shlex.split(e['command']);clean=[];skip=False
 for v in cmd:
  if skip:skip=False;continue
  if v in ['-MF','-MT','-MQ']:skip=True;continue
  if v in ['-MD','-MMD']:continue
  clean.append(v)
 obj=O/(Path(source).name+'.o');clean[clean.index('-o')+1]=str(obj);clean+=flags
 if source.endswith('.cpp'):clean+=['-fno-sanitize=vptr']
 inputs[source]=sha(R/source);run(clean);replacements[str((B/e['output']).resolve())]=str(obj)
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/mypaint-options-readiness'],cwd=B,text=True).strip().splitlines()[-1]);archives={}
for archive in {x for x in link if x.endswith('.a') and (B/x).resolve().is_relative_to(B)}:
 raw=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines()
 with (B/archive).open('rb') as f:thin=f.read(8)==b'!<thin>\n'
 if not thin:
  objects[str((B/archive).resolve())]=sha(B/archive);assert not any(Path(x).name in {Path(y).name for y in replacements} for x in raw);continue
 members=[str((B/x).resolve()) for x in raw]
 if any(x in replacements for x in members):
  out=O/archive.replace('/','_');out.unlink(missing_ok=True);run(['ar','crsT',str(out),*[replacements.get(x,x) for x in members]]);archives[archive]=str(out)
 for x in members:objects[x]=sha(x)
for x in link:
 if x.endswith('.o'):objects[str((B/x).resolve())]=sha(B/x)
exe=O/'mypaint-options-readiness';link[link.index('-o')+1]=str(exe);link=[archives.get(x,replacements.get(str((B/x).resolve()),x)) for x in link];link+=flags;run(link)
assert all(sha(p)==h for p,h in objects.items())
(O.parent/'asan-build.json').write_text(json.dumps({'instrumented_sources':inputs,'dependency_objects':objects,'dependency_objects_unchanged':True,'commands':commands,'binary_sha256':sha(exe),'whole_application_instrumented':False,'vptr_instrumented':False,'leak_detection':False},indent=2)+'\n')
