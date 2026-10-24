from pathlib import Path
import json,subprocess,hashlib,shlex,os,time,resource,fcntl
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';O=Path('/tmp/video-native-final');O.mkdir(exist_ok=True);P=O/'source';P.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest();lock=open(B.parent/'build.lock','w');fcntl.flock(lock,fcntl.LOCK_SH)
base='55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94';manifest=B.parent/('exact-'+base)/'prebuild-tracked-source-sha256.json';sealed=json.loads(manifest.read_text())
for p,h in sealed.items():assert sha((S/p).read_bytes())==h,p
owned=['app/core/gimpfilterlayer.cpp','app/painter/filter-point-shadow.cpp','app/painter/filter-point-shadow.hpp','app/tests/test-gimp-filter-layer.c','app/tests/test-filter-normalize-contract.inc','app/tests/test-filter-video-contract.inc','migration/tests/video-foundation/reference-patterns.h']
for p in owned:(P/p).parent.mkdir(parents=True,exist_ok=True);(P/p).write_bytes((R/p).read_bytes())
owned_hashes={p:sha((P/p).read_bytes())for p in owned};(O/'owned.json').write_text(json.dumps(owned_hashes,indent=2)+'\n')
db=json.loads((B/'compile_commands.json').read_text());link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/gimp-filter-layer'],cwd=B,text=True).strip().splitlines()[-1]);records={}
def archive_members(a):return subprocess.check_output(['ar','t',a],cwd=B,text=True).splitlines()
inputs=set()
for arg in link:
 p=Path(arg);p=p if p.is_absolute()else B/p
 if p.is_file():
  inputs.add(p.resolve())
  if p.suffix=='.a' and p.read_bytes()[:8]==b'!<thin>\n':
   for m in archive_members(str(p)):inputs.add((B/m).resolve())
link_hash={str(p):sha(p.read_bytes())for p in sorted(inputs)};(O/'linked-before.json').write_text(json.dumps(link_hash,indent=2)+'\n')
normal=['app/core/gimpfilterlayer.cpp','app/painter/filter-point-shadow.cpp','app/tests/test-gimp-filter-layer.c']
san=['app/core/gimpfilterlayer.cpp','app/core/gimpfiltercontext.cpp','app/painter/filter-context.cpp','app/painter/filter-native-kernels.cpp','app/painter/filter-normalize-raster.cpp','app/painter/filter-point-shadow.cpp','app/painter/filter-raster.cpp','app/painter/filter-scheduler.cpp','app/painter/binding-store.cpp','app/tests/test-gimp-filter-layer.c']
def build(mode,wanted,omit_core=False):
 D=O/mode;D.mkdir(exist_ok=True);replacements={};commands=[];source_hashes={}
 for p in wanted:
  e=next(x for x in db if x['file'].endswith('/'+p)and (x['output'].startswith('app/core/libappcore.a.p/')if p.startswith('app/core/')else x['output'].startswith('app/painter/libapppainter.a.p/')if p.startswith('app/painter/')else True));a=shlex.split(e['command']);out=[];skip=False
  for v in a:
   if skip:skip=False;continue
   if v in ['-MF','-MT','-MQ']:skip=True;continue
   if v in ['-MD','-MMD']:continue
   if v==e['file']:v=str(P/p)if p in owned else str(S/p)
   out.append(v)
  obj=D/(Path(p).name+'.o');out[out.index('-o')+1]=str(obj);out[1:1]=['-I'+str(P/'app'),'-I'+str(P/'app/painter'),'-I'+str(P/'app/tests')]
  if mode=='sanitizer':out+=['-O1','-g0','-fsanitize=address,undefined','-fno-sanitize=vptr','-fno-omit-frame-pointer']
  q=subprocess.run(out,cwd=B,capture_output=True,text=True);(D/(Path(p).name+'.build.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr;commands.append(out);replacements[e['output']]=str(obj);source_hashes[p]=sha((P/p if p in owned else S/p).read_bytes())
 def linkit(name,skipcore):
  cmd=list(link);exe=D/name;cmd[cmd.index('-o')+1]=str(exe)
  active={k:v for k,v in replacements.items()if not(skipcore and k.startswith('app/core/'))}
  for a in ['app/core/libappcore.a','app/painter/libapppainter.a']:
   dest=D/(name+'-'+Path(a).name);members=[active.get(m,str((B/m).resolve()))for m in archive_members(a)]
   if dest.exists():dest.unlink()
   q=['ar','crsT',str(dest),*members];subprocess.run(q,cwd=B,check=True);commands.append(q);cmd=[str(dest)if x==a else x for x in cmd]
  cmd=[active.get(x,x)for x in cmd]
  if mode=='sanitizer':cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize=vptr']
  q=subprocess.run(cmd,cwd=B,capture_output=True,text=True);(D/(name+'-link.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr;commands.append(cmd);return exe
 green=linkit('green',False);red=linkit('red',True)if mode=='normal'else None
 records[mode]={'sources':source_hashes,'commands':commands,'binary_sha256':sha(green.read_bytes()),'runs':{}}
 return green,red
cases=['filter_video_native_basic','video_native_patterns_and_flags','video_native_roi_phase','video_native_invalid_and_formats','video_native_rgb_rgba_inputs','video_native_noalpha_owner_rejected','video_native_cancel_and_resume','video_native_invalid_arguments','video_native_late_context_routes','video_native_component_masks']
env=dict(os.environ);env['LD_LIBRARY_PATH']=':'.join(str(B/x)for x in ['libgimpmath','libgimpbase','libgimpcolor','libgimpconfig','libgimpmodule','libgimpthumb','libgimpwidgets'])+':'+env.get('LD_LIBRARY_PATH','');env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(B),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),UI_TEST='yes',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
def run(exe,case):
 cmd=[str(exe),'-p','/gimp-filter-layer/'+case];t=time.monotonic();q=subprocess.run(cmd,cwd=B,env=env,capture_output=True,text=True,timeout=240,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));return {'command':cmd,'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'seconds':time.monotonic()-t}
runtime_libs={}
for line in subprocess.check_output(['ldd',str(B/'app/tests/gimp-filter-layer')],env=env,text=True).splitlines():
 for token in line.split():
  if token.startswith('/') and Path(token).is_file():runtime_libs[token]=sha(Path(token).read_bytes())
(O/'runtime-before.json').write_text(json.dumps(runtime_libs,indent=2)+'\n')
for mode,wanted in [('normal',normal),('sanitizer',san)]:
 green,red=build(mode,wanted)
 if red:
  row=run(red,cases[0]);(O/'expected-red.json').write_text(json.dumps(row,indent=2)+'\n');print('expected-red',row['exit_code'],row['stdout'][-400:],flush=True);assert row['exit_code']!=0 and 'Saved filter procedure or argument mapping is unsupported' in row['stdout'],row
 for case in cases+(['filter_red_eye_native_basic','filter_colorify_native_basic','filter_color_exchange_native_basic','filter_normalize_native_basic']if mode=='normal'else []):
  row=run(green,case);records[mode]['runs'][case]=row;(O/'results.json').write_text(json.dumps(records,indent=2)+'\n');print(mode,case,row['exit_code'],row['stderr'][-300:],flush=True);assert row['exit_code']==0,row
for p,h in owned_hashes.items():assert sha((R/p).read_bytes())==h and sha((P/p).read_bytes())==h,p
for p,h in link_hash.items():assert sha(Path(p).read_bytes())==h,p
for p,h in sealed.items():assert sha((S/p).read_bytes())==h,p
(O/'seal.json').write_text(json.dumps({'base_commit':base,'manifest':str(manifest),'manifest_sha256':sha(manifest.read_bytes()),'base_sources':len(sealed),'linked_inputs':len(link_hash),'changed':[],'owned':owned_hashes},indent=2)+'\n');print('SEALED private55bf+committed3b507 Normalize dependencies+owned Video, no shared mutation',flush=True)

assert all(sha(Path(p).read_bytes())==h for p,h in runtime_libs.items())
(O/'runtime-seal.json').write_text(json.dumps({'libraries':len(runtime_libs),'unchanged':True}))
