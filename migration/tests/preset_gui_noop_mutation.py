#!/usr/bin/env python3
"""Native behavioral negative control: incorrectly alias old no-op to new actions.

Requires an isolated build and caller-held build lock. Production sources and
archives are read only; one replacement object/archive/executable is private.
"""
import argparse,pathlib,json,subprocess,shlex,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
e=next(e for e in json.loads((B/'compile_commands.json').read_text()) if e['file'].endswith('/layer-presets-actions.c'))
s=(pathlib.Path(e['directory'])/e['file']).resolve();text=s.read_text();old='  (void) group;';assert text.count(old)==1
m=O/'layer-presets-actions-mutant.c';m.write_text(text.replace(old,'  if (group) layer_presets_actions_setup (group);'))
raw=shlex.split(e['command']);cmd=[];skip=False
for item in raw:
 if skip:skip=False;continue
 if item in ['-MF','-MT','-MQ']:skip=True;continue
 if item in ['-MD','-MMD']:continue
 cmd.append(item)
obj=O/'mutant.o';cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(m);subprocess.run(cmd,cwd=B,check=True)
archive='app/actions/libappactions.a';members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines();original=(B/e['output']).resolve();assert sum((B/x).resolve()==original for x in members)==1
newarchive=O/'actions.a';newarchive.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(newarchive),*[str(obj if (B/x).resolve()==original else (B/x).resolve()) for x in members]],cwd=B,check=True)
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/preset-gui-entries'],cwd=B,text=True).strip().splitlines()[-1]);exe=O/'preset-gui-mutant';link[link.index('-o')+1]=str(exe);link=[str(newarchive) if x==archive else x for x in link];subprocess.run(link,cwd=B,check=True)
q=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60);result=dict(mutation='incorrectly register native actions from historical empty entry',returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,production_sha256=hashlib.sha256(s.read_bytes()).hexdigest(),mutant_sha256=hashlib.sha256(m.read_bytes()).hexdigest(),command=cmd,link=link);(O/'result.json').write_text(json.dumps(result,indent=2)+'\n');assert q.returncode!=0;assert 'after' in q.stderr and 'NULL' in q.stderr,q.stderr
print('PASS: native test rejects incorrect action registration alias')
