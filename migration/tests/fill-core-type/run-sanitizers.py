from pathlib import Path
import json,hashlib,subprocess,os,re
rec=Path(__file__).resolve().parent
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree')
binary=rec/'sanitizer-private/gimp-fill-brush-asan'
build=json.loads((rec/'sanitizer-build.json').read_text())
seals=json.loads((rec/'native.json').read_text())['source_sha256']
for p,h in seals.items():assert hashlib.sha256((root/p).read_bytes()).hexdigest()==h,p
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
cases=['/fill-core/'+s for s in ['registered-type-parent-contract','parent-start-failure-retry','standalone-parent-lifetime','parent-start-notification-lifetime','generic-start-gate','dispose-rollback','caller-core-loss','pending-query-lifetime']]+['/owned-generic/reentry-abort-loss','/owned-generic/finish-boundary']
results=[]
for case in cases:
 run=subprocess.run([str(binary),'-p',case],cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=180)
 results.append({'case':case,'exit_code':run.returncode,'stdout':run.stdout});print(case,run.returncode,flush=True)
 (rec/('sanitizer-'+case.replace('/','_')+'.log')).write_text(run.stdout)
changed=[p for p,h in seals.items() if hashlib.sha256((root/p).read_bytes()).hexdigest()!=h]
status='PASS' if len(results)==10 and not changed and all(r['exit_code']==0 and len(re.findall(r'^ok \d+ ',r['stdout'],re.M))==1 for r in results) else 'FAIL'
(rec/'sanitizer.json').write_text(json.dumps({'status':status,'results':results,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'source_sha256':seals,'changed_during_run':changed,'build_report_sha256':hashlib.sha256((rec/'sanitizer-build.json').read_bytes()).hexdigest(),'ASAN_OPTIONS':env['ASAN_OPTIONS'],'UBSAN_OPTIONS':env['UBSAN_OPTIONS'],'scope':'Focused original08.011 core type/parent lifetime; no LeakSanitizer or full-dependency coverage'},indent=2)+'\n')
assert status=='PASS'
