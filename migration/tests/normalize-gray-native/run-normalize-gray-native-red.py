from pathlib import Path
import argparse,json,hashlib,subprocess,shlex,os,fcntl,time,resource
ap=argparse.ArgumentParser();ap.add_argument('--commit',required=True);ap.add_argument('--manifest',type=Path,required=True);a=ap.parse_args()
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');A=Path('/workspace/shared/gimp-clean-aggregate');B=A/'build';S=A/'source';O=Path('/tmp/normalize-gray-native-red');O.mkdir(exist_ok=True);P=O/'source';P.mkdir(exist_ok=True)
lock=open(A/'build.lock','w');fcntl.flock(lock,fcntl.LOCK_SH)
sha=lambda b:hashlib.sha256(b).hexdigest();source=json.loads(a.manifest.read_text())
def seal_source():
 for p,h in source.items():assert sha(os.fsencode(os.readlink(S/p))if(S/p).is_symlink()else(S/p).read_bytes())==h,p
seal_source();assert subprocess.check_output(['git','rev-parse',a.commit],cwd=R,text=True).strip()==a.commit
owned=[];pins={}
for p in owned:
 f=P/p;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes((R/p).read_bytes());pins[p]=sha(f.read_bytes())
(O/'owned.json').write_text(json.dumps(pins,indent=2)+'\n')
p='app/tests/test-gimp-filter-layer.c'; f=P/p;f.parent.mkdir(parents=True,exist_ok=True)
t=(S/p).read_text(); extra=Path('/tmp/normalize-gray-oracle/native-gray.inc').read_text();t=t.replace('int main (int argc, char **argv)',extra+'\nint main (int argc, char **argv)');t=t.replace('  ADD (filter_normalize_native_basic);','  ADD (normalize_gray_required);\n  ADD (filter_normalize_native_basic);');f.write_text(t);pins[p]=sha(f.read_bytes());(O/'owned.json').write_text(json.dumps(pins,indent=2)+'\n')
db=json.loads((B/'compile_commands.json').read_text());e=next(x for x in db if x['file'].endswith('/app/tests/test-gimp-filter-layer.c'));cmd=shlex.split(e['command']);obj=O/'fixture.o';clean=[];skip=False
for x in cmd:
 if skip:skip=False;continue
 if x in ['-MF','-MT','-MQ']:skip=True;continue
 if x in ['-MD','-MMD']:continue
 clean.append(str(P/'app/tests/test-gimp-filter-layer.c')if x==e['file']else x)
cmd=clean;cmd[cmd.index('-o')+1]=str(obj);cmd[1:1]=['-I'+str(P/'app/tests')]
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/gimp-filter-layer'],cwd=B,text=True).splitlines()[-1]);exe=O/'gimp-filter-layer';link[link.index('-o')+1]=str(exe);link=[str(obj)if x==e['output']else x for x in link]
inputs=set()
for x in link:
 f=Path(x);f=f if f.is_absolute()else B/f
 if f.is_file()and f not in [obj,exe]:
  inputs.add(f.resolve())
  if f.suffix=='.a'and f.read_bytes()[:8]==b'!<thin>\n':
   for m in subprocess.check_output(['ar','t',str(f)],cwd=B,text=True).splitlines():inputs.add((B/m).resolve())
# Hash runtime assets reached through deliberate private symlinks too.
runtime=O/'runtime-build';(runtime/'app/tests/gimpdir-output').mkdir(parents=True,exist_ok=True);assets={}
for name in ['icons','themes','plug-ins','menus']:
 target=runtime/name
 if not target.exists():target.symlink_to(B/name)
 assert target.is_symlink()and target.resolve()==(B/name).resolve()
 for f in (B/name).rglob('*'):
  if f.is_file():assets[str(f)]=sha(f.read_bytes())
linked={str(f):sha(f.read_bytes())for f in sorted(inputs)};(O/'linked-before.json').write_text(json.dumps(linked,indent=2)+'\n');(O/'runtime-assets.json').write_text(json.dumps(assets,indent=2)+'\n')
for phase,c in [('compile',cmd),('link',link)]:
 q=subprocess.run(c,cwd=B,capture_output=True,text=True);(O/(phase+'.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr
(O/'commands.json').write_text(json.dumps({'compile':cmd,'link':link,'binary_sha256':sha(exe.read_bytes())},indent=2)+'\n')
env=dict(os.environ,GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(runtime),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),UI_TEST='yes',GIMP_TEST_COMPAT_PORTRAIT_DIR=str(O/'screenshots'),TMPDIR='/tmp');(O/'screenshots').mkdir(exist_ok=True)
env['LD_LIBRARY_PATH']=':'.join(str(p)for p in sorted(B.glob('libgimp*'))if p.is_dir())+':'+env.get('LD_LIBRARY_PATH','')
for var,name in [('HOME','home'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache'),('XDG_DATA_HOME','data'),('XDG_STATE_HOME','state'),('XDG_RUNTIME_DIR','runtime')]:
 p=O/'profile'/name;p.mkdir(parents=True,exist_ok=True);p.chmod(0o700);env[var]=str(p)
results={}
for name in ['gray']:
 c=['python3',str(S/'tools/run-painter-xvfb.py'),str(exe),'-p','/gimp-filter-layer/normalize_gray_required'];t=time.monotonic();q=subprocess.run(c,cwd=B,env=env,capture_output=True,text=True,timeout=180,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));results[name]={'command':c,'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'seconds':time.monotonic()-t};(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(name,q.returncode,q.stderr[-600:],flush=True);assert q.returncode!=0 and 'Saved filter procedure or argument mapping is unsupported' in q.stdout+q.stderr,results[name]
seal_source()
for p,h in linked.items():assert sha(Path(p).read_bytes())==h,p
for p,h in assets.items():assert sha(Path(p).read_bytes())==h,p
for p,h in pins.items():assert sha((P/p).read_bytes())==h,p
(O/'seal.json').write_text(json.dumps({'base_commit':a.commit,'manifest':str(a.manifest),'manifest_sha256':sha(a.manifest.read_bytes()),'baseline_sources':len(source),'linked_inputs':len(linked),'runtime_assets':len(assets),'only_test_overlay':pins,'changed':[]},indent=2)+'\n');print('SEALED causal unsupported Gray red; test-only override; shared inputs unchanged',flush=True)
