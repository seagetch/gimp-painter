from pathlib import Path
import json,subprocess,hashlib,fcntl,os,resource,time
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';O=Path('/tmp/video-native-final');D=O/'context';D.mkdir(exist_ok=True);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();lock=open(B.parent/'build.lock','w');fcntl.flock(lock,fcntl.LOCK_SH)
owned=['app/tests/test-gimp-filter-layer.c','app/tests/test-filter-video-contract.inc','migration/tests/video-foundation/reference-patterns.h']
for p in owned:q=D/p;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes((R/p).read_bytes())
old=json.loads((O/'results.json').read_text());inputs=json.loads((O/'linked-before.json').read_text());before=dict(inputs);runs={}
for p,w in inputs.items():assert sha(Path(p))==w,p
for mode in ['normal','sanitizer']:
 commands=old[mode]['commands'];c=next(c for c in commands if '-c'in c and any(x.endswith('/app/tests/test-gimp-filter-layer.c')for x in c));c=[str(D/'app/tests/test-gimp-filter-layer.c')if x==str(O/'source/app/tests/test-gimp-filter-layer.c')else x for x in c];obj=D/(mode+'.o');c[c.index('-o')+1]=str(obj);c[1:1]=['-I'+str(D/'app/tests')]
 q=subprocess.run(c,cwd=B,capture_output=True,text=True);(D/(mode+'-build.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr
 link=next(c for c in commands if '-o'in c and c[c.index('-o')+1]==str(O/mode/'green'));link=[str(obj)if x==str(O/mode/'test-gimp-filter-layer.c.o')else x for x in link];exe=D/mode;link[link.index('-o')+1]=str(exe)
 for a in link:
  p=Path(a);p=p if p.is_absolute()else B/p
  if not p.is_file()or p==exe:continue
  before[str(p.resolve())]=sha(p)
  if p.suffix=='.a'and p.open('rb').read(8)==b'!<thin>\n':
   for m in subprocess.check_output(['ar','t',str(p)],cwd=B,text=True).splitlines():
    p=Path(m);p=p if p.is_absolute()else B/p;before[str(p.resolve())]=sha(p)
 q=subprocess.run(link,cwd=B,capture_output=True,text=True);assert q.returncode==0,q.stderr
 runs[mode]={'compile':c,'link':link,'binary_sha256':sha(exe),'runs':{}}
env=dict(os.environ);env['LD_LIBRARY_PATH']=':'.join(str(B/x)for x in ['libgimpmath','libgimpbase','libgimpcolor','libgimpconfig','libgimpmodule','libgimpthumb','libgimpwidgets'])+':'+env.get('LD_LIBRARY_PATH','');env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(B),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),UI_TEST='yes',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for mode,v in runs.items():
 for case in list(old[mode]['runs'])+['video_native_saved_context_metadata']:
  cmd=[str(D/mode),'-p','/gimp-filter-layer/'+case];t=time.monotonic();q=subprocess.run(cmd,cwd=B,env=env,capture_output=True,text=True,timeout=240,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));v['runs'][case]={'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'command':cmd,'seconds':time.monotonic()-t};(D/'results.json').write_text(json.dumps(runs,indent=2));print(mode,case,q.returncode,flush=True);assert q.returncode==0,q.stdout+q.stderr
 assert sha(D/mode)==v['binary_sha256']
for p,w in before.items():assert sha(Path(p))==w,p
for p in owned:assert sha(R/p)==sha(D/p)
(D/'seal.json').write_text(json.dumps({'linked_inputs':before,'changed':[],'source':{p:sha(D/p)for p in owned},'reused_provider_receipt_sha256':sha(O/'results.json'),'reused_provider_source_sha256':sha(O/'source/app/core/gimpfilterlayer.cpp'),'case_counts':{m:len(v['runs'])for m,v in runs.items()}},indent=2));print('SEALED test-only context successor, providers reused byte-exact',flush=True)
