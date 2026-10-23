import json,os,subprocess,time,hashlib
from pathlib import Path
build=Path('/workspace/shared/gimp-native-baseline/build-port');out=Path('/workspace/shared/gimp-native-baseline/mybrush-filename-evidence/asan')
registry=json.loads((build/'meson-info/intro-tests.json').read_text()); report={'results':[],'display_geometry':'1080x1920x24','whole_application_instrumented':False,'leak_detection':False}
for target in ('gimp-painter-mybrush',):
 reg=next(r for r in registry if r['name']==target);env=os.environ.copy(); libs=env.get('LD_LIBRARY_PATH','');env.update(reg['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+libs
 env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1:abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 cmd=[str(build/'app/tests'/f'{target}-asan')];start=time.monotonic()
 try:
  r=subprocess.run(cmd,cwd=build,env=env,text=True,capture_output=True,timeout=reg['timeout']); code=r.returncode; stdout=r.stdout;stderr=r.stderr
 except subprocess.TimeoutExpired as e:
  code='timeout';stdout=e.stdout or b'';stderr=e.stderr or b''
  if isinstance(stdout,bytes):stdout=stdout.decode(errors='replace')
  if isinstance(stderr,bytes):stderr=stderr.decode(errors='replace')
 (out/(target+'.stdout')).write_text(stdout);(out/(target+'.stderr')).write_text(stderr)
 row={'target':target,'exit_code':code,'duration_seconds':time.monotonic()-start,'timeout_seconds':reg['timeout'],'tap_ok':sum(l.startswith('ok ') for l in stdout.splitlines()),'executable_sha256':hashlib.sha256(Path(cmd[0]).read_bytes()).hexdigest()};report['results'].append(row);print(json.dumps(row),flush=True)
 (out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if all(r['exit_code']==0 for r in report['results']) else 1)
