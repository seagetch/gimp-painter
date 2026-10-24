#!/usr/bin/env python3
"""Caller holds frozen clean build lock; only private outputs are modified."""
import hashlib,json,os,pathlib,shlex,subprocess,time
R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');O=pathlib.Path(__file__).resolve().parent
B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base=json.loads((O/'provider.json').read_text())['base_commit']
clean_commit=(B.parent/'source-commit.txt').read_text().strip();assert clean_commit=='2ef2dd32eeb7201e8e0318f169c6005a21c10410'
clean_manifest=json.loads((B.parent/('exact-'+clean_commit)/'prebuild-tracked-source-sha256.json').read_text())
assert all(sha(S/n)==h for n,h in clean_manifest.items()),'clean source changed before link'
(O/'clean-source-manifest.json').write_text(json.dumps(clean_manifest,sort_keys=True,indent=2)+'\n')
fixture='app/tests/test-gimp-painter-surface.cpp';tools='app/tools/gimp-tools.c'
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/gimp-painter-surface'],cwd=B,text=True).strip().splitlines()[-1]);entries=json.loads((B/'compile_commands.json').read_text())
archives={};linked=set();tracked=set()
for x in link:
 p=(B/x).resolve()
 if x.endswith('.o'):linked.add(str(p));tracked.add(p)
 if x.endswith('.a') and p.is_file():
  members=([str((B/n).resolve())for n in subprocess.check_output(['ar','t',x],cwd=B,text=True).splitlines()] if p.read_bytes()[:8]==b'!<thin>\n' else []);archives[x]=members;linked.update(members);tracked.add(p);tracked.update(pathlib.Path(n)for n in members)
 if '.so' in x and p.is_file():tracked.add(p)
selected=['app/core/gimptooloptions.c','app/core/gimptoolinfo.c']
# Compile the exact committed providers rather than trusting unrelated overlays.
for n in selected:
 p=O/n.replace('/','_');p.write_bytes(subprocess.check_output(['git','show',base+':'+n],cwd=R))
source={n:sha(O/n.replace('/','_')) for n in selected}
source.update({'test.cpp':sha(O/'test.cpp'),'before-tools':sha(O/'gimp-tools-before.c'),'after-tools':sha(O/'gimp-tools-after.c')})
before={str(p):sha(p)for p in sorted(tracked)};records=[];depfiles=[];dynamic={}
def run(name,cmd,env=None,timeout=240):
 t=time.monotonic();z=subprocess.run(cmd,cwd=B,env=env,capture_output=True,text=True,timeout=timeout);records.append(dict(name=name,command=cmd,returncode=z.returncode,stdout=z.stdout,stderr=z.stderr,seconds=time.monotonic()-t));return z

def build(mode,flags,after):
 names=[fixture,tools]+selected;replacements={};found=set()
 for ent in entries:
  old=(pathlib.Path(ent['directory'])/ent['file']).resolve();obj=str((B/ent['output']).resolve())
  try:n=old.relative_to(S).as_posix()
  except ValueError:continue
  if n not in names or obj not in linked:continue
  cmd=shlex.split(ent['command']);clean=[];skip=False
  for x in cmd:
   if skip:skip=False;continue
   if x in ['-MF','-MT','-MQ']:skip=True;continue
   if x in ['-MD','-MMD']:continue
   clean.append(x)
  dest=O/(mode+'-'+n.replace('/','_')+'.o');clean[clean.index('-o')+1]=str(dest)
  provider=O/('test.cpp' if n==fixture else ('gimp-tools-after.c' if after else 'gimp-tools-before.c') if n==tools else n.replace('/','_'))
  clean[clean.index(ent['file'])]=str(provider);clean+=['-I'+str(old.parent)]
  dep=O/(mode+'-'+n.replace('/','_')+'.d');clean+=flags+['-O1','-MD','-MF',str(dep)];depfiles.append(dep)
  if flags and old.suffix=='.cpp':clean+=['-fno-sanitize=vptr']
  z=run(mode+'-compile-'+n,clean);assert z.returncode==0,z.stderr
  replacements[obj]=str(dest);found.add(n)
 assert found==set(names),(found,names)
 modified={}
 for name,members in archives.items():
  if not any(n in replacements for n in members):continue
  dest=O/(mode+'-'+name.replace('/','_'));dest.unlink(missing_ok=True)
  z=run(mode+'-archive-'+name,['ar','crsT',str(dest),*[replacements.get(n,n)for n in members]]);assert z.returncode==0,z.stderr;modified[name]=str(dest)
 cmd=[modified.get(x,replacements.get(str((B/x).resolve()),x))for x in link];exe=O/mode;cmd[cmd.index('-o')+1]=str(exe);cmd[1:1]=flags+(['-fno-sanitize=vptr']if flags else [])
 cmd+=['-Wl,-rpath,'+':'.join(str(B/n)for n in ['libgimpmath','libgimpbase','libgimpcolor','libgimpconfig','libgimpmodule','libgimpthumb','libgimpwidgets'])]
 z=run(mode+'-link',cmd);assert z.returncode==0,z.stderr;return exe

def execute(mode,exe,args=[]):
 env=os.environ.copy();profile=O/('profile-'+mode)
 for key,suffix in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
  p=profile/suffix;p.mkdir(parents=True,exist_ok=True);p.chmod(0o700);env[key]=str(p)
 # Native helper may write pluginrc output. Isolate its build-root too.
 private_build=profile/'build';(private_build/'app/tests/gimpdir-output').mkdir(parents=True,exist_ok=True)
 env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(private_build),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',LC_ALL='C.UTF-8')
 trace=run(mode+'-loader',[str(exe)],dict(env,LD_TRACE_LOADED_OBJECTS='1'),30)
 for line in trace.stdout.splitlines():
  for token in line.split():
   if token.startswith('/') and pathlib.Path(token).is_file():dynamic.setdefault(token,sha(pathlib.Path(token)))
 return run(mode+'-run',[str(exe)]+args,env,240)
try:
 red=build('red',[],False);z=execute('red',red,['-p','/tool-options-delete/caller-notempty']);print('RED',z.returncode,z.stdout[-1500:],z.stderr[-2000:],flush=True);assert z.returncode!=0 and "'error' should not be nullptr" in z.stdout+z.stderr
 normal=build('normal',[],True);z=execute('normal',normal);print('NORMAL',z.returncode,z.stdout[-2000:],z.stderr[-2000:],flush=True);assert z.returncode==0
 asan=build('asan',['-fsanitize=address,undefined','-fno-omit-frame-pointer'],True);z=execute('asan',asan);print('ASAN',z.returncode,z.stdout[-2000:],z.stderr[-2000:],flush=True);assert z.returncode==0
finally:
 changed=[p for p,h in dict(before,**dynamic).items() if sha(pathlib.Path(p))!=h]
 clean_changed=[n for n,h in clean_manifest.items() if sha(S/n)!=h]
 assert not clean_changed,clean_changed
 deps={}
 for dep in depfiles:
  if dep.exists():
   for n in shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]):
    path=(B/n).resolve()
    if path.is_file():deps[str(path)]=sha(path)
 (O/'results.json').write_text(json.dumps({'scope':'committed ToolOptions/ToolInfo/tools plus one-line clear fix; selected4-unit sanitizer; native transient fixture only; no GUI proof','provider_base':base,'clean_commit':clean_commit,'clean_source_count':len(clean_manifest),'clean_source_changed':clean_changed,'source_sha256':source,'linked_inputs_sha256':before,'linked_inputs_changed':changed,'runtime_dependency_sha256':dynamic,'compiled_dependency_sha256':deps,'results':records},indent=2)+'\n')
 assert not changed,changed
print('PASS causal red; native10 + selected4-unit sanitizer10; linked inputs unchanged')
