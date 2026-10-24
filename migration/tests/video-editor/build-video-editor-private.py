from pathlib import Path
import json,hashlib,shlex,subprocess,sys,os
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');S=Path('/workspace/shared/gimp-clean-aggregate/source');B=S.parent/'build';O=Path('/tmp/video-editor-private');O.mkdir(exist_ok=True)
mode=sys.argv[1];assert mode in ['red','green','sanitizer','portrait'];san=mode=='sanitizer';D=O/mode;D.mkdir(exist_ok=True);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=json.loads((B/'compile_commands.json').read_text());commands=[];owned=['app/tests/test-painter-layer-ui.c','app/tests/test-video-editor.inc','migration/tests/video-foundation/reference-patterns.h']
if mode!='red':owned+=['app/dialogs/painter-layer-dialog.cpp']
for p in owned:q=D/p;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes((R/p).read_bytes())
names=['app/tests/test-painter-layer-ui.c']+(['app/dialogs/painter-layer-dialog.cpp']if mode!='red'else[])
if san:names+=['app/core/gimpfilterlayer.cpp','app/core/gimpfiltercontext.cpp','app/core/gimpimage-undo.c','app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/painter/gimp-painter-error.cpp']
ancillary=['app/tests/tests.c','app/tests/test-filter-editor-metadata.cpp','app/tests/test-filter-argument-patch.cpp'];names+=ancillary
objects={};source={}
for name in names:
 row=next(x for x in rows if x['file'].endswith('/'+name) and (name not in ancillary or x['output'].startswith('app/tests/painter-layer-ui.p/')));c=shlex.split(row['command']);obj=D/(Path(name).name+'.o');src=D/name if name in owned else S/name;source[name]=sha(src)
 c[c.index('-c')+1]=str(src)
 for flag in ['-o','-MQ','-MF']:
  if flag in c:c[c.index(flag)+1]=str(obj)+('.d'if flag=='-MF'else'')
 c+=['-I'+str(D/'app/tests'),'-I'+str(S/'app/tests'),'-I'+str(S/'app/dialogs')]
 if san and name not in ancillary:c+=['-O1','-g1','-fsanitize=address,undefined','-fno-sanitize=vptr','-fno-omit-frame-pointer']
 subprocess.run(c,cwd=B,check=True);commands.append(c);objects[name]=(row['output'],str(obj))
lines=subprocess.check_output(['ninja','-t','commands','app/tests/painter-layer-ui'],cwd=B,text=True).splitlines();link=shlex.split(lines[-1]);exe=D/'painter-layer-ui';link[link.index('-o')+1]=str(exe)
old,obj=objects['app/tests/test-painter-layer-ui.c'];assert old in link;link=[obj if x==old else x for x in link]
for name,(old,obj)in objects.items():
 if name!='app/tests/test-painter-layer-ui.c':
  if old in link:link=[obj if x==old else x for x in link]
  else:link.insert(1,obj)
if san:link[1:1]=['-fsanitize=address,undefined','-fno-sanitize=vptr']
inputs={}
# Include dependency files are generated privately by the actual compiler.
headers={}
for dep in D.glob('*.o.d'):
 text=dep.read_text().replace('\\\n',' ')
 for token in shlex.split(text.split(':',1)[1]):
  p=Path(token);p=p if p.is_absolute()else B/p
  if p.is_file():headers[str(p.resolve())]=sha(p)
for arg in link:
 p=Path(arg);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==exe:continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a' and p.open('rb').read(8)==b'!<thin>\n':
  for name in subprocess.check_output(['ar','t',str(p)],cwd=B,text=True).splitlines():
   member=Path(name);member=member if member.is_absolute()else B/member;inputs[str(member.resolve())]=sha(member)
subprocess.run(link,cwd=B,check=True)
assert all(sha(Path(p))==h for p,h in inputs.items())
assert all(sha(Path(p))==h for p,h in headers.items())
ldd=subprocess.check_output(['ldd',str(exe)],text=True);libs={}
for line in ldd.splitlines():
 for token in line.split():
  if token.startswith('/') and Path(token).is_file():libs[token]=sha(Path(token))
(D/'build.json').write_text(json.dumps({'mode':mode,'owned':{p:sha(D/p)for p in owned},'compiled_source':source,'compile':commands,'link':link,'linked_inputs':inputs,'compiler_dependencies':headers,'base_commit':'0a775df413','linked_inputs_unchanged':True,'runtime_libraries':libs,'binary_sha256':sha(exe),'sanitizer_scope':'eight test/dialog/Filter/context/Undo/store/binding/error units; vptr excluded, no RTTI change; other dependencies uninstrumented'if san else'none'},indent=2)+'\n')
print(mode,str(exe),len(inputs),len(libs))
