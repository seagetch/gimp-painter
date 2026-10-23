import hashlib,json,os,subprocess,time
from pathlib import Path
root=Path('/workspace/shared/gimp-native-baseline/source'); build=root.parent/'build-port'; out=Path('/workspace/shared/gimp-editor-page-asan')
registry=json.loads((build/'meson-info/intro-tests.json').read_text()); registration=next(t for t in registry if t['name']=='painter-mypaint-editor')
report={'scope':'Focused editor ASan/UBSan/float-cast-overflow; remaining libraries/app units uninstrumented or RTTI compatibility only; leak detection disabled','results':[],'source_sha256':{}}
for name in ['app/widgets/gimppaintermybrusheditor.cpp','app/tests/test-painter-mypaint-editor.cpp']:
 report['source_sha256'][name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
for mode in ('existing-25','page-memory-3'):
 env=os.environ.copy(); old_lib=env.get('LD_LIBRARY_PATH',''); env.update(registration['env']); env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+old_lib
 env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1:abort_on_error=1'; env['UBSAN_OPTIONS']='halt_on_error=1:abort_on_error=1:print_stacktrace=1'
 if mode=='page-memory-3':env['GIMP_TEST_EDITOR_PAGE_MEMORY']='1'
 else:env.pop('GIMP_TEST_EDITOR_PAGE_MEMORY',None)
 cmd=[str(build/'app/tests/painter-mypaint-editor-asan')]
 started=time.monotonic()
 try:
  result=subprocess.run(cmd,env=env,cwd=build,capture_output=True,text=True,timeout=registration['timeout']); code=result.returncode; stdout=result.stdout; stderr=result.stderr
 except subprocess.TimeoutExpired as e:
  code='timeout'; stdout=e.stdout or b''; stderr=e.stderr or b''
  if isinstance(stdout,bytes):stdout=stdout.decode(errors='replace')
  if isinstance(stderr,bytes):stderr=stderr.decode(errors='replace')
 (out/(mode+'.stdout')).write_text(stdout); (out/(mode+'.stderr')).write_text(stderr)
 row={'mode':mode,'exit_code':code,'duration_seconds':time.monotonic()-started,'timeout_seconds':registration['timeout'],'command':cmd,'tap_ok':sum(l.startswith('ok ') for l in stdout.splitlines())}
 report['results'].append(row); print(json.dumps(row),flush=True)
report['source_hashes_unchanged']=all(hashlib.sha256((root/n).read_bytes()).hexdigest()==h for n,h in report['source_sha256'].items())
(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if report['source_hashes_unchanged'] and all(r['exit_code']==0 for r in report['results']) else 1)
