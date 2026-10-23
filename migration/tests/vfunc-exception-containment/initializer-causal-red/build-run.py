import subprocess,shlex,json,os,pathlib,time
b=pathlib.Path('<native>/build-port');out=pathlib.Path('<native>/vfunc-initializer-red'); db=json.loads((b/'compile_commands.json').read_text())
for original,source in [('test-painter-widget-exceptions.c','test.c'),('test-painter-widget-exception-fault.cpp','fault.cpp')]:
 r=next(r for r in db if r['file'].endswith('/'+original));cmd=shlex.split(r['command']);obj=str(out/(source+'.o'))
 for flag in ['-o','-MF','-MQ']:
  if flag in cmd:cmd[cmd.index(flag)+1]=obj+('.d' if flag=='-MF' else '')
 cmd[-1]=str(out/source)
 with (out/(source+'.compile.log')).open('w') as f:subprocess.run(cmd,cwd=b,stdout=f,stderr=subprocess.STDOUT,check=True)
cmds=subprocess.check_output(['ninja','-t','commands','app/tests/painter-widget-exceptions'],cwd=b,text=True).splitlines();args=shlex.split(next(c for c in reversed(cmds) if ' -o app/tests/painter-widget-exceptions ' in c));args[args.index('-o')+1]=str(out/'test-init')
args=[str(out/'test.c.o') if a.endswith('/test-painter-widget-exceptions.c.o') else str(out/'fault.cpp.o') if a.endswith('/test-painter-widget-exception-fault.cpp.o') else a for a in args]
args=[a.replace('_ZNK11GimpPainter12BindingStore12check_threadEv','_ZN11GimpPainter12BindingStore6ensureEP8_GObject') for a in args]
with (out/'link.log').open('w') as f:subprocess.run(args,cwd=b,stdout=f,stderr=subprocess.STDOUT,check=True)
(out/'link-command.json').write_text(json.dumps(args,indent=2))
spec=next(r for r in json.loads((b/'meson-info/intro-tests.json').read_text()) if r['name']=='painter-widget-exceptions');env=os.environ.copy();env.update(spec['env']);env['LD_LIBRARY_PATH']=spec['env']['LD_LIBRARY_PATH']+':'+os.environ.get('LD_LIBRARY_PATH','');results=[]
for kind in ['typed','standard','allocation','unknown','raw-unknown']:
 start=time.monotonic();r=subprocess.run([str(out/'test-init'),'-p','/vfunc-init/'+kind],cwd=b,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=spec['timeout']);(out/(kind+'.log')).write_text(r.stdout);results.append({'kind':kind,'returncode':r.returncode,'duration':time.monotonic()-start});print(kind,r.returncode,r.stdout[-600:],flush=True)
(out/'results.json').write_text(json.dumps(results,indent=2))
