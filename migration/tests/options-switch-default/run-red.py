from pathlib import Path
import subprocess,json,shlex,hashlib,os
W=Path('/workspace/shared/editor-switch-default');R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();cmds=json.loads((B/'compile_commands.json').read_text());row=next(x for x in cmds if x['file']=='../source/app/tests/test-gimp-painter-options.cpp');text=(S/'app/tests/test-gimp-painter-options.cpp').read_text().split('static Resource reset_scalar_defaults')[0];text+=(W/'cases-proposal.inc').read_text();text+='\nint main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();g_test_add_func("/switch-default/old-metadata-reset-serialize",switch_default_metadata);return g_test_run();}\n';fixture=W/'fixture.cpp';fixture.write_text(text);c=shlex.split(row['command']);c[c.index('-o')+1]=str(W/'fixture.o');c[c.index('-MF')+1]=str(W/'fixture.o.d');c[-1]=str(fixture)
subprocess.run(c,cwd=B,check=True)
l=shlex.split(next(x for x in reversed(subprocess.check_output(['ninja','-C',str(B),'-t','commands','app/tests/gimp-painter-options'],text=True).splitlines())if ' -o app/tests/gimp-painter-options 'in x));l[l.index('-o')+1]=str(W/'expected-red');l=[str(W/'fixture.o')if x==row['output']else '/workspace/shared/device-name-identity/exact503-libappcore.a'if x=='app/core/libappcore.a'else x for x in l]
for i,x in enumerate(l):
 if x.endswith('.o') and not Path(x).is_absolute()and not(B/x).exists():
  rr=next(v for v in cmds if v.get('output')==x);cc=shlex.split(rr['command']);obj=W/Path(x).name;cc[cc.index('-o')+1]=str(obj);cc[cc.index('-MF')+1]=str(obj)+'.d';subprocess.run(cc,cwd=B,check=True);l[i]=str(obj)
inputs={}
for a in l:
 p=Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==W/'expected-red':continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a'and p.read_bytes()[:8]==b'!<thin>\n':
  for n in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   n=Path(n);n=n if n.is_absolute()else p.parent/n;inputs[str(n.resolve())]=sha(n)
(W/'linked-before.json').write_text(json.dumps(inputs,indent=2)+'\n');(W/'commands.json').write_text(json.dumps({'compile':c,'link':l},indent=2)+'\n');subprocess.run(l,cwd=B,check=True);after={p:sha(Path(p))for p in inputs};assert after==inputs;(W/'linked-after.json').write_text(json.dumps(after,indent=2)+'\n')
t=next(t for t in json.loads((B/'meson-info/intro-tests.json').read_text())if t['name']=='gimp-painter-options');env=os.environ.copy();ld=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+ld
for key,d in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime'),('TMPDIR','tmp')]:
 p=W/'profile'/d;p.mkdir(parents=True,exist_ok=True);env[key]=str(p)
run=['python3',str(S/'tools/run-painter-xvfb.py'),str(W/'expected-red')];p=subprocess.run(run,cwd=B,env=env,capture_output=True,text=True,timeout=90);(W/'expected-red.log').write_text(p.stdout+p.stderr);(W/'expected-red.json').write_text(json.dumps({'exit':p.returncode,'command':run,'binary_sha256':sha(W/'expected-red'),'options_provider_sha256':sha(S/'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'),'scope':'new ordinary Options class-init default/reset/serialization only; no ref_for_context/acquisition probes'},indent=2)+'\n');print(p.returncode);print(p.stdout[-1800:]);print(p.stderr[-500:])
