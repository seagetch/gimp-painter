import json,os,pathlib,subprocess,time,hashlib,sys
B=pathlib.Path('/workspace/shared/gimp-native-baseline/build-port');E=pathlib.Path('/workspace/shared/gimp-native-baseline/retinex-entry-evidence')
t=next(x for x in json.loads((B/'meson-info/intro-tests.json').read_text()) if x['name']=='filter-entry-contracts')
env=os.environ.copy();old=env.get('LD_LIBRARY_PATH','');env.update(t['env']);env['LD_LIBRARY_PATH']+=':'+old
env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1',GIMP_PAINTER_SMALL_TILES_FIXTURES=str(E/'fixtures/small-tiles-evidence'))
exe=B/'app/tests/filter-entry-contracts-asan';start=time.monotonic()
command=[sys.executable,str(B.parent/"source/migration/tests/run_small_tiles_entry_fixture_test.py"),"--",str(exe)]
p=subprocess.run(command,cwd=B,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
(E/'sanitizer-filter-entry-contracts.log').write_text(p.stdout)
(E/'sanitizer-results.json').write_text(json.dumps([dict(name='filter-entry-contracts',command=command,returncode=p.returncode,duration=time.monotonic()-start,timeout=120,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),stdout=p.stdout)],indent=2)+'\n')
raise SystemExit(p.returncode)
