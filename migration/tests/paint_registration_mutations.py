#!/usr/bin/env python3
"""Two native behavioral controls using private replacement objects/archives.

Caller holds isolated build lock. Production sources/archives remain read-only.
"""
import argparse,pathlib,json,subprocess,shlex,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
e=next(e for e in json.loads((B/'compile_commands.json').read_text()) if e['file'].endswith('/gimp-paint.c') and e['output'].startswith('app/paint/libapppaint.a'))
s=(pathlib.Path(e['directory'])/e['file']).resolve();text=s.read_text();results=[]
controls=[('retain-transferred-reference','  gimp_container_add (registry, GIMP_OBJECT (paint_info));\n  g_object_unref (paint_info);','  gimp_container_add (registry, GIMP_OBJECT (paint_info));\n  /* wrong: retain transfer */','ref_count'),('omit-native-registration','  gimp_container_add (registry, GIMP_OBJECT (paint_info));','  /* wrong: omit sole native registry */','failed')]

for name,old,new,expected in controls:
 assert text.count(old)==1
 D=O/name;D.mkdir(exist_ok=True);m=D/'gimp-paint-mutant.c';m.write_text(text.replace(old,new))
 raw=shlex.split(e['command']);cmd=[];skip=False
 for item in raw:
  if skip:skip=False;continue
  if item in ['-MF','-MT','-MQ']:skip=True;continue
  if item in ['-MD','-MMD']:continue
  cmd.append(item)
 obj=D/'mutant.o';cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(m);subprocess.run(cmd,cwd=B,check=True)
 archive='app/paint/libapppaint.a';members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines();original=(B/e['output']).resolve();assert sum((B/x).resolve()==original for x in members)==1
 newarchive=D/'app.a';newarchive.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(newarchive),*[str(obj if (B/x).resolve()==original else (B/x).resolve()) for x in members]],cwd=B,check=True)
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/paint-registration'],cwd=B,text=True).strip().splitlines()[-1]);exe=D/'paint-mutant';link[link.index('-o')+1]=str(exe);link=[str(newarchive) if x==archive else x for x in link];subprocess.run(link,cwd=B,check=True)
 q=subprocess.run([str(exe)],capture_output=True,text=True,timeout=120);results.append(dict(mutation=name,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,production_sha256=hashlib.sha256(s.read_bytes()).hexdigest(),mutant_sha256=hashlib.sha256(m.read_bytes()).hexdigest(),command=cmd,link=link));(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert q.returncode!=0 and expected in q.stderr+q.stdout,(name,q.stderr,q.stdout)
print('PASS: retained transfer and omitted native registration mutations rejected')
