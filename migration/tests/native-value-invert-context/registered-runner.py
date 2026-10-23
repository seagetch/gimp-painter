#!/usr/bin/env python3
"""Bounded exact-registered-target regression evidence; never a full aggregate."""
import importlib.util,json,os,re,signal,subprocess,sys,time
from pathlib import Path
SOURCE=Path('/workspace/shared/gimp-painter-value-invert-owner')
BUILD=Path('/workspace/shared/gimp-value-invert-owner-build')
OUTPUT=Path('/workspace/shared/gimp-value-invert-owner-regression-3.json')
sys.path.insert(0,str(SOURCE/'migration/tests'))
from run_sharpen_context_sanitizers import seals,reseal,verify_lock,build_library_path
spec=importlib.util.spec_from_file_location('strict_aggregate', SOURCE/'tools/run-painter-frozen-aggregate.py')
strict=importlib.util.module_from_spec(spec);spec.loader.exec_module(strict)
NAMES=('gimp-filter-layer','painter-xcf-open','painter-xcf-roundtrip',
       'painter-filter-scheduler','painter-filter-owner-gates','painter-filter-native-kernels',
       'painter-filter-context-native','painter-filter-point-shadow','painter-filter-sharpen',
       'painter-filter-sharpen-shadow','painter-filter-sobel','painter-filter-context',
       'painter-filter-import-completion')
registry=json.loads((BUILD/'meson-info/intro-tests.json').read_text())
selected=[]
for name in NAMES:
 rows=[r for r in registry if r['name']==name]
 if len(rows)!=1:raise RuntimeError('Missing/duplicate registered target '+name)
 selected+=rows
report={'status':'running','scope':'Thirteen exact registered affected regression targets, unfiltered, clean retry after full default build; both failed prerequisite attempts retained; not full aggregate',
        'source_commit':strict.git(SOURCE,'rev-parse','HEAD'),'source_tree':strict.git(SOURCE,'rev-parse','HEAD^{tree}'),
        'registry':[{k:r[k] for k in ('name','cmd','timeout','workdir','protocol','suite') if k in r} for r in selected],
        'tests':[]}
if OUTPUT.exists():raise RuntimeError('Fresh report required')
def save():OUTPUT.write_text(json.dumps(report,indent=2)+'\n')
before={};save()
try:
 report['lock']=verify_lock(9,Path('/workspace/shared/gimp-painter-build.lock'))
 names=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=SOURCE).decode().split('\0')
 before['source']=seals(SOURCE/n for n in names if n and (SOURCE/n).is_file())
 binaries={BUILD/'app/tests'/n for n in NAMES[:3]}|{BUILD/'app/painter'/n for n in NAMES[3:]}
 allowlist=sorted(re.findall(r'case FilterProcedure::\w+: return "([^"]+)";', (SOURCE/'app/core/gimpfilterpaths.cpp').read_text()))
 if allowlist!=sorted(['blinds','tile-small','contrast-retinex','convolution-matrix']):raise RuntimeError('Fixed worker allowlist changed')
 runtime={BUILD/'app/gimp-painter-filter-worker'}|{BUILD/'plug-ins/common'/name for name in allowlist}
 targets=json.loads((BUILD/'meson-info/intro-targets.json').read_text())
 for name in (*NAMES,'gimp-painter-filter-worker',*allowlist):
  rows=[x for x in targets if x['name']==name and x['type']=='executable']
  if len(rows)!=1:raise RuntimeError('Missing or ambiguous executable declaration '+name)
  runtime.update(Path(x) for x in rows[0]['filename'])
 report['runtime_preflight']={'fixed_plugin_allowlist':allowlist,'resolver_source_sha256':seals([SOURCE/'app/core/gimpfilterpaths.cpp']), 'executables':{}}
 linkenv=dict(os.environ);linkenv['LD_LIBRARY_PATH']=build_library_path(BUILD,os.environ.get('LD_LIBRARY_PATH',''))
 for path in sorted(runtime):
  if not path.is_file() or not os.access(path,os.X_OK):raise RuntimeError('Missing required runtime executable '+str(path))
  check=subprocess.run(['ldd',str(path)],env=linkenv,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
  if check.returncode or 'not found' in check.stdout+check.stderr:raise RuntimeError('Unresolved runtime libraries for '+str(path))
  report['runtime_preflight']['executables'][str(path)]={'sha256':seals([path])[str(path)],'resolved_libraries':check.stdout,'stderr':check.stderr,'exit_code':check.returncode}
 binaries|=runtime
 binaries|={p.resolve() for p in BUILD.rglob('*.so*') if p.is_file()}
 binaries|={BUILD/'build.ninja',BUILD/'compile_commands.json',BUILD/'meson-info/intro-tests.json',BUILD/'meson-info/intro-targets.json'}
 before['build']=seals(binaries);report['before']=before;save()
 for row in selected:
  verify_lock(9,Path('/workspace/shared/gimp-painter-build.lock'))
  env=dict(os.environ);env.update({k:v for k,v in row['env'].items() if k!='LD_LIBRARY_PATH'})
  env['LD_LIBRARY_PATH']=build_library_path(BUILD,os.environ.get('LD_LIBRARY_PATH',''))
  record={'name':row['name'],'command':row['cmd'],'status':'running','timeout':row['timeout']};report['tests'].append(record);save()
  start=time.monotonic();p=subprocess.Popen(row['cmd'],cwd=row['workdir'] or BUILD,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE,start_new_session=True)
  try:stdout,stderr=p.communicate(timeout=row['timeout']);status='finished'
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL);stdout,stderr=p.communicate();status='timeout'
  scrub=strict.Sanitizer(env);stdout=scrub.text(stdout);stderr=scrub.text(stderr)
  skipped,crashes,failures,known=strict.diagnostics(row['name'],stdout+'\n'+stderr)
  plans=re.findall(r'^1\.\.(\d+)\s*$',stdout,re.M);tap=re.findall(r'^(ok|not ok)\s+(\d+)\s+(.+)$',stdout,re.M)
  tap_ok=not plans or (len(plans)==1 and len(tap)==int(plans[0]) and all(x[0]=='ok' for x in tap))
  passed=status=='finished' and p.returncode==0 and not(skipped or crashes or failures) and tap_ok
  record.update(status=status,exit_code=p.returncode,seconds=time.monotonic()-start,stdout=stdout,stderr=stderr,
                skipped=skipped,crashes=crashes,failures=failures,expected_negative_diagnostics=known,
                tap_plan_counts=list(map(int,plans)),tap_count=len(tap),passed=passed)
  save();print(('PASS ' if passed else 'FAIL ')+row['name'],flush=True)
 report['status']='passed' if len(report['tests'])==len(NAMES) and all(r['passed'] for r in report['tests']) else 'failed'
except BaseException as error:report.update(status='failed',error=str(error),exception=type(error).__name__)
finally:
 report['seals']={k:reseal(v) for k,v in before.items()}
 names_after=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=SOURCE).decode().split('\0')
 after_set={str(SOURCE/n) for n in names_after if n and (SOURCE/n).is_file()}
 report['source_path_set']={'added':sorted(after_set-set(before.get('source',{}))), 'removed':sorted(set(before.get('source',{}))-after_set)}
 if any(report['source_path_set'].values()):report['status']='failed'
 if any(v['changed'] or v['read_errors'] for v in report['seals'].values()):report['status']='failed'
 save()
raise SystemExit(0 if report['status']=='passed' else 1)
