#!/usr/bin/env python3
"""Caller holds frozen clean build lock; only private outputs are modified."""
import hashlib,json,os,pathlib,shlex,subprocess,time
R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');O=pathlib.Path(__file__).resolve().parent
B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
base=json.loads((O/'audit.json').read_text())['base']
phase=__import__('sys').argv[1]
assert phase in ('red','green')
seal=json.loads(pathlib.Path('/workspace/shared/filter-attributes-proof/source-after.json').read_text())
predecessor=json.loads(pathlib.Path('/workspace/shared/preset-help-successor/green/source-manifest.json').read_text())
clean_manifest=predecessor['complete_source_sha256'].copy();clean_manifest.update(seal['sha256'])
seal['complete_source_sha256']=clean_manifest
assert all(sha(S/n)==h for n,h in clean_manifest.items()),'clean source differs from released Filter seal'
clean_commit=seal['committed_base']
(O/(phase+'-clean-source-manifest.json')).write_text(json.dumps(seal,sort_keys=True,indent=2)+'\n')
fixture='app/tests/test-gimp-painter-surface.cpp';tools='app/core/gimp-user-install.c'
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/gimp-painter-surface'],cwd=B,text=True).strip().splitlines()[-1]);entries=json.loads((B/'compile_commands.json').read_text())
archives={};linked=set();tracked=set()
for x in link:
 p=(B/x).resolve()
 if x.endswith('.o'):linked.add(str(p));tracked.add(p)
 if x.endswith('.a') and p.is_file():
  members=([str((B/n).resolve())for n in subprocess.check_output(['ar','t',x],cwd=B,text=True).splitlines()] if p.read_bytes()[:8]==b'!<thin>\n' else []);archives[x]=members;linked.update(members);tracked.add(p);tracked.update(pathlib.Path(n)for n in members)
 if '.so' in x and p.is_file():tracked.add(p)
selected=['app/core/gimppainterprofile.cpp']
# Compile the exact committed providers rather than trusting unrelated overlays.
for n in selected:
 p=O/n.replace('/','_');p.write_bytes(subprocess.check_output(['git','show',base+':'+n],cwd=R))
source={n:sha(O/n.replace('/','_')) for n in selected}
source.update({'test.cpp':sha(O/'test.cpp'),'before-install':sha(O/'before.c'),'test-helper':sha(O/'helper.c')})
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
  provider=O/('test.cpp' if n==fixture else ('after-with-helper.c' if after else 'before-with-helper.c') if n==tools else n.replace('/','_'))
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

def execute(mode,exe,case):
 env=os.environ.copy();profile=O/('profile-'+mode+'-'+case)
 assert not profile.exists(),profile
 home=profile/('home spaced 日本語' if case=='unicode' else 'home')
 for key,p in [('HOME',home),('XDG_CONFIG_HOME',profile/'config'),('XDG_CACHE_HOME',profile/'cache'),('XDG_DATA_HOME',profile/'data'),('XDG_STATE_HOME',profile/'state'),('XDG_RUNTIME_DIR',profile/'runtime'),('TMPDIR',profile/'tmp')]:
  p.mkdir(parents=True,exist_ok=True);p.chmod(0o700);env[key]=str(p)
 painter=home/'.gimp-painter-2.8';standard=home/'.gimp-2.8';xdg=profile/'config/GIMP/2.10';target=profile/'config/GIMP/3.0';expected=painter;minor=8
 if case in ('missing','regular-file'):expected=None
 if case=='regular-file':painter.write_text('not a directory')
 if case not in ('missing','regular-file'):painter.mkdir()
 if case=='standard-precedence':standard.mkdir();expected=standard
 if case=='xdg-precedence':
  xdg.mkdir(parents=True);standard.mkdir();expected=xdg;minor=10
 if case=='explicit-precedence':
  xdg.mkdir(parents=True);target=profile/'explicit/3.0';expected=profile/'explicit/2.10';expected.mkdir(parents=True);minor=10
 if case=='migrate-retry':
  (painter/'tool-options').mkdir();(painter/'tool-options/gimp-mypaint-tool').write_text('(stroke-opacity .37)\n')
  (painter/'gimprc').write_text('(default-mypaint-brush "legacy preserved brush") (global-mypaint-brush no)\n')
 if case=='migrate-standard':(painter/'gimprc').write_text('(default-brush "ordinary standard brush")\n')
 sentinel=home/'unrelated-note';sentinel.write_text('must remain unchanged')
 preserved={str(p):sha(p) for p in profile.rglob('*') if p.is_file()}
 private_build=profile/'build';(private_build/'app/tests/gimpdir-output').mkdir(parents=True)
 env.update(GIMP3_DIRECTORY=str(target),PROFILE_TARGET=str(target),EXPECTED_SOURCE=str(expected) if expected else '',EXPECTED_MINOR=str(minor),PROFILE_CASE=case,GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(private_build),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',LC_ALL='C.UTF-8')
 for key in ('SNAP','SNAP_USER_DATA'):env.pop(key,None)
 trace=run(mode+'-'+case+'-loader',[str(exe)],dict(env,LD_TRACE_LOADED_OBJECTS='1'),30)
 for line in trace.stdout.splitlines():
  for token in line.split():
   if token.startswith('/') and pathlib.Path(token).is_file():dynamic.setdefault(token,sha(pathlib.Path(token)))
 result=run(mode+'-'+case+'-run',[str(exe)],env,240)
 assert all(pathlib.Path(p).is_file() and sha(pathlib.Path(p))==h for p,h in preserved.items()),'fixture source/sentinel changed'
 records[-1]['preserved_fixture_sha256']=preserved
 return result

try:
 if phase=='red':
  exe=build('red',[],False);z=execute('red',exe,'lone-painter');print('RED',z.returncode,z.stdout,z.stderr,flush=True)
  assert z.returncode!=0 and '(selected == expected)' in z.stdout+z.stderr and 'NULL' in z.stdout+z.stderr
 else:
  cases=['missing','regular-file','lone-painter','standard-precedence','xdg-precedence','explicit-precedence','unicode','migrate-retry','migrate-standard']
  for mode,flags in [('normal',[]),('asan',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
   exe=build(mode,flags,True)
   for case in cases:
    z=execute(mode,exe,case);print(mode,case,z.returncode,z.stdout[-1000:],z.stderr[-1000:],flush=True);assert z.returncode==0

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
 (O/(phase+'-results.json')).write_text(json.dumps({'scope':'private committed installer/profile providers; selected3-unit sanitizer; isolated HOME discovery/migrate/retry; no GUI proof','provider_base':base,'clean_commit':clean_commit,'clean_source_count':len(clean_manifest),'clean_source_changed':clean_changed,'source_sha256':source,'linked_inputs_sha256':before,'linked_inputs_changed':changed,'runtime_dependency_sha256':dynamic,'compiled_dependency_sha256':deps,'results':records},indent=2)+'\n')
 assert not changed,changed
print('PASS',phase,'linked inputs/source unchanged')
