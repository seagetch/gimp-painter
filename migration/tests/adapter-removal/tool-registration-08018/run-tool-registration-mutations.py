import pathlib,json,subprocess,shlex,hashlib,os,time
W=pathlib.Path('/workspace/shared/gimp-native-preset-gui');B=W/'build';S=W/'source';E=W/'tool-registration-evidence';O=E/'mutations';O.mkdir(exist_ok=True)
source=S/'app/tools/gimp-tools.c';original=source.read_text();entries=json.loads((B/'compile_commands.json').read_text());entry=next(e for e in entries if e['file'].endswith('/app/tools/gimp-tools.c'));base=shlex.split(entry['command']);actual=str((B/entry['output']).resolve())
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/tool-registration'],cwd=B,text=True).strip().splitlines()[-1]);test=next(t for t in json.loads((B/'meson-info/intro-tests.json').read_text()) if t['name']=='tool-registration');env=os.environ.copy();old=env.get('LD_LIBRARY_PATH','');env.update(test['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+old
rows=[]
mutants=[('retain-transferred-reference','/tool-registration/transfer',original.replace('  gimp_container_add (registry, GIMP_OBJECT (tool_info));\n  g_object_unref (tool_info);','  gimp_container_add (registry, GIMP_OBJECT (tool_info));\n  /* mutation: do not consume caller reference */',1),'ref_count == 1'),('omit-native-registration','/tool-registration/native-catalog',original.replace('    gimp_tools_register (gimp, g_object_ref (tool_info));','    (void) tool_info; /* mutation: native registration disconnected */',1),'count == 50')]
for name,path,text,reason in mutants:
 assert text!=original;D=O/name;D.mkdir(exist_ok=True);src=D/'gimp-tools.c';obj=D/'gimp-tools.o';exe=D/'test-tool-registration';src.write_text(text);cmd=[];skip=False
 for arg in base:
  if skip:skip=False;continue
  if arg in ['-MF','-MQ','-MT']:skip=True;continue
  if arg in ['-MD','-MMD']:continue
  cmd.append(str(src) if arg==entry['file'] else arg)
 cmd[cmd.index('-o')+1]=str(obj);subprocess.run(cmd,cwd=B,check=True)
 linked=list(link);linked[linked.index('-o')+1]=str(exe)
 for archive in sorted({a for a in linked if a.endswith('.a') and (B/a).resolve().is_relative_to(B)}):
  members=[str((B/m).resolve()) for m in subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines()]
  if actual not in members:continue
  dest=D/archive.replace('/','_');dest.unlink(missing_ok=True);subprocess.run(['ar','crsT',str(dest),*[str(obj) if m==actual else m for m in members]],check=True);linked=[str(dest) if a==archive else a for a in linked]
 subprocess.run(linked,cwd=B,check=True);start=time.monotonic();p=subprocess.run([str(exe),'-p',path],cwd=B,env=env,capture_output=True,text=True,timeout=35);out=p.stdout+p.stderr
 assert p.returncode!=0 and text and path in out and reason in out,(name,p.returncode,out)
 rows.append(dict(mutation=name,test=path,returncode=p.returncode,duration=time.monotonic()-start,stdout=p.stdout,stderr=p.stderr,production_sha256=hashlib.sha256(original.encode()).hexdigest(),mutant_sha256=hashlib.sha256(text.encode()).hexdigest(),compile_command=cmd,link_command=linked));(E/'native-mutations.json').write_text(json.dumps(rows,indent=2)+'\n')
print('PASS two real provider mutations rejected')
