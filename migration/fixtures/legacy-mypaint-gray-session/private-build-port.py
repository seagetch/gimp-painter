import pathlib,json,shlex,subprocess,hashlib,os
root=pathlib.Path('/workspace/scratch/5b5281e79681/gimp-painter');build=root/'build-debian13';out=pathlib.Path('/workspace/shared/painter-mypaint-gray-oracle');commands=json.loads((build/'compile_commands.json').read_text());record=[];replacements={}
for rel,newsrc,newobj in [('app/tests/painter-mypaint-session-trace.cpp','port-gray.cpp','port-gray.o'),('app/paint/painter-mypaint-surface/gegl-surface.cpp','gegl-surface-gray.cpp','gegl-surface-gray.o')]:
 entry=next(e for e in commands if (pathlib.Path(e['directory'])/e['file']).resolve()==root/rel);cmd=shlex.split(entry['command']);clean=[];skip=False
 for a in cmd:
  if skip:skip=False;continue
  if a in ('-MF','-MQ','-MT'):skip=True;continue
  if a in ('-MD','-MMD'):continue
  clean.append(a)
 clean[clean.index('-o')+1]=str(out/newobj);clean[clean.index('-c')+1]=str(out/newsrc);record.append(clean);subprocess.run(clean,cwd=build,check=True);replacements[str((build/entry['output']).resolve())]=str(out/newobj)
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-mypaint-session-trace'],cwd=build,text=True).strip().splitlines()[-1]);link[link.index('-o')+1]=str(out/'port-gray')
link=[replacements.get(str((build/a).resolve()),a) for a in link]
for archive in sorted(set(a for a in link if a.endswith('.a') and (build/a).resolve().is_relative_to(build))):
 members=[str((build/a).resolve()) for a in subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines()]
 if not any(m in replacements for m in members):continue
 target=out/(archive.replace('/','_'))
 if target.exists():target.unlink()
 cmd=['ar','crsT',str(target)]+[replacements.get(m,m) for m in members];record.append(cmd);subprocess.run(cmd,cwd=build,check=True);link=[str(target) if a==archive else a for a in link]
record.append(link);subprocess.run(link,cwd=build,check=True);env=dict(os.environ);profile=out/'profile3';profile.mkdir(exist_ok=True);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'GIMP3_DIRECTORY':str(profile),'UI_TEST':'yes'})
with (out/'port-runtime.log').open('wb') as stdout,(out/'port-runtime.stderr').open('wb') as stderr:run=subprocess.run([str(out/'port-gray')],env=env,stdout=stdout,stderr=stderr)
expected=b'\n'.join(x for x in (out/'runtime.log').read_bytes().splitlines() if x.startswith(b'GRAY_SESSION_'))+b'\n';actual=b'\n'.join(x for x in (out/'port-runtime.log').read_bytes().splitlines() if x.startswith(b'GRAY_SESSION_'))+b'\n'
r={'exit_code':run.returncode,'equal':expected==actual,'records':len(actual.splitlines()),'expected_sha256':hashlib.sha256(expected).hexdigest(),'actual_sha256':hashlib.sha256(actual).hexdigest(),'bytes':len(actual),'compile_and_link_commands':record,'scope':'Private normal Surface object and comparator linked against current application; production sources/objects untouched.','source_sha256':{n:hashlib.sha256((out/n).read_bytes()).hexdigest() for n in ['port-gray.cpp','gegl-surface-gray.cpp']},'executable_sha256':hashlib.sha256((out/'port-gray').read_bytes()).hexdigest()};(out/'port-report.json').write_text(json.dumps(r,indent=2)+'\n');print({k:v for k,v in r.items() if k not in ['compile_and_link_commands','source_sha256']});raise SystemExit(run.returncode or (expected!=actual))
