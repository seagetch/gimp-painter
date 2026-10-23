import hashlib,json,os,pathlib,shlex,subprocess,tempfile,time
root=pathlib.Path('<checkout>');out=pathlib.Path('<evidence>')
sources=['app/paint/painter-mypaint-surface/gegl-surface.cpp','app/paint/painter-mypaint-surface/tests/coordinate-boundaries.cpp']
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True));rows=[]
with tempfile.TemporaryDirectory(prefix='dirty-focused-') as d:
 for sanitized in (False,True):
  exe=pathlib.Path(d)/('asan' if sanitized else 'normal');cmd=['c++','-std=c++14','-Wall','-Wextra','-Werror','-g','-O1','-I'+str(root/'app'),*[str(root/p) for p in sources],*flags,'-o',str(exe)]
  if sanitized:cmd+=['-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  subprocess.run(cmd,check=True,timeout=120)
  t=time.monotonic();r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60,env=dict(os.environ,GEGL_SWAP='RAM',GEGL_USE_OPENCL='no',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  assert r.returncode==0,(r.stdout,r.stderr)
  rows.append(dict(sanitized=sanitized,command=cmd,exit_code=r.returncode,duration=time.monotonic()-t,stdout=r.stdout,stderr=r.stderr))
paths=sources+['app/paint/painter-mypaint-surface/tests/write-domains.inc']+[str(p.relative_to(root)) for p in (root/'app/paint/painter-mypaint-surface').glob('*.hpp')]+['app/paint/painter-mypaint/surface.hpp','app/painter/object-ref.hpp','app/painter/gimp-painter-visibility.h','app/painter/boundary.hpp','app/painter/gimp-painter-error.h']
report=dict(scope='Existing actual production dirty/coordinate bounds;7 groups normal+focused ASan/UBSan/float-cast-overflow; GEGL/babl uninstrumented; LSan off; not native GimpPaintCore/full-app sanitizer',source_sha256={p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in paths},runs=rows)
(out/'standalone.json').write_text(json.dumps(report,indent=2)+'\n');print('Dirty existing7 groups normal +focused ASan PASS')
