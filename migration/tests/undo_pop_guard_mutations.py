#!/usr/bin/env python3
"""Three native behavioral controls using private replacement objects/archives.

Caller holds isolated build lock. Production sources/archives remain read-only.
"""
import argparse,pathlib,json,subprocess,shlex,hashlib
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();B=a.build.resolve();O=a.output.resolve();O.mkdir(parents=True,exist_ok=True)
e=next(e for e in json.loads((B/'compile_commands.json').read_text()) if e['file'].endswith('/gimpimage-undo.c') and e['output'].startswith('app/core/libappcore.a'))
s=(pathlib.Path(e['directory'])/e['file']).resolve();text=s.read_text();results=[]
controls=[('skip-preflight','if (GIMP_IS_UNDO_POP_GUARD (node->undo))','if (FALSE)','should be TRUE'),('skip-identity-check','if (! gimp_undo_pop_plan_matches (plan))','if (FALSE)','should be FALSE'),('release-before-remove','  undo = gimp_undo_stack_pop_undo (undo_stack, undo_mode, &accum);','  for (n = 0; n < plan->len; n++) { GimpUndoPopNode *node = g_ptr_array_index (plan,n); if(node->prepared) {gimp_undo_pop_guard_finish(node->undo,undo_mode);node->prepared=FALSE;} }\n  undo = gimp_undo_stack_pop_undo (undo_stack, undo_mode, &accum);','should be TRUE')]

for name,old,new,expected in controls:
 assert text.count(old)==1
 mutated=new.join(text.rsplit(old,1))
 D=O/name;D.mkdir(exist_ok=True);m=D/'gimpimage-undo-mutant.c';m.write_text(mutated)
 raw=shlex.split(e['command']);cmd=[];skip=False
 for item in raw:
  if skip:skip=False;continue
  if item in ['-MF','-MT','-MQ']:skip=True;continue
  if item in ['-MD','-MMD']:continue
  cmd.append(item)
 obj=D/'mutant.o';cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(m);subprocess.run(cmd,cwd=B,check=True)
 archive='app/core/libappcore.a';members=subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines();original=(B/e['output']).resolve();assert sum((B/x).resolve()==original for x in members)==1
 newarchive=D/'app.a';newarchive.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(newarchive),*[str(obj if (B/x).resolve()==original else (B/x).resolve()) for x in members]],cwd=B,check=True)
 link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/undo-pop-guard'],cwd=B,text=True).strip().splitlines()[-1]);exe=D/'prepared-mutant';link[link.index('-o')+1]=str(exe);link=[str(newarchive) if x==archive else x for x in link];subprocess.run(link,cwd=B,check=True)
 q=subprocess.run([str(exe),'-p',{'skip-preflight':'/pop-guard/success','skip-identity-check':'/pop-guard/changed-prepare','release-before-remove':'/pop-guard/remove-reentry'}[name]],capture_output=True,text=True,timeout=120);results.append(dict(mutation=name,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr,production_sha256=hashlib.sha256(s.read_bytes()).hexdigest(),mutant_sha256=hashlib.sha256(m.read_bytes()).hexdigest(),command=cmd,link=link));(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');assert q.returncode!=0 and expected in q.stderr+q.stdout,(name,q.stderr,q.stdout)
print('PASS: omitted guard, stale-plan check, and early-release mutants rejected')
