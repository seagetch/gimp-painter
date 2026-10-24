#!/usr/bin/env python3
"""Two nonincremental composition causal controls using private replacement objects/archives.

Caller holds isolated build lock. Production sources/archives remain read-only.
"""
import argparse,pathlib,json,subprocess,shlex,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
results=[]
core='/painter-mypaint-surface/paint-core.cpp';core_archive='app/paint/painter-mypaint-surface/libpainter-mypaint-surface.a';tool='/tools/gimppaintermybrushtool.cpp';tool_archive='app/tools/libapptools.a'
controls=[
 ('stroke-opacity-ignored','impl_->opacity=clamp(value);','impl_->opacity=1.f;','assertion failed','/painter-mypaint-surface/gegl-surface.cpp',core_archive,'painter-nonincremental-display','/nonincremental-display/lifecycle'),
 ('floating-not-composed','impl_->write(impl_->target.get(),rect,result);','/* mutated: skip visible composition */','should be TRUE','/painter-mypaint-surface/gegl-surface.cpp',core_archive,'painter-nonincremental-display','/nonincremental-display/lifecycle')]



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
 obj=D/'mutant.o';cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(m);cmd.extend(["-iquote",str(s.parent)]);subprocess.run(cmd,cwd=B,check=True)
 members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines();original=(B/e['output']).resolve();assert sum((B/x).resolve()==original for x in members)==1
 newarchive=D/'app.a';newarchive.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(newarchive),*[str(obj if (B/x).resolve()==original else (B/x).resolve()) for x in members]],cwd=B,check=True)
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/'+target],cwd=B,text=True).strip().splitlines()[-1]);exe=D/'prepared-mutant';link[link.index('-o')+1]=str(exe);link=[str(newarchive) if x==archive else x for x in link];subprocess.run(link,cwd=B,check=True)
 q=subprocess.run([str(exe),'-p',testpath],capture_output=True,text=True,timeout=120);results.append(dict(mutation=name,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,production_sha256=hashlib.sha256(s.read_bytes()).hexdigest(),mutant_sha256=hashlib.sha256(m.read_bytes()).hexdigest(),command=cmd,link=link));(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert q.returncode!=0 and expected in q.stderr+q.stdout,(name,q.stderr,q.stdout)
print('PASS: two nonincremental composition controls rejected')
