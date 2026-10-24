import pathlib,json,shlex,subprocess,tarfile,hashlib
O=pathlib.Path('/workspace/shared/physical-save-baseline');B=pathlib.Path('/workspace/shared/gimp-native-preset-gui/build');R=B.parent/'source';entries=json.loads((B/'compile_commands.json').read_text());replacements={};commands=[]
with tarfile.open('/workspace/shared/physical-gesture/baseline-sources.tar.gz')as t:
 for m in t.getmembers():
  if m.name.endswith('.cpp'):
   p=O/pathlib.Path(m.name).name;p.write_bytes(t.extractfile(m).read());e=next(e for e in entries if(pathlib.Path(e['directory'])/e['file']).resolve()==R/m.name);cmd=shlex.split(e['command']);obj=O/(p.name+'.o');cmd[cmd.index('-o')+1]=str(obj);cmd[-1]=str(p);cmd+=['-iquote',str((R/m.name).parent)];
   if '-MF'in cmd:cmd[cmd.index('-MF')+1]=str(obj)+'.d'
   subprocess.run(cmd,cwd=B,check=True);commands.append(cmd);replacements[str((B/e['output']).resolve())]=str(obj)
e=next(e for e in entries if e['file'].endswith('/test-paint-geometry-tool.cpp'));cmd=shlex.split(e['command']);cmd[cmd.index('-o')+1]=str(O/'probe.o');cmd[-1]=str(O/'probe.cpp')
if '-MF'in cmd:cmd[cmd.index('-MF')+1]=str(O/'probe.d')
subprocess.run(cmd,cwd=B,check=True);commands.append(cmd);replacements[str((B/e['output']).resolve())]=str(O/'probe.o')
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/paint-geometry-tool'],cwd=B,text=True).strip().splitlines()[-1]);archives={}
for archive in {x for x in link if x.endswith('.a')and(B/x).resolve().is_relative_to(B)}:
 members=[str((B/x).resolve())for x in subprocess.check_output(['ar','t',archive],cwd=B,text=True).splitlines()]
 if not any(x in replacements for x in members):continue
 dest=O/archive.replace('/','_');dest.unlink(missing_ok=True);cmd=['ar','crsT',str(dest),*[replacements.get(x,x)for x in members]];subprocess.run(cmd,cwd=B,check=True);commands.append(cmd);archives[archive]=str(dest)
link[link.index('-o')+1]=str(O/'probe');link=[archives.get(x,replacements.get(str((B/x).resolve()),x))for x in link];subprocess.run(link,cwd=B,check=True);commands.append(link);(O/'build.json').write_text(json.dumps({'commands':commands,'sources':{p.name:hashlib.sha256(p.read_bytes()).hexdigest()for p in O.glob('*.cpp')}},indent=2))
