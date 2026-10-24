from pathlib import Path
import json,subprocess,shlex,hashlib,os,sys
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/unsafe-undo-query');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();C=json.loads((B/'compile_commands.json').read_text());test=sys.argv[1];out=W/test;san=bool(os.environ.get('PROOF_ASAN'));out=W/(('asan-' if san else '')+test)
if test=='painter-mypaint-tool':
 row=next(c for c in C if c['file']=='../source/app/tests/test-painter-mypaint-tool.cpp');cmd=shlex.split(row['command']);cmd[1:1]=['-I'+str(W/'source/app'),'-iquote',str(S/'app/core'),'-iquote',str(S/'app/tests')];cmd[-1]=str(R/'app/tests/test-painter-mypaint-tool.cpp');cmd[cmd.index('-o')+1]=str(W/'tool-fixture.o');cmd[cmd.index('-MF')+1]=str(W/'tool-fixture.o.d');subprocess.run(cmd,cwd=B,check=True)
if test=='undo-pop-guard':
 row=next(c for c in C if c['file']=='../source/app/tests/test-undo-pop-guard.c');cmd=shlex.split(row['command']);cmd[1:1]=['-I'+str(W/'source/app'),'-iquote',str(S/'app/core')];cmd[-1]=str(R/'app/tests/test-undo-pop-guard.c');cmd[cmd.index('-o')+1]=str(W/'fixture.o');cmd[cmd.index('-MF')+1]=str(W/'fixture.o.d');subprocess.run(cmd,cwd=B,check=True)
lines=subprocess.check_output(['ninja','-C',str(B),'-t','commands','app/tests/'+test],text=True).splitlines();l=shlex.split(next(s for s in reversed(lines)if ' -o app/tests/'+test+' ' in s));l[l.index('-o')+1]=str(out);l=[str(W/'fixture.o') if test=='undo-pop-guard' and x.endswith('/test-undo-pop-guard.c.o') else str(W/'tool-fixture.o') if test=='painter-mypaint-tool' and x.endswith('/test-painter-mypaint-tool.cpp.o') else '/workspace/shared/device-name-identity/exact503-libappcore.a' if x=='app/core/libappcore.a' else x for x in l];l[1:1]=[str(W/(('asan-' if san else '')+p+'.o')) for p in ['gimpimage.c','gimpimage-undo.c','paint-core.cpp','gimppainterpaintgate.cpp','gimppaintermybrushtool.cpp']]
# Build missing fixture objects privately, never mutate the clean checkpoint.
for idx,a in enumerate(l):
 if a.endswith('.o') and not (B/a).is_file() and not Path(a).is_absolute():
  row=next(c for c in C if c.get('output')==a);cmd=shlex.split(row['command']);obj=W/(test+'-'+Path(a).name);cmd[cmd.index('-o')+1]=str(obj);cmd[cmd.index('-MF')+1]=str(obj)+'.d';subprocess.run(cmd,cwd=B,check=True);l[idx]=str(obj)
if san:l[1:1]=['-fsanitize=address,undefined'];env_dummy=None
inputs={}
for a in l:
 p=Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file() or p==out:continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a' and p.read_bytes()[:8]==b'!<thin>\n':
  for n in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   n=Path(n);n=n if n.is_absolute()else p.parent/n;inputs[str(n.resolve())]=sha(n)
(W/(('asan-' if san else '')+test+'-inputs.json')).write_text(json.dumps(inputs,indent=2)+'\n');(W/(('asan-' if san else '')+test+'-link.json')).write_text(json.dumps(l,indent=2)+'\n');subprocess.run(l,cwd=B,check=True);assert all(sha(Path(p))==h for p,h in inputs.items())
t=next(t for t in json.loads((B/'meson-info/intro-tests.json').read_text())if t['name']==test);env=os.environ.copy();ld=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['GIMP_TESTING_ABS_TOP_SRCDIR']=str(S);env['GIMP_TESTING_ABS_TOP_BUILDDIR']=str(B);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+ld;env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1';env['UI_TEST']='yes';env['GIMP_TEST_PAINTER_ONCANVAS']='1'
for k,d in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
 p=W/'profile'/d;p.mkdir(parents=True,exist_ok=True);env[k]=str(p)
cmd=['python3',str(S/'tools/run-painter-xvfb.py'),str(out)]+sys.argv[2:];p=subprocess.run(cmd,env=env,cwd=B,capture_output=True,text=True,timeout=200);(W/(('asan-' if san else '')+test+'.log')).write_text(p.stdout+p.stderr);(W/(('asan-' if san else '')+test+'-result.json')).write_text(json.dumps({'command':cmd,'exit':p.returncode,'binary_sha256':sha(out)},indent=2)+'\n');print(test,p.returncode,(p.stdout+p.stderr)[-1200:]);sys.exit(p.returncode)
