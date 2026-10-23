#!/usr/bin/env python3
"""Expected-red private copy of the actual production default snapshot branch."""
import hashlib,json,os,pathlib,shlex,subprocess,tempfile,time
out=pathlib.Path(__file__).resolve().parent;root=out.parents[2]
path='app/paint/painter-mypaint-surface/gegl-surface.cpp';original=(root/path).read_text()
before=':ObjectRef<GeglBuffer>::adopt(gegl_buffer_dup(impl_->target.get()));'
after=':ObjectRef<GeglBuffer>::retain(impl_->target.get());'
assert original.count(before)==1
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True))
with tempfile.TemporaryDirectory(prefix='snapshot-mutation-') as d:
 source=pathlib.Path(d)/'gegl-surface.cpp';source.write_text(original.replace(before,after));exe=pathlib.Path(d)/'test'
 cmd=['c++','-std=c++14','-Wall','-Wextra','-Werror','-g','-O1','-I'+str(root/'app'),'-I'+str(root/'app/paint/painter-mypaint-surface'),str(source),str(root/'app/paint/painter-mypaint-surface/tests/snapshot-isolation.cpp'),*flags,'-o',str(exe)]
 subprocess.run(cmd,check=True,timeout=120);t=time.monotonic()
 r=subprocess.run([str(exe),'-p','/snapshot-isolation/default'],capture_output=True,text=True,timeout=60,env=dict(os.environ,GEGL_SWAP='RAM',GEGL_USE_OPENCL='no'))
 assert r.returncode!=0 and 'bytes(left,rect)==bytes(right,rect)' in r.stdout+r.stderr,(r.stdout,r.stderr)
 report=dict(mutation='alias-default-snapshot',scope='Private source copy of actual production Surface; no model or shared source mutation',path=path,original_sha256=hashlib.sha256(original.encode()).hexdigest(),mutated_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),before=before,after=after,command=cmd,compile_exit_code=0,exit_code=r.returncode,duration=time.monotonic()-t,stdout=r.stdout,stderr=r.stderr)
 (out/'default-mutation.json').write_text(json.dumps(report,indent=2)+'\n')
print('Default actual production snapshot alias mutation: expected red')
