import json,os,subprocess,time,hashlib,re
from pathlib import Path
build=Path('/workspace/shared/gimp-native-baseline/build-port');out=Path('/workspace/shared/gimp-tool-group-retirement')
reg=next(r for r in json.loads((build/'meson-info/intro-tests.json').read_text()) if r['name']=='painter-perspective-ui')
env=os.environ.copy();libs=env.get('LD_LIBRARY_PATH','');env.update(reg['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+libs
env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1:abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
exe=build/'app/tests/painter-perspective-ui-tool-groups-asan';start=time.monotonic()
try:
 r=subprocess.run([str(exe)],env=env,cwd=build,text=True,capture_output=True,timeout=reg['timeout']);code=r.returncode;stdout=r.stdout;stderr=r.stderr
except subprocess.TimeoutExpired as e:
 code='timeout';stdout=e.stdout or b'';stderr=e.stderr or b''
 if isinstance(stdout,bytes):stdout=stdout.decode(errors='replace')
 if isinstance(stderr,bytes):stderr=stderr.decode(errors='replace')
(out/'asan.stdout').write_text(stdout);(out/'asan.stderr').write_text(stderr)
result={'exit_code':code,'duration_seconds':time.monotonic()-start,'timeout_seconds':reg['timeout'],'tap_ok':len(re.findall(r'^ok ',stdout,re.M)),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'display_geometry':'1080x1920x24','whole_application_instrumented':False,'leak_detection':False}
(out/'asan-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result));raise SystemExit(0 if code==0 and result['tap_ok']==24 else 1)
