#!/usr/bin/env python3
"""Six geometry conflict / recovery controls using private replacement objects/archives.

Caller holds isolated build lock. Production sources/archives remain read-only.
"""
import argparse,pathlib,json,subprocess,shlex,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
results=[]
core='/painter-mypaint-surface/paint-core.cpp';core_archive='app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a';tool='/tools/gimppaintermybrushtool.cpp';tool_archive='app/tools/libapptools.a'
controls=[
 ('missing-null-guard','if(!b)throw std::invalid_argument("Drawable buffer is unavailable");','','gegl_buffer_get_extent',core,core_archive,'paint-geometry-conflict','/geometry-conflict/null-companion-buffer'),
 ('late-setup-recapture','if(!geometry_matches())throw std::invalid_argument("Drawable changed during paint resource setup");','if(fresh){geometry.capture(d);companion.capture(companion_of(d));} if(!geometry_matches())throw std::invalid_argument("Drawable changed during paint resource setup");','should be TRUE',core,core_archive,'paint-geometry-conflict','/geometry-conflict/prestart-resource-replacement'),
 ('missing-conflict-detection','return geometry.matches()&&companion.matches()&&','return true || geometry.matches()&&companion.matches()&&','should be TRUE',core,core_archive,'paint-geometry-conflict','/geometry-conflict/layer/grow'),
 ('missing-companion-identity','companion_of(geometry.target.get())==companion.target.get()','true','should be TRUE',core,core_archive,'paint-geometry-conflict','/geometry-conflict/layer/companion-remove'),
 ('false-engine-rollback','conflicted=true;started=false;batch=false;batch_aborted=false;','conflicted=true;started=false;batch=false;batch_aborted=false;restore_before();','should be TRUE',core,core_archive,'paint-geometry-conflict','/geometry-conflict/layer/offset'),
 ('no-tool-retirement','if(conflict&&binding->accepts(generation))','if(false&&binding->accepts(generation))','gimp_tool_control_activate',tool,tool_archive,'paint-geometry-tool','/geometry/tool/next-input-replace')]

for name,old,new,expected,provider,archive,target,testpath in controls:
 e=next(e for e in json.loads((B/'compile_commands.json').read_text()) if e['file'].endswith(provider))
 s=(pathlib.Path(e['directory'])/e['file']).resolve();text=s.read_text()
 assert text.count(old)==1
 mutated=new.join(text.rsplit(old,1))
 D=O/name;D.mkdir(exist_ok=True);m=D/'paint-core-mutant.cpp';m.write_text(mutated)
 raw=shlex.split(e['command']);cmd=[];skip=False
 for item in raw:
  if skip:skip=False;continue
  if item in ['-MF','-MT','-MQ']:skip=True;continue
  if item in ['-MD','-MMD']:continue
  cmd.append(item)
 obj=D/'mutant.o';cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(m);subprocess.run(cmd,cwd=B,check=True)
 members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines();original=(B/e['output']).resolve();assert sum((B/x).resolve()==original for x in members)==1
 newarchive=D/'app.a';newarchive.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(newarchive),*[str(obj if (B/x).resolve()==original else (B/x).resolve()) for x in members]],cwd=B,check=True)
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=B,text=True).strip().splitlines()[-1]);exe=D/'prepared-mutant';link[link.index('-o')+1]=str(exe);link=[str(newarchive) if x==archive else x for x in link];subprocess.run(link,cwd=B,check=True)
 q=subprocess.run([str(exe),'-p',testpath],capture_output=True,text=True,timeout=120);results.append(dict(mutation=name,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,production_sha256=hashlib.sha256(s.read_bytes()).hexdigest(),mutant_sha256=hashlib.sha256(m.read_bytes()).hexdigest(),command=cmd,link=link));(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert q.returncode!=0 and expected in q.stderr+q.stdout,(name,q.stderr,q.stdout)
print('PASS: geometry conflict controls rejected')
