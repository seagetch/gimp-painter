import pathlib,json,hashlib,shlex,subprocess
O=pathlib.Path('/workspace/shared/device-name-identity'); B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build'); S=B.parent/'source'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
commands=json.loads((B/'compile_commands.json').read_text()); row=next(r for r in commands if r['file'].endswith('/test-painter-layer-modes.c')); c=shlex.split(row['command']); old=row['file'];obj=O/'fixture.o'
for flag in ['-o','-MQ','-MF']:
 c[c.index(flag)+1]=str(obj)+('.d' if flag=='-MF' else '')
c[c.index('-c')+1]=str(O/'test-device-name-identity.c')
lines=subprocess.check_output(['ninja','-C',str(B),'-t','commands','app/tests/painter-layer-modes'],text=True).splitlines();l=shlex.split(next(s for s in reversed(lines)if ' -o app/tests/painter-layer-modes ' in s));l[l.index('-o')+1]=str(O/'device-name-red'); oldobj=row['output'];assert l.count(oldobj)==1;l[l.index(oldobj)]=str(obj);l+=['-Wl,--wrap=gdk_device_get_name']
inputs={}
for a in l:
 p=pathlib.Path(a);p=p if p.is_absolute()else B/p
 if not p.is_file() or p==obj:continue
 inputs[str(p.resolve())]=sha(p)
 if p.suffix=='.a' and p.read_bytes()[:8]==b'!<thin>\n':
  for name in subprocess.check_output(['ar','t',str(p)],text=True).splitlines():
   n=pathlib.Path(name);n=n if n.is_absolute()else p.parent/n;assert n.is_file(),str(n);inputs[str(n.resolve())]=sha(n)
sources={str(p):sha(p)for p in [O/'test-device-name-identity.c',S/'app/widgets/gimpdevicemanager.c',S/'app/widgets/gimpdeviceinfo.c',S/'app/widgets/gimpdevices.c',S/'app/core/gimpcontainer.c',S/'app/core/gimplist.c']}
(O/'link-inputs-before.json').write_text(json.dumps(inputs,indent=2)+'\n');(O/'source-before.json').write_text(json.dumps(sources,indent=2)+'\n');(O/'commands.json').write_text(json.dumps({'compile':c,'link':l},indent=2)+'\n')
subprocess.run(c,cwd=B,check=True);subprocess.run(l,cwd=B,check=True)
after={p:sha(pathlib.Path(p))for p in inputs};assert after==inputs;(O/'link-inputs-after.json').write_text(json.dumps(after,indent=2)+'\n');assert all(sha(pathlib.Path(p))==h for p,h in sources.items());(O/'build-result.json').write_text(json.dumps({'result':'PASS','linked_inputs':len(inputs),'binary_sha256':sha(O/'device-name-red'),'source_commit':(B.parent/'source-commit.txt').read_text().strip()},indent=2)+'\n')
print('PASS private link',len(inputs),'unchanged exact inputs')
