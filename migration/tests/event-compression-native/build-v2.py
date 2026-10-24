import pathlib,json,shlex,subprocess,hashlib,sys
O=pathlib.Path('/workspace/shared/event-compression-order/native');B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');D=pathlib.Path('/workspace/shared/device-name-identity');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
san='--sanitize' in sys.argv
source_inputs={str(p):sha(p) for p in [O/'test-event-order.c',O/'test-painter-perspective-events.c',O/'test-painter-perspective-editor-events.inc',*sorted((O/'overlay/app/display').iterdir())]}
rows=json.loads((B/'compile_commands.json').read_text());row=next(r for r in rows if r['file'].endswith('/test-painter-layer-modes.c'));c=shlex.split(row['command']);obj=O/'fixture.o';c[c.index('-c')+1]=str(O/'test-event-order.c')
for flag in ['-o','-MQ','-MF']:c[c.index(flag)+1]=str(obj)+('.d' if flag=='-MF'else'')
c.insert(1,'-I'+str(O/'overlay/app'));c+=['-DLEASE_CANDIDATE=1'];c+=['-I'+str(B.parent/'source/app/tests')];c+=['-O1','-g1','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if san else [];subprocess.run(c,cwd=B,check=True)
(O/'compile.json').write_text(json.dumps(c,indent=2)+'\n')
if '--compile-only' in sys.argv:sys.exit()
l=json.loads((D/'commands.json').read_text())['link'];l=[str(obj)if x==str(D/'fixture.o')else x for x in l if x!='-Wl,--wrap=gdk_device_get_name'];l[l.index('-o')+1]=str(O/'event-order')
providers=[]
for name in ['gimpdisplayshell.c','gimpdisplayshell-tool-events.c','gimpdisplayshell-handlers.c']:
 row=next(r for r in rows if r['file'].endswith('/'+name));pc=shlex.split(row['command']);po=O/(name+'.o');pc[pc.index('-c')+1]=str(O/'overlay/app/display'/name)
 for flag in ['-o','-MQ','-MF']:pc[pc.index(flag)+1]=str(po)+('.d' if flag=='-MF' else '')
 pc.insert(1,'-I'+str(O/'overlay/app'));c+=['-DLEASE_CANDIDATE=1'];pc+=['-I'+str(B.parent/'source/app/display')]
 pc+=['-O1','-g1','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if san else [];subprocess.run(pc,cwd=B,check=True);l.insert(1,str(po));providers.append(pc)
if '--objects-only' in sys.argv:sys.exit()
if san:l+=['-fsanitize=address,undefined']
inputs={}

for a in l:
 p=pathlib.Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==O/'event-order':continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a'and p.read_bytes()[:8]==b'!<thin>\n':
  for n in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   p=pathlib.Path(n);inputs[str(p.resolve())]=sha(p)
(O/'link-before.json').write_text(json.dumps(inputs,indent=2)+'\n');subprocess.run(l,cwd=B,check=True);assert all(sha(pathlib.Path(p))==h for p,h in inputs.items());(O/'build.json').write_text(json.dumps({'compile':c,'provider_compile':providers,'source_inputs':source_inputs,'source_unchanged':all(sha(pathlib.Path(p))==h for p,h in source_inputs.items()),'link':l,'inputs':len(inputs),'unchanged':True,'binary_sha256':sha(O/'event-order')},indent=2)+'\n')
