from pathlib import Path
import json,hashlib,subprocess,shlex,fcntl,os,time,resource
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';O=Path('/tmp/normalize-gray-native-final-proof');P=O/'source';D=O/'mutants';D.mkdir(exist_ok=True)
lock=open(B.parent/'build.lock','w');fcntl.flock(lock,fcntl.LOCK_SH)
sha=lambda b:hashlib.sha256(b).hexdigest()
seal=json.loads((O/'seal.json').read_text());linked=json.loads((O/'linked-before.json').read_text());baseline=json.loads(Path(seal['manifest']).read_text())
for p,h in linked.items():assert sha(Path(p).read_bytes())==h,p
for p,h in baseline.items():assert sha((S/p).read_bytes())==h,p
records=json.loads((O/'results.json').read_text());commands=records['normal']['commands'];compile_cmd=next(c for c in commands if '-c'in c and any(x.endswith('/app/core/gimpfilterlayer.cpp')for x in c));link_cmd=next(c for c in commands if '-o'in c and c[c.index('-o')+1]==str(O/'normal/green'))
source=(P/'app/core/gimpfilterlayer.cpp').read_text();a=source.index('    else if (procedure == "plug-in-normalize")');z=source.index('    else if (procedure == "plug-in-red-eye-removal")',a);branch=source[a:z]
mutations=[('deny-gray','== GIMP_GRAY) &&','== GIMP_RGB) &&','normalize_gray_routes'),('omit-gray-roi','prepared.kernel_region = region;','/* mutation: omit captured ROI */','normalize_gray_context'),('accept-extra-slot','args->size () == 3','args->size () >= 3','normalize_invalid_arguments')]
env=dict(os.environ);env['LD_LIBRARY_PATH']=':'.join(str(B/x)for x in ['libgimpmath','libgimpbase','libgimpcolor','libgimpconfig','libgimpmodule','libgimpthumb','libgimpwidgets'])+':'+env.get('LD_LIBRARY_PATH','');env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(O/'runtime-build'),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),UI_TEST='yes')
rows=[]
for name,old,new,case in mutations:
 assert branch.count(old)==1
 src=D/(name+'.cpp');src.write_text(source[:a]+branch.replace(old,new)+source[z:]);obj=D/(name+'.o');cmd=[str(src)if x==str(P/'app/core/gimpfilterlayer.cpp')else x for x in compile_cmd];cmd[cmd.index('-o')+1]=str(obj)
 q=subprocess.run(cmd,cwd=B,text=True,capture_output=True);(D/(name+'-build.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr
 archive=D/(name+'.a');members=subprocess.check_output(['ar','t',str(O/'normal/green-libappcore.a')],text=True,cwd=B).splitlines();oldobj=str(O/'normal/gimpfilterlayer.cpp.o');assert members.count(oldobj)==1;members=[str(obj)if x==oldobj else x for x in members]
 if archive.exists():archive.unlink()
 ar=['ar','crsT',str(archive),*members];subprocess.run(ar,cwd=B,check=True)
 exe=D/name;cmdlink=[str(archive)if x==str(O/'normal/green-libappcore.a')else x for x in link_cmd];cmdlink[cmdlink.index('-o')+1]=str(exe);q=subprocess.run(cmdlink,cwd=B,text=True,capture_output=True);assert q.returncode==0,q.stderr
 runtime=[str(exe),'-p','/gimp-filter-layer/'+case];t=time.monotonic();q=subprocess.run(runtime,cwd=B,env=env,text=True,capture_output=True,timeout=240,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));row={'name':name,'original':old,'replacement':new,'case':case,'source_sha256':sha(source.encode()),'mutant_sha256':sha(src.read_bytes()),'commands':[cmd,ar,cmdlink,runtime],'binary_sha256':sha(exe.read_bytes()),'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'seconds':time.monotonic()-t};rows.append(row);(D/'results.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,q.returncode,flush=True);assert q.returncode!=0,row
for p,h in linked.items():assert sha(Path(p).read_bytes())==h,p
for p,h in baseline.items():assert sha((S/p).read_bytes())==h,p
for p,h in seal['owned'].items():assert sha((R/p).read_bytes())==h and sha((P/p).read_bytes())==h,p
(D/'seal.json').write_text(json.dumps({'baseline':seal,'linked_unchanged':True,'source_unchanged':True},indent=2)+'\n');print('SEALED three private causal mutations, no shared writes',flush=True)
