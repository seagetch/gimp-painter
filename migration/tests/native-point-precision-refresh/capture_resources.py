import hashlib,json,os,re,resource,subprocess,sys,time
from pathlib import Path
E=Path(os.environ.get('GIMP_POINT_EVIDENCE', Path(__file__).resolve().parent)); E.mkdir(parents=True,exist_ok=True)
B=Path(os.environ.get('GIMP_POINT_BUILD', E.parent/'build-port')); S=Path(os.environ.get('GIMP_POINT_SOURCE', E.parent/'source'))
mode=sys.argv[1]
test=next(x for x in json.loads((B/'meson-info/intro-tests.json').read_text()) if x['name']=='gimp-filter-layer')
env=os.environ.copy(); old=env.get('LD_LIBRARY_PATH',''); env.update(test['env']); env['LD_LIBRARY_PATH']+=':'+old
env['GSETTINGS_BACKEND']='memory'
if mode in ('normal','large-asan-direct','core-normal','core-asan'): cmd=[str(B/'app/tests/gimp-filter-layer'),'-p','/gimp-filter-layer/large_spill_execution_finishes']
else: cmd=[sys.executable,'-B',str(S/'migration/tests/run_filter_layer_sanitizers.py'),str(B),'--report',str(E/'large-asan-build-and-test.json'),'--test-path','/gimp-filter-layer/large_spill_execution_finishes']
if mode in ('large-asan-direct', 'core-asan'):
 cmd[0] += '-asan'
 env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
if mode.startswith('core-'):
 cmd=cmd[:1]
 for case in ('genuine_point_filter_transfer','native_precision_loaded_cache_and_rerun','native_precision_point_and_gaussian_transfer','native_precision_spill_transfer','spill_gaussian_native_gray','spill_gaussian_native_rgb','gaussian_alias_and_negative_fixtures','gaussian_alias_failures_preserve_cache','gaussian_alias_identity_spills'):
  cmd+=['-p','/gimp-filter-layer/'+case]
names=['app/tests/test-gimp-filter-layer.c','app/core/gimpfilterlayer.cpp','app/painter/filter-scheduler.cpp','app/painter/filter-spool.cpp','app/config/gimpgeglconfig.c']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before={n:sha(S/n) for n in names}
def memory():
 return {a:int(b.split()[0]) for a,b in (x.split(':',1) for x in Path('/proc/meminfo').read_text().splitlines()) if a in ('MemTotal','MemAvailable','SwapTotal','SwapFree','Shmem')}
def children(pid):
 result=[pid]
 try:
  for c in Path('/proc/%d/task/%d/children'%(pid,pid)).read_text().split():result+=children(int(c))
 except (FileNotFoundError,ProcessLookupError):pass
 return result
def sample(pid):
 total=0; details=[]
 for p in children(pid):
  try:
   d={a:b.strip() for a,b in (x.split(':',1) for x in Path('/proc/%d/status'%p).read_text().splitlines())}
   rss=int(d.get('VmRSS','0 kB').split()[0]); total+=rss
   details.append({'name':d['Name'],'rss_kib':rss})
  except (FileNotFoundError,ProcessLookupError):pass
 return total,details
# Build orchestration has its own bound; selected executable runs keep the
# existing focused helper's 180-second runtime watchdog.
limit_seconds=1200 if mode=='asan' else 180
start=time.monotonic(); initial=memory();peak=0; peakdetails=[]; samples=[]
with (E/(mode+'.log')).open('w') as log:
 p=subprocess.Popen(cmd,cwd=B,env=env,stdout=log,stderr=subprocess.STDOUT)
 while p.poll() is None:
  rss,details=sample(p.pid)
  if rss>peak:peak=rss;peakdetails=details
  samples.append([round(time.monotonic()-start,3),rss,memory()['MemAvailable']])
  if time.monotonic()-start>limit_seconds:p.terminate(); raise RuntimeError('bounded runner timeout')
  time.sleep(.2)
 rc=p.wait()
report={'mode':mode,'watchdog_seconds':limit_seconds,'command':cmd,'exit_code':rc,'elapsed_seconds':time.monotonic()-start,'initial_memory_kib':initial,'final_memory_kib':memory(),'peak_sampled_process_tree_rss_kib':peak,'peak_processes':peakdetails,'rusage_children_maxrss_kib':resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,'samples_seconds_rss_kib_available_kib':samples,'source_sha256':before,'sources_changed':[n for n in names if sha(S/n)!=before[n]],'log_sha256':sha(E/(mode+'.log')),'selected_test_paths':cmd[2::2] if mode!='asan' else ['/gimp-filter-layer/large_spill_execution_finishes'],'executable_sha256':sha(Path(cmd[0])),'limits':['Only named source hashes and process memory counters captured; no inherited environment.','RSS excludes file-system cache; sampled tree aggregate may count shared pages more than once.','Unmodified selected native tests/assertions; default GEGL cache and existing build/filter-spill-cache; no memory limiting.']}
log_text = (E/(mode+'.log')).read_text()
report['executed_paths'] = re.findall(r'^ok \d+ (/\S+)', log_text, re.M)
report['skipped_lines'] = re.findall(r'^ok .*#\s*SKIP.*$', log_text, re.M)
report['sanitizer_diagnostics'] = [line for line in log_text.splitlines() if any(marker in line for marker in ('ERROR: AddressSanitizer', 'runtime error:', 'AddressSanitizer:DEADLYSIGNAL'))]
report['passed'] = (rc == 0 and sorted(report['executed_paths']) == sorted(report['selected_test_paths']) and not report['skipped_lines'] and not report['sanitizer_diagnostics'] and not report['sources_changed'])
(E/(mode+'-resources.json')).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('samples_seconds_rss_kib_available_kib','source_sha256')},indent=2))
sys.exit(0 if report['passed'] else (rc if rc>0 else 1))
