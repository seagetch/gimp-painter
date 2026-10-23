import pathlib,json,subprocess,shlex,hashlib,os,time
W=pathlib.Path('/workspace/shared/gimp-native-preset-gui');B=W/'build';S=W/'source';E=W/'tool-options-retirement-evidence';O=E/'mutations';O.mkdir(exist_ok=True)
source=S/'app/core/gimptooloptions.c';original=source.read_text();entries=json.loads((B/'compile_commands.json').read_text());entry=next(e for e in entries if e['file'].endswith('/app/core/gimptooloptions.c'));base=shlex.split(entry['command']);actual=str((B/entry['output']).resolve())
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/tool-options-retirement'],cwd=B,text=True).strip().splitlines()[-1]);test=next(t for t in json.loads((B/'meson-info/intro-tests.json').read_text()) if t['name']=='tool-options-retirement');env=os.environ.copy();old=env.get('LD_LIBRARY_PATH','');env.update(test['env']);env['LD_LIBRARY_PATH']=env.get('LD_LIBRARY_PATH','')+':'+old
rows=[]
owned_start=original.index('  /* A removed tool may remain alive')
unbound_start=original.index('  /* Paint-core/global options')
check_start=original.index('  new_info = gimp_tool_options_check_tool_info (options, tool_info, FALSE);')
mutants=[('forget-retired-identity','/tool-options-retirement/remove',original[:owned_start]+original[unbound_start:],'gimp_tool_options_check_tool_info'),('force-unbound-fallback','/tool-options-retirement/thaw',original[:unbound_start]+original[check_start:],'gimp_tool_options_check_tool_info'),('accept-malformed-property','/tool-options-retirement/retired-deserialization-rejected',original.replace('        tool_info = gimp_tool_options_check_tool_info (options, tool_info, TRUE);','        /* mutation: bypass malformed property validation */',1),'Did not see expected message')]

for name,path,text,reason in mutants:
 assert text!=original;D=O/name;D.mkdir(exist_ok=True);src=D/'gimptooloptions.c';obj=D/'gimptooloptions.o';exe=D/'test-tool-options-retirement';src.write_text(text);cmd=[];skip=False
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
print('PASS three actual retirement mutations rejected')
