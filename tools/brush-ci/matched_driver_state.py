#!/usr/bin/env python3
"""Owned diagnostic scratch/metadata only; never a native acceptance gate."""
import argparse,hashlib,json,os,re,stat,subprocess,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
CELLS=('baseline-normal','current-normal','baseline-sanitizer','current-sanitizer')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def policy():return json.loads((HERE/'matched-preclick/policy.json').read_text())
def put(p,j):Path(p).write_text(json.dumps(j,indent=2)+'\n')
def owned_run(raw):
 p=Path(raw).absolute();root=Path(os.environ['RUNNER_TEMP']).resolve();s=p.lstat()
 if p.resolve()!=p or p.parent!=root or not re.fullmatch(r'painter-matched-preclick\.[A-Za-z0-9]{8}',p.name) or not stat.S_ISDIR(s.st_mode) or s.st_uid!=os.getuid() or s.st_mode&0o077:raise ValueError('Invalid private diagnostic root')
 return p
def selection(cell,rep,order,kind):
 expected={'cell':cell,'repetition':rep,'order':order,'kind':kind}
 if expected not in policy()['order']:raise ValueError('Unknown exact matrix invocation')
 return expected

def profile_path(run,cell,order):
 if cell not in CELLS or not 1<=order<=14:raise ValueError('Invalid cell/order')
 return run/'profiles'/('%02d-%s'%(order,cell))
def cache_path(run,cell):
 if cell not in CELLS:raise ValueError('Unknown cell')
 p=run/'cells'/cell/'build/app/tests/gimpdir-output'
 for x in [p,*list(p.parents)[:4]]:
  if x.exists() and x.is_symlink():raise ValueError('Symlinked owned output path')
 if not p.parent.is_dir():raise ValueError('Missing exact configured test build directory')
 return p

def prepare_profile(run,cell,order):
 profile=profile_path(run,cell,order);profile.mkdir(mode=0o700,parents=False)
 for name in ('home','config','cache','data','state','runtime','tmp','gimp-temp','gimp-cache'):(profile/name).mkdir(mode=0o700)
 p=cache_path(run,cell)
 if p.exists():
  if not p.is_dir():raise ValueError('Mutable output is not directory')
  # Preserve even initial configure outputs; no deletion or global cache reset.
  dest=run/'retained-output'/('%02d-%s-before'%(order,cell))
  if dest.exists():raise ValueError('Reused mutable output archive')
  p.rename(dest)
 p.mkdir(mode=0o700)
 if (p/'pluginrc').exists():raise ValueError('Plugin cache not fresh')
 return profile

def archive_output(run,cell,order):
 profile=profile_path(run,cell,order);p=cache_path(run,cell);dest=profile/'native-output'
 if not profile.is_dir() or profile.is_symlink() or dest.exists():raise ValueError('Invalid archive destination')
 if not p.is_dir() or p.is_symlink():raise ValueError('Missing actual mutable build output')
 hashes={};total=0
 for f in sorted(p.rglob('*')):
  if f.is_symlink() or not (f.is_dir() or f.is_file()):raise ValueError('Unexpected mutable output entry')
  if f.is_file():
   total+=f.stat().st_size
   if len(hashes)>=2048 or total>256*1024*1024:raise ValueError('Mutable output inventory exceeds bound')
   hashes[str(f.relative_to(p))]=sha(f)
 p.rename(dest)
 receipt={'format':1,'cell':cell,'order':order,'source_relative':'app/tests/gimpdir-output','archive_relative':'native-output','files':hashes,'bytes':total,'acceptance_eligible':False}
 put(profile/'native-output-archive.json',receipt)
 return receipt

def log_evidence(run,cell,order,out):
 # Public bounded facts only. Full Meson JSON contains environment values and
 # remains in the private build directory; never copy it into artifacts.
 if cell not in CELLS or type(order) is not int or not 1<=order<=14:raise ValueError('Invalid log evidence selection')
 build=run/'cells'/cell/'build';logs=build/'meson-logs';name='matched-%02d-python3.json'%order
 r={'format':1,'cell':cell,'order':order,'expected_log_basename':name,'acceptance_eligible':False,'raw_environment_exported':False,'available':False}
 try:
  if any(x.is_symlink() or not x.is_dir() for x in (run/'cells',run/'cells'/cell,build,logs)):raise ValueError('invalid-log-directory')
  fd=os.open(logs/name,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
  try:
   before=os.fstat(fd)
   if not stat.S_ISREG(before.st_mode) or before.st_size>32*1024*1024:raise ValueError('invalid-log-file')
   with os.fdopen(fd,'rb',closefd=False) as stream:data=stream.read(32*1024*1024+1)
   after=os.fstat(fd)
   if len(data)!=before.st_size or len(data)>32*1024*1024 or (before.st_size,before.st_mtime_ns,before.st_ctime_ns)!=(after.st_size,after.st_mtime_ns,after.st_ctime_ns):raise ValueError('changed-log-file')
  finally:os.close(fd)
  r.update(available=True,raw_log_sha256=hashlib.sha256(data).hexdigest(),raw_bytes=len(data))
  lines=data.decode('utf-8').splitlines()
  if len(lines)!=1:raise ValueError('invalid-record-count')
  def unique(items):
   result={}
   for k,v in items:
    if k in result:raise ValueError('duplicate-json-key')
    result[k]=v
   return result
  row=json.loads(lines[0],object_pairs_hook=unique)
  if not isinstance(row,dict) or row.get('result') not in ('OK','FAIL','ERROR','TIMEOUT','INTERRUPT','SKIP','EXPECTEDFAIL','UNEXPECTEDPASS') or type(row.get('returncode')) is not int or not -255<=row['returncode']<=255:raise ValueError('invalid-native-verdict')
  r.update(meson_result=row['result'],native_returncode=row['returncode'],metadata_valid=True)
 except FileNotFoundError:r['failure_category']='expected-log-missing'
 except (OSError,ValueError,TypeError):r['failure_category']='invalid-or-unreadable-log'
 put(Path(out)/('log-evidence-%02d.json'%order),r)
 return 0 if r.get('metadata_valid') else 1

def check_checkout(source):
 source=Path(source).resolve();head=subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True,timeout=30).strip()
 if not re.fullmatch(r'[0-9a-f]{40}',os.environ.get('GITHUB_SHA','')) or head!=os.environ['GITHUB_SHA']:raise ValueError('Wrong controlling checkout')
 if subprocess.check_output(['git','-C',str(source),'status','--porcelain=v1','--untracked-files=all'],timeout=30):raise ValueError('Controlling checkout changed')
 return head

def main():
 p=argparse.ArgumentParser();p.add_argument('operation',choices=['guard','ready','verify-ready','profile','archive','status','order','cells','log-evidence']);p.add_argument('--run');p.add_argument('--source');p.add_argument('--out');p.add_argument('--cell');p.add_argument('--repetition',type=int);p.add_argument('--order',type=int);p.add_argument('--kind');p.add_argument('--phase');p.add_argument('--stage');p.add_argument('--code',type=int);a=p.parse_args()
 if a.operation=='order':
  for r in policy()['order']:print('\t'.join(str(r[k]) for k in ('cell','repetition','order','kind')))
  return 0
 if a.operation=='cells':
  for c in CELLS:print(c)
  return 0
 if os.environ.get('GITHUB_ACTIONS')!='true':raise ValueError('Hosted diagnostic only')
 if a.operation=='status':
  if a.phase not in ('prepare','measure','one') or not re.fullmatch(r'[a-z0-9-]{1,48}',a.stage or '') or a.code is None:raise ValueError('Invalid finite status')
  out=Path(a.out).resolve();expected=Path(os.environ['GITHUB_WORKSPACE']).resolve()/'matched-results'
  if out!=expected:raise ValueError('Wrong public artifact directory')
  label=a.phase if a.phase!='one' else 'one-%02d'%a.order
  put(out/(label+'-status.json'),{'format':1,'phase':a.phase,'stage':a.stage,'exit_code':a.code,'acceptance_eligible':False,'native_failures_not_waived':True});return 0
 run=owned_run(a.run)
 if a.operation=='guard':
  head=check_checkout(a.source)
  if Path(a.out).resolve()!=Path(os.environ['GITHUB_WORKSPACE']).resolve()/'matched-results':raise ValueError('Wrong artifact destination')
  sys.path.insert(0,str(HERE));from prepare_brush_baseline import commits
  accepted,candidate=commits(json.loads(Path(os.environ['BRUSH_CI_RUNTIME_PINS']).read_text()))
  if candidate!=head or accepted!='e25a94e21efca6e6604ddac6f15570c049f421e7':raise ValueError('Wrong runtime source seal')
  return 0
 if a.operation=='log-evidence':
  if Path(a.out).resolve()!=Path(os.environ['GITHUB_WORKSPACE']).resolve()/'matched-results':raise ValueError('Wrong artifact destination')
  return log_evidence(run,a.cell,a.order,a.out)
 if a.operation=='profile':selection(a.cell,a.repetition,a.order,a.kind);prepare_profile(run,a.cell,a.order);return 0
 if a.operation=='archive':archive_output(run,a.cell,a.order);return 0
 state=run/'ready.json'
 inputs={str(x.relative_to(HERE)):sha(x) for x in sorted(HERE.rglob('*')) if x.is_file() and x.suffix!='.pyc'}
 if a.operation=='ready':
  head=check_checkout(a.source);put(state,{'head':head,'bundle_sha256':inputs,'build_identity_sha256':{c:sha(Path(a.out)/'identities'/(c+'.json')) for c in CELLS}});return 0
 saved=json.loads(state.read_text())
 if saved['head']!=check_checkout(a.source) or saved['bundle_sha256']!=inputs:raise ValueError('Diagnostic code/handoff changed')
 if saved['build_identity_sha256']!={c:sha(Path(a.out)/'identities'/(c+'.json')) for c in CELLS}:raise ValueError('Build identity receipt changed')
 return 0
if __name__=='__main__':
 try:raise SystemExit(main())
 except (OSError,ValueError,KeyError,TypeError,subprocess.SubprocessError):raise SystemExit('Diagnostic state check failed (no private data exported)')
