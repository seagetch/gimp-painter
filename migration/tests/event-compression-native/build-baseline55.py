import pathlib,json,shlex,subprocess,hashlib,sys
O=pathlib.Path('/workspace/shared/event-compression-order/native');B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');D=pathlib.Path('/workspace/shared/device-name-identity');sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
rows=json.loads((B/'compile_commands.json').read_text());row=next(r for r in rows if r['file'].endswith('/test-painter-layer-modes.c'));c=shlex.split(row['command']);obj=O/'fixture.o';c[c.index('-c')+1]=str(O/'test-event-order.c')
for flag in ['-o','-MQ','-MF']:c[c.index(flag)+1]=str(obj)+('.d' if flag=='-MF'else'')
c+=['-I'+str(B.parent/'source/app/tests')];subprocess.run(c,cwd=B,check=True)
(O/'baseline55-compile.json').write_text(json.dumps(c,indent=2)+'\n')
if '--compile-only' in sys.argv:sys.exit()
l=json.loads((D/'commands.json').read_text())['link'];l=[str(obj)if x==str(D/'fixture.o')else x for x in l if x!='-Wl,--wrap=gdk_device_get_name'];l[l.index('-o')+1]=str(O/'event-order-baseline55')
inputs={}
for a in l:
 p=pathlib.Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file()or p==O/'event-order-baseline55':continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a'and p.read_bytes()[:8]==b'!<thin>\n':
  for n in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   p=pathlib.Path(n);inputs[str(p.resolve())]=sha(p)
(O/'baseline55-link-before.json').write_text(json.dumps(inputs,indent=2)+'\n');subprocess.run(l,cwd=B,check=True);assert all(sha(pathlib.Path(p))==h for p,h in inputs.items());(O/'baseline55-build.json').write_text(json.dumps({'compile':c,'link':l,'inputs':len(inputs),'unchanged':True,'binary_sha256':sha(O/'event-order-baseline55')},indent=2)+'\n')
