import pathlib,json,shlex,subprocess,hashlib,os
W=pathlib.Path('/workspace/shared/gimp-clean-aggregate');B=W/'build';S=W/'source';E=pathlib.Path('/workspace/shared/preset-catalog/mutants');E.mkdir(exist_ok=True)
name='app/presets/gimppresetdescriptor.c';src=(S/name).read_text();cc=next(e for e in json.load(open(B/'compile_commands.json')) if pathlib.Path(e['directory'],e['file']).resolve()==S/name)
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/preset-catalog'],cwd=B,text=True).strip().splitlines()[-1]);archive='app/libapp.a';members=[str((B/x).resolve()) for x in subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines()];original=str((B/cc['output']).resolve());assert original in members
cases=[('no-op','  o = gimp_preset_owner (gimp);\n  if (o->publishing','  return TRUE; /* causal mutant: falsely report replacement */\n  o = gimp_preset_owner (gimp);\n  if (o->publishing','replace-retained-snapshots','gimp_preset_descriptor_values (d)->label == name'),('lost-generation','global_generation != catalog_generation || ','','prepare-other-owner-generation','should be FALSE')]
results=[]
for label,old,new,case,needle in cases:
 assert src.count(old)==1
 mutant=E/(label+'.c');mutant.write_text(src.replace(old,new));obj=E/(label+'.o');cmd=shlex.split(cc['command']);cmd=[str(mutant) if pathlib.Path(cc['directory'],x).resolve()==S/name else x for x in cmd];cmd[cmd.index('-o')+1]=str(obj);cmd+=['-I'+str((S/name).parent)];subprocess.run(cmd,cwd=B,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
 ar=E/(label+'.a');subprocess.run(['ar','crsT',str(ar),*[str(obj) if x==original else x for x in members]],cwd=B,check=True)
 exe=E/label;lc=[str(ar) if x==archive else x for x in link];lc[lc.index('-o')+1]=str(exe);subprocess.run(lc,cwd=B,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
 p=subprocess.run([str(exe),'-p','/preset-catalog/'+case],capture_output=True,text=True,timeout=90);r=dict(name=label,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr,mutant_sha256=hashlib.sha256(mutant.read_bytes()).hexdigest(),expected_assertion=needle);results.append(r);(E/'results.json').write_text(json.dumps(results,indent=2));assert p.returncode!=0 and needle in p.stdout+p.stderr,r
print('PASS: two causal implementation mutants rejected')
