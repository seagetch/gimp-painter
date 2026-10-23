import json,pathlib,os,subprocess,time,hashlib
base=pathlib.Path('<native>');b=base/'build-port';out=base/'layer-dispatch-green'
registry=json.loads((b/'meson-info/intro-tests.json').read_text());results=[]
for name in ['painter-layer-dispatch','painter-layer-ui','painter-canvas-ui']:
 spec=next(r for r in registry if r['name']==name);env=os.environ.copy();env.update(spec['env']);env['LD_LIBRARY_PATH']=spec['env']['LD_LIBRARY_PATH']+':'+os.environ.get('LD_LIBRARY_PATH','');env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 exe=b/('app/tests/'+name+'-asan');start=time.monotonic()
 try:
  r=subprocess.run([str(exe)],cwd=b,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=spec['timeout']);code=r.returncode;log=r.stdout
 except subprocess.TimeoutExpired as e:code=124;log=e.stdout.decode() if isinstance(e.stdout,bytes) else e.stdout or ''
 (out/('sanitizer-'+name+'.log')).write_text(log);report={'name':name,'returncode':code,'timeout':spec['timeout'],'duration':time.monotonic()-start,'passed':sum(l.startswith('ok ') for l in log.splitlines()),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()};results.append(report);(out/'sanitizer-results.json').write_text(json.dumps(results,indent=2)+'\n');print(json.dumps(report),flush=True)
raise SystemExit(any(r['returncode'] for r in results))
