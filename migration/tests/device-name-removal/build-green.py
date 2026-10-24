import pathlib,json,shlex,subprocess,hashlib,sys
O=pathlib.Path('/workspace/shared/device-name-identity');B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');rows=json.loads((B/'compile_commands.json').read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();mode=sys.argv[1] if len(sys.argv)>1 else 'normal';san=mode=='sanitizer';prefix=mode;commands=[]
for source,key,kind in [(O/'test-device-name-identity.c','/test-painter-layer-modes.c','fixture'),(O/('manager-before.c' if mode=='mutant' else 'manager-after.c'),'/gimpdevicemanager.c','manager')]:
 row=next(r for r in rows if r['file'].endswith(key));c=shlex.split(row['command']);obj=O/(prefix+'-'+kind+'.o');c[c.index('-c')+1]=str(source)
 for flag in ['-o','-MQ','-MF']:
  c[c.index(flag)+1]=str(obj)+('.d' if flag=='-MF' else '')
 c+=['-I'+str(B.parent/'source/app/widgets')]
 if san:c+=['-O1','-g1','-fsanitize=address,undefined','-fno-omit-frame-pointer']
 subprocess.run(c,cwd=B,check=True);commands.append(c)
l=json.loads((O/'commands.json').read_text())['link'];l=[str(O/(prefix+'-fixture.o'))if x==str(O/'fixture.o') else str(O/'exact503-libappcore.a')if x=='app/core/libappcore.a' else x for x in l];l[l.index('-o')+1]=str(O/('device-name-'+mode));l.insert(1,str(O/(prefix+'-manager.o')))
if san:l+=['-fsanitize=address,undefined']
inputs={}
for a in l:
 p=pathlib.Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file() or p == O/('device-name-'+mode):continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a' and p.read_bytes()[:8]==b'!<thin>\n':
  for n in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   n=pathlib.Path(n);assert n.is_file();inputs[str(n.resolve())]=sha(n)
(O/(prefix+'-link-before.json')).write_text(json.dumps(inputs,indent=2)+'\n');subprocess.run(l,cwd=B,check=True);assert all(sha(pathlib.Path(p))==h for p,h in inputs.items());(O/(prefix+'-build.json')).write_text(json.dumps({'compile':commands,'link':l,'linked_inputs':len(inputs),'unchanged':True,'sources':{str(p):sha(p) for p in [O/'test-device-name-identity.c',O/('manager-before.c'if mode=='mutant'else'manager-after.c')]},'binary_sha256':sha(O/('device-name-'+mode))},indent=2)+'\n');print('PASS',mode,len(inputs))
