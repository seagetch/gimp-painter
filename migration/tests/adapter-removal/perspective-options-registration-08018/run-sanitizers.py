import json,os,pathlib,subprocess,time,hashlib
B=pathlib.Path('/workspace/shared/gimp-native-baseline/build-port'); E=pathlib.Path('/workspace/shared/gimp-native-baseline/guide-options-evidence')
entries=json.loads((B/'meson-info/intro-tests.json').read_text()); results=[]
for name in ['painter-perspective','painter-perspective-ui']:
 t=next(x for x in entries if x['name']==name);env=os.environ.copy();old=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['LD_LIBRARY_PATH']+=':'+old
 env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 exe=B/'app/tests'/f'{name}-asan'; start=time.monotonic()
 p=subprocess.run([str(exe)],cwd=B,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
 (E/f'sanitizer-{name}.log').write_text(p.stdout)
 results.append({'name':name,'returncode':p.returncode,'duration':time.monotonic()-start,'timeout':60,'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'stdout':p.stdout})
 (E/'sanitizer-results.json').write_text(json.dumps(results,indent=2)+'\n')
 if p.returncode:raise SystemExit(p.returncode)
