#!/usr/bin/env python3
"""Private native proof build; caller holds clean build.lock, no rebuild."""
import argparse,hashlib,json,os,pathlib,shlex,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--build',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent;B=a.build.resolve();S=B.parent/'source';O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
target='app/tests/gimp-painter-surface';fixture='app/tests/test-gimp-painter-surface.cpp';own=E/'test.cpp'
link=shlex.split(subprocess.check_output(['ninja','-t','commands',target],cwd=B,text=True).strip().splitlines()[-1]);entries=json.loads((B/'compile_commands.json').read_text())
archives={};linked=set();tracked=set()
for x in link:
 p=(B/x).resolve()
 if x.endswith('.o'):linked.add(str(p));tracked.add(p)
 if x.endswith('.a') and p.is_file():
  members=([str((B/n).resolve())for n in subprocess.check_output(['ar','t',x],cwd=B,text=True).splitlines()] if p.read_bytes()[:8]==b'!<thin>\n' else []);archives[x]=members;linked.update(members);tracked.add(p);tracked.update(pathlib.Path(n)for n in members)
 if '.so' in x and p.is_file():tracked.add(p)
selected=['app/paint/painter-mypaint/resource.cpp','app/paint/painter-mypaint-surface/gimp-resources.cpp','app/core/gimpbrush.c','app/core/gimpbrushgenerated.c','app/core/gimppattern.c','app/core/gimpdata.c','app/core/gimpobject.c']
for n in selected:assert (S/n).read_bytes()==subprocess.check_output(['git','show','20b2204b11:'+n],cwd=R),n
source={n:sha(S/n)for n in selected};source[str(own.relative_to(R))]=sha(own)
all_before={str(p):sha(p)for p in sorted(tracked)}
records=[];depfiles=[];dynamic_before={}
def run(name,cmd,env=None,timeout=180):
 t=time.monotonic();z=subprocess.run(cmd,cwd=B,env=env,capture_output=True,text=True,timeout=timeout);records.append(dict(name=name,command=cmd,returncode=z.returncode,stdout=z.stdout,stderr=z.stderr,seconds=time.monotonic()-t));return z

def build(mode,flags,mutation=None):
 replacements={};found=set()
 for ent in entries:
  old=(pathlib.Path(ent['directory'])/ent['file']).resolve();obj=str((B/ent['output']).resolve())
  try:n=old.relative_to(S).as_posix()
  except ValueError:continue
  names=[fixture]+(selected if flags else [])
  if mutation:names=[fixture,'app/paint/painter-mypaint-surface/gimp-resources.cpp']
  if n not in names or obj not in linked:continue
  cmd=shlex.split(ent['command']);clean=[];skip=False
  for x in cmd:
   if skip:skip=False;continue
   if x in ['-MF','-MT','-MQ']:skip=True;continue
   if x in ['-MD','-MMD']:continue
   clean.append(x)
  dest=O/(mode+'-'+n.replace('/','_')+'.o');clean[clean.index('-o')+1]=str(dest)
  if n==fixture:clean[clean.index(ent['file'])]=str(own)
  if mutation and n.endswith('gimp-resources.cpp'):
   data=old.read_text();before,after=mutation;assert data.count(before)==1;path=O/(mode+'-gimp-resources.cpp');path.write_text(data.replace(before,after));clean[clean.index(ent['file'])]=str(path);clean+=['-I'+str(old.parent)]
  dep=O/(mode+'-'+n.replace('/','_')+'.d');clean+=flags+['-O1','-MD','-MF',str(dep)];depfiles.append(dep)
  if flags and old.suffix=='.cpp':clean+=['-fno-sanitize=vptr']
  z=run(mode+'-compile-'+n,clean);assert z.returncode==0,z.stderr
  replacements[obj]=str(dest);found.add(n)
 assert found==set(names),(found,names)
 modified={}
 for name,members in archives.items():
  if not any(n in replacements for n in members):continue
  dest=O/(mode+'-'+name.replace('/','_'));dest.unlink(missing_ok=True)
  cmd=['ar','crsT',str(dest),*[replacements.get(n,n)for n in members]];z=run(mode+'-archive-'+name,cmd);assert z.returncode==0,z.stderr;modified[name]=str(dest)
 cmd=[modified.get(x,replacements.get(str((B/x).resolve()),x))for x in link];exe=O/mode;cmd[cmd.index('-o')+1]=str(exe);cmd[1:1]=flags+(['-fno-sanitize=vptr']if flags else [])
 # Preserve original runtime search dirs when executable lives outside build/app/tests.
 cmd+=['-Wl,-rpath,'+':'.join(str(B/n)for n in ['libgimpmath','libgimpbase','libgimpcolor','libgimpconfig','libgimpmodule','libgimpthumb','libgimpwidgets'])]
 z=run(mode+'-link',cmd);assert z.returncode==0,z.stderr;return exe

def execute(mode,exe):
 env=os.environ.copy();profile=O/('profile-'+mode)
 for key,suffix in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
  p=profile/suffix;p.mkdir(parents=True,exist_ok=True);p.chmod(0o700);env[key]=str(p)
 env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(B),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 trace=run(mode+'-loader',[str(exe)],dict(env,LD_TRACE_LOADED_OBJECTS='1'),30)
 for line in trace.stdout.splitlines():
  for token in line.split():
   if token.startswith('/') and pathlib.Path(token).is_file():dynamic_before.setdefault(token,sha(pathlib.Path(token)))
 return run(mode+'-run',[str(exe)],env,90)
try:
 normal=build('normal',[]);z=execute('normal',normal);print('normal',z.returncode,z.stdout[-2000:],z.stderr[-1000:],flush=True);assert z.returncode==0
 asan=build('asan',['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer']);z=execute('asan',asan);print('asan',z.returncode,z.stdout[-2000:],z.stderr[-1000:],flush=True);assert z.returncode==0
 for which,typ,field in [('brush','GIMP_BRUSH','brush'),('paper','GIMP_PATTERN','texture')]:
  needle=f'      r.{field}_missing=!matched;';replacement=needle+' matched=nullptr;'
  exe=build('mutant-'+which,[],(needle,replacement));z=execute('mutant-'+which,exe);assert z.returncode!=0 and 'effective_' in z.stderr+z.stdout,z.stderr
finally:
 changed=[p for p,h in dict(all_before,**dynamic_before).items()if sha(pathlib.Path(p))!=h]
 deps={}
 for dep in depfiles:
  if dep.exists():
   for n in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
    path=(B/n).resolve()
    if path.is_file():deps[str(path)]=sha(path)
 (O/'results.json').write_text(json.dumps({'scope':'clean20b providers; explicit unrelated source overlays; private native link; no Preview branch/no wholeHEAD claim','provider_base':'20b2204b11','source_sha256':source,'linked_inputs_sha256':all_before,'linked_inputs_changed':changed,'runtime_dependency_sha256':dynamic_before,'compiled_dependency_sha256':deps,'results':records},indent=2)+'\n')
 assert not changed,changed
print('PASS native2/scoped8-unit sanitizer2;2 lookup mutants;linked inputs unchanged')
