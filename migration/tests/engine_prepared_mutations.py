#!/usr/bin/env python3
"""Native isolated evaluator mutants; never edits production source/archive."""
import argparse,pathlib,json,subprocess,shlex,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
e=next(e for e in json.loads((B/'compile_commands.json').read_text()) if e['file'].endswith('/painter-mypaint/engine.cpp'))
source=(pathlib.Path(e['directory'])/e['file']).resolve();header=source;original=source.read_text();rows=[]
controls=[('skip-exchange','  impl_.swap(prepared.value_);','  /* mutant: no exchange */','/prepared-state/historical-resource-pair'),('ignore-active-sample','impl_->processing || ','','/prepared-state/busy-exchange')]
for name,old,new,test in controls:
 assert original.count(old)==1
 D=O/name;D.mkdir(exist_ok=True);m=D/'engine-mutant.cpp';m.write_text(original.replace(old,new));raw=shlex.split(e['command']);cmd=[];skip=False
 for v in raw:
  if skip:skip=False;continue
  if v in ['-MF','-MT','-MQ']:skip=True;continue
  if v in ['-MD','-MMD']:continue
  cmd.append(v)
 obj=D/'mutant.o';cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(m);subprocess.run(cmd,cwd=B,check=True)
 archive='app/paint/painter-mypaint/libpainter-mypaint.a';members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines();original_obj=(B/e['output']).resolve();assert sum((B/x).resolve()==original_obj for x in members)==1
 newarchive=D/'engine.a';newarchive.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(newarchive),*[str(obj if (B/x).resolve()==original_obj else (B/x).resolve()) for x in members]],cwd=B,check=True)
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/paint/painter-mypaint/painter-mypaint-prepared-state'],cwd=B,text=True).strip().splitlines()[-1]);exe=D/'checkpoint-mutant';link[link.index('-o')+1]=str(exe);link=[str(newarchive) if x==archive else x for x in link];subprocess.run(link,cwd=B,check=True)
 q=subprocess.run([str(exe),'-p',test],capture_output=True,text=True,timeout=60);rows.append(dict(mutation=name,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,production_sha256=hashlib.sha256(header.read_bytes()).hexdigest(),command=cmd,link=link));(O/'results.json').write_text(json.dumps(rows,indent=2)+'\n');assert q.returncode!=0 and ('void equal(' in q.stderr+q.stdout or 'should be FALSE' in q.stderr+q.stdout),(name,q.stderr,q.stdout)
print('PASS: omitted exchange and active-sample guard mutants rejected')
