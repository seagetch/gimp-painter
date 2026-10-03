import fcntl,hashlib,json,os,re,subprocess
from pathlib import Path
root=Path('/workspace/scratch/5b5281e79681/gimp-painter'); build=root/'build-debian13'; overlay=build/'filter-process-sanitizers'
source_report=json.loads((overlay/'report.json').read_text()); names=['image_close_during_worker','retained_handle_after_image_close']
output=root/'migration/tests/filter-process-checkpoint/foundation-sanitizers.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
with Path('/workspace/shared/gimp-painter-build.lock').open('a') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 sources=source_report['source_sha256']; before={p:sha(root/p) for p in sources}
 if before!=sources:raise RuntimeError('Sealed overlay source no longer matches current source')
 exe=overlay/'gimp-filter-layer'; binary=sha(exe)
 if binary!=source_report['executable_sha256']['gimp-filter-layer']:raise RuntimeError('Sealed overlay executable changed')
 env=os.environ.copy();env['LD_LIBRARY_PATH']=os.pathsep.join(str(p) for p in build.glob('libgimp*') if p.is_dir())+os.pathsep+env.get('LD_LIBRARY_PATH','')
 env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(root),GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),GIMP_TESTING_PLUGINDIRS=str(build/'plug-ins/common'),GSETTINGS_BACKEND='memory',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 rows=[]
 for name in names:
  command=[str(exe),'-p','/gimp-filter-layer/'+name]
  r=subprocess.run(command,cwd=build,env=env,text=True,capture_output=True,timeout=90)
  okay=r.returncode==0 and re.findall(r'^ok \d+ (\S+)$',r.stdout,re.M)==['/gimp-filter-layer/'+name]
  rows.append(dict(name=name,command=command,exit_code=r.returncode,passed=okay,stdout=r.stdout,stderr=r.stderr));print(name,'PASS' if okay else 'FAIL',flush=True)
 after={p:sha(root/p) for p in sources};changed=[p for p in before if before[p]!=after[p]]
 report=dict(status='PASS' if all(r['passed'] for r in rows) and not changed and binary==sha(exe) else 'FAIL',scope=source_report['scope'],source_sha256=before,source_sha256_after=after,changed_during_run=changed,executable_sha256=binary,results=rows,stdout='\n'.join(r['stdout'] for r in rows),sanitizer_build_report='migration/tests/filter-process-checkpoint/sanitizers.json')
 output.write_text(json.dumps(report,indent=2)+'\n')
 raise SystemExit(0 if report['status']=='PASS' else 1)
