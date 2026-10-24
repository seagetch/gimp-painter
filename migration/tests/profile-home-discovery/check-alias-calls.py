#!/usr/bin/env python3
"""Use the already sealed native fixture as a rejection probe; no rebuild."""
import hashlib,json,os,pathlib,subprocess
O=pathlib.Path(__file__).resolve().parent;R=pathlib.Path('/workspace/scratch/0015fc5d1a03/gimp-painter');B=pathlib.Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
receipt=json.loads((O/'green-results.json').read_text());deps=receipt['runtime_dependency_sha256']
assert all(sha(pathlib.Path(p))==h for p,h in deps.items())
runs=[]
for mode in ('same','dotdot','symlink','same-standard'):
 root=O/('alias-probe-'+mode);assert not root.exists();home=root/'home';source=home/'.gimp-painter-2.8';source.mkdir(parents=True)
 (source/'gimprc').write_text('(default-brush "source untouched")\n')
 if mode!='same-standard':
  (source/'tool-options').mkdir();(source/'tool-options/gimp-mypaint-tool').write_text('(stroke-opacity .37)\n')
 if mode=='dotdot':(home/'nested').mkdir();target=home/'nested/../.gimp-painter-2.8'
 elif mode=='symlink':target=home/'alias';target.symlink_to(source,target_is_directory=True)
 else:target=source
 env=os.environ.copy()
 for key,p in [('HOME',home),('XDG_CONFIG_HOME',root/'config'),('XDG_CACHE_HOME',root/'cache'),('XDG_DATA_HOME',root/'data'),('XDG_STATE_HOME',root/'state'),('XDG_RUNTIME_DIR',root/'runtime'),('TMPDIR',root/'tmp')]:
  p.mkdir(parents=True,exist_ok=True);p.chmod(0o700);env[key]=str(p)
 tags=root/'system-data/tags';tags.mkdir(parents=True);(tags/'gimp-tags-default.xml').write_text("<?xml version='1.0'?><tags/>\n")
 private_build=root/'build';(private_build/'app/tests/gimpdir-output').mkdir(parents=True)
 env.update(GIMP3_DIRECTORY=str(target),PROFILE_TARGET=str(target),EXPECTED_SOURCE=str(source),EXPECTED_MINOR='8',PROFILE_CASE='migrate-retry',GIMP3_DATADIR=str(tags.parent),GIMP_TESTING_ABS_TOP_SRCDIR=str(S),GIMP_TESTING_ABS_TOP_BUILDDIR=str(private_build),GIMP_TESTING_PLUGINDIRS=str(B/'plug-ins/common'),LC_ALL='C.UTF-8')
 for key in ('SNAP','SNAP_USER_DATA'):env.pop(key,None)
 before={str(p.relative_to(source)):sha(p) for p in source.rglob('*') if p.is_file()}
 z=subprocess.run([str(O/'normal')],env=env,capture_output=True,text=True,timeout=90)
 after={str(p.relative_to(source)):sha(p) for p in source.rglob('*') if p.is_file()}
 assert z.returncode!=0 and "'gimp_user_install_run(install,1)' should be TRUE" in z.stdout+z.stderr,(mode,z.stdout,z.stderr)
 assert before==after,(mode,before,after)
 runs.append(dict(case=mode,returncode=z.returncode,stdout=z.stdout,stderr=z.stderr,source_before=before,source_after=after,meaning='Existing positive migration assertion fails specifically because public run returns FALSE; source unchanged'))
assert all(sha(pathlib.Path(p))==h for p,h in deps.items())
(O/'alias-calls.json').write_text(json.dumps(dict(executable_sha256=sha(O/'normal'),runtime_dependencies_unchanged=True,runs=runs),indent=2)+'\n')
print('PASS: four isolated direct-call alias rejections; every source byte/file unchanged')
