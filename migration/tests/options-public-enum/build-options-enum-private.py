from pathlib import Path
import subprocess,shlex,json,hashlib,os,sys
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/gimp-clean-aggregate');B=W/'build';sanitized='--sanitize' in sys.argv;O=Path('/workspace/shared/options-enum-private-asan' if sanitized else '/workspace/shared/options-enum-private');O.mkdir(exist_ok=True)
if sanitized:R=W/'source'
flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1'] if sanitized else []
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();commands=[]
def run(cmd):
 commands.append(cmd);subprocess.run(cmd,cwd=B,check=True)
def command(target):return shlex.split(subprocess.check_output(['ninja','-t','commands',target],cwd=B,text=True).strip().splitlines()[-1])
entries=json.loads((B/'compile_commands.json').read_text());inputs={};replacements={}
def compile_from(template,source,name,extra=[]):
 e=next(e for e in entries if e['file'].endswith(template));cmd=shlex.split(e['command']);clean=[];skip=False
 for v in cmd:
  if skip:skip=False;continue
  if v in ['-MF','-MT','-MQ']:skip=True;continue
  if v in ['-MD','-MMD']:continue
  clean.append(v)
 obj=O/(name+'.o');clean[clean.index('-o')+1]=str(obj);clean[clean.index('-c')+1]=str(R/source);clean[1:1]=['-I'+str(R),'-I'+str(R/'app'),'-I'+str(O)];clean+=extra+flags
 if sanitized and source.endswith('.cpp'):clean+=['-fno-sanitize=vptr']
 inputs[source]=sha(R/source);print('compile',source,flush=True);run(clean);return e,obj
# Use private stamp, never mutate clean generator artifacts.
(O/'stamp-gimpbaseenums.h').write_text('/* Private enum proof generated-source prerequisite. */\n')
e,enum=compile_from('libgimpbase/gimpbaseenums.c','libgimpbase/gimpbaseenums.c','enum')
link=command('libgimpbase/libgimpbase-3.0.so.0.8.1');lib=O/'libgimpbase-3.0.so.0.8.1';link[link.index('-o')+1]=str(lib);link=[str(enum) if x==e['output'] else x for x in link];link+=flags
object_inputs={str((B/x).resolve()):sha((B/x).resolve()) for x in link if x.endswith('.o') and (B/x).exists()};run(link)
for name in ['libgimpbase-3.0.so.0','libgimpbase-3.0.so']:
 p=O/name;p.unlink(missing_ok=True);p.symlink_to(lib.name)
if sanitized:
 e,bind=compile_from('app/painter/binding-store.cpp','app/painter/binding-store.cpp','binding');replacements[str((B/e['output']).resolve())]=str(bind)
e,opt=compile_from('gimp-painter-options.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','options');replacements[str((B/e['output']).resolve())]=str(opt)
e,cpp=compile_from('test-mypaint-options-mode.cpp','app/tests/test-mypaint-enum-public.cpp','enum-test');replacements[str((B/e['output']).resolve())]=str(cpp)
e,c=compile_from('test-mypaint-options-mode-c.c','app/tests/test-mypaint-enum-public-c.c','enum-c-test');replacements[str((B/e['output']).resolve())]=str(c)
link=command('app/tests/mypaint-options-mode');archives={}
for archive in {x for x in link if x.endswith('.a') and (B/x).resolve().is_relative_to(B)}:
 raw_members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines()
 with (B/archive).open('rb') as f:thin=f.read(8)==b'!<thin>\n'
 if not thin:
  object_inputs[str((B/archive).resolve())]=sha((B/archive).resolve())
  assert not any(Path(x).name in {Path(y).name for y in replacements} for x in raw_members),('replacement inside normal archive',archive)
  continue
 members=[str((B/x).resolve()) for x in raw_members]
 if any(x in replacements for x in members):
  out=O/archive.replace('/','_');out.unlink(missing_ok=True);run(['ar','crsT',str(out),*[replacements.get(x,x) for x in members]]);archives[archive]=str(out)
 for x in members:object_inputs[x]=sha(Path(x))
for x in link:
 if x.endswith('.o'):object_inputs[str((B/x).resolve())]=sha((B/x).resolve())
link[link.index('-o')+1]=str(O/'mypaint-enum-public');link=[str(lib) if x=='libgimpbase/libgimpbase-3.0.so.0.8.1' else archives.get(x,replacements.get(str((B/x).resolve()),x)) for x in link];link+=flags;run(link)
# Recompile and run all six committed helper cases against the moved enum/header.
e,modecpp=compile_from('test-mypaint-options-mode.cpp','app/tests/test-mypaint-options-mode.cpp','mode-test')
mode_replace={str((B/e['output']).resolve()):str(modecpp)}
e,modec=compile_from('test-mypaint-options-mode-c.c','app/tests/test-mypaint-options-mode-c.c','mode-c-test');mode_replace[str((B/e['output']).resolve())]=str(modec)
mode_link=command('app/tests/mypaint-options-mode');mode_link[mode_link.index('-o')+1]=str(O/'mypaint-options-mode');mode_link=[str(lib) if x=='libgimpbase/libgimpbase-3.0.so.0.8.1' else archives.get(x,mode_replace.get(str((B/x).resolve()),x)) for x in mode_link];mode_link+=flags;run(mode_link)
# The C-only executable links the public shared library, no libapp/archive.
pkgflags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gobject-2.0','gio-2.0','gegl-0.4','gexiv2'],text=True));run(['cc','-std=c11','-DMYPAINT_ENUM_STANDALONE','-I'+str(B),'-I'+str(R),str(R/'app/tests/test-mypaint-enum-public-c.c'),'-L'+str(O),'-lgimpbase-3.0','-Wl,-rpath,'+str(O),'-o',str(O/'mypaint-enum-library-only'),*pkgflags,*(['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1'] if sanitized else [])])
for p in ['libgimpbase/gimpbaseenums.h','libgimpbase/gimpbase.def','libgimp/gimpenums.c.tail','app/paint/gimpmypaintoptions.h','app/paint/painter-mypaint-surface/gimp-painter-options.hpp','tools/meson-mkenums.py','tools/gimp-mkenums']:inputs[p]=sha(R/p)
changed=[p for p,h in object_inputs.items() if sha(Path(p))!=h];assert not changed,changed
(O/'build.json').write_text(json.dumps({'scope':'bounded private relink, not real Meson full integration','sanitized':sanitized,'vptr_instrumented':False,'leak_detection':False,'whole_application_instrumented':False,'owned_sources':inputs,'dependency_objects':object_inputs,'dependency_objects_unchanged':True,'commands':commands,'binaries':{p.name:sha(p) for p in [lib,O/'mypaint-enum-public',O/'mypaint-enum-library-only',O/'mypaint-options-mode']}},indent=2)+'\n')
