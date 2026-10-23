import pathlib,subprocess,json,shlex,hashlib,os
R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');A=pathlib.Path('/workspace/shared/gimp-clean-aggregate');B=A/'build';S=A/'source';E=pathlib.Path('/workspace/shared/v1-curve-matrix');E.mkdir(exist_ok=True)
fixture='app/paint/painter-mypaint/tests/test-resource.cpp';(E/'test-resource.cpp').write_bytes((R/fixture).read_bytes())
names=['app/paint/painter-mypaint/resource.cpp','app/paint/painter-mypaint/engine.cpp',fixture]
hashes={str(p.relative_to(S)):hashlib.sha256(p.read_bytes()).hexdigest() for base in ['app/paint/painter-mypaint','app/painter'] for p in (S/base).rglob('*') if p.is_file() and p.suffix in ['.hpp','.h','.cpp']};hashes[fixture]=hashlib.sha256((E/'test-resource.cpp').read_bytes()).hexdigest()
(E/'before-source.json').write_text(json.dumps(hashes,indent=2)+'\n')
entries=json.load(open(B/'compile_commands.json'));link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/paint/painter-mypaint/painter-mypaint-resource'],cwd=B,text=True).splitlines()[-1]);commands=[]
for variant in ['normal','asan']:
 objs=[];flags=['-O1']+(['-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize=vptr','-fno-omit-frame-pointer']if variant=='asan'else[])
 for name in names:
  e=next(e for e in entries if (pathlib.Path(e['directory'])/e['file']).resolve()==S/name)
  cmd=shlex.split(e['command']);out=[];skip=False
  for x in cmd:
   if skip:skip=False;continue
   if x in ['-MF','-MT','-MQ']:skip=True;continue
   if x in ['-MD','-MMD']:continue
   out.append(x)
  obj=E/(variant+'-'+pathlib.Path(name).name+'.o');out[out.index('-o')+1]=str(obj)
  if name==fixture:out[out.index(e['file'])]=str(E/'test-resource.cpp')
  out+=flags;commands.append(out);subprocess.run(out,cwd=B,check=True);objs.append(str(obj))
 cmd=[x for x in link if x not in ['app/paint/painter-mypaint/painter-mypaint-resource.p/tests_test-resource.cpp.o','app/paint/painter-mypaint/libpainter-mypaint.a']];cmd[cmd.index('-o')+1]=str(E/variant);cmd[1:1]=flags+objs;commands.append(cmd);subprocess.run(cmd,cwd=B,check=True)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 run=subprocess.run([str(E/variant),str(S/'data/painter-mypaint-brushes')],env=env,text=True,capture_output=True,timeout=60)
 (E/(variant+'.json')).write_text(json.dumps({'returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr},indent=2)+'\n');print(variant,run.returncode,run.stdout[-1400:],flush=True);assert run.returncode==0
(E/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
for n,h in hashes.items():assert hashlib.sha256(((E/'test-resource.cpp')if n==fixture else S/n).read_bytes()).hexdigest()==h,n
(E/'post-source-seal.json').write_text(json.dumps({'changed':[],'checked':len(hashes)})+'\n')
