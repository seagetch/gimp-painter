import json,os,pathlib,subprocess,time,hashlib
B=pathlib.Path('/workspace/shared/gimp-native-baseline/build-port');E=pathlib.Path('/workspace/shared/gimp-native-baseline/bucket-fill-entry-evidence')
t=next(x for x in json.loads((B/'meson-info/intro-tests.json').read_text()) if x['name']=='bucket-fill-brush-entry')
env=os.environ.copy();old=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+old
env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',GIMP_TEST_BUCKET_FILL_SCREENSHOT=str(E/'portrait-oncanvas-sanitizer.png'))
exe=B/'app/tests/bucket-fill-brush-entry-asan';start=time.monotonic();command=[str(exe)]
p=subprocess.run(command,cwd=B,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60)
(E/'sanitizer-bucket-fill-brush-entry.log').write_text(p.stdout)
(E/'sanitizer-results.json').write_text(json.dumps([dict(name='bucket-fill-brush-entry',command=command,returncode=p.returncode,duration=time.monotonic()-start,timeout=60,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),stdout=p.stdout)],indent=2)+'\n')
raise SystemExit(p.returncode)
