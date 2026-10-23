#!/usr/bin/env python3
"""Execute the actual Surface write producer and coordinate tests, not a model."""
import hashlib,json,os,pathlib,re,shlex,subprocess,tempfile,time
ROOT=pathlib.Path(__file__).resolve().parents[3]
sources=['app/paint/painter-mypaint-surface/gegl-surface.cpp','app/paint/painter-mypaint-surface/tests/coordinate-boundaries.cpp']
inputs=sources+['app/paint/painter-mypaint-surface/tests/write-domains.inc','app/paint/painter-mypaint-surface/gegl-surface.hpp','app/paint/painter-mypaint-surface/buffer-read.hpp','app/paint/painter-mypaint-surface/native-pixels.hpp','app/paint/painter-mypaint-surface/legacy-pixel-modes.hpp','app/paint/painter-mypaint-surface/gray-alpha-pixels.hpp']
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True));runs=[]
with tempfile.TemporaryDirectory(prefix='painter-buffer-write-') as tmp:
 for sanitized in (False,True):
  exe=str(pathlib.Path(tmp)/('sanitized' if sanitized else 'normal'))
  cmd=[os.environ.get('CXX','c++'),'-std=c++14','-Wall','-Wextra','-Werror','-O1','-g','-I'+str(ROOT/'app'),'-I'+str(ROOT/'app/paint/painter-mypaint-surface')]
  if sanitized:cmd+=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer']
  cmd += [str(ROOT/s) for s in sources]+flags+['-o',exe];subprocess.run(cmd,check=True,timeout=120)
  env=dict(os.environ,GEGL_USE_OPENCL='no',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  start=time.monotonic();r=subprocess.run([exe],capture_output=True,text=True,env=env,timeout=60)
  count=len(re.findall(r'^ok \d+ /',r.stdout,re.M));assert r.returncode==0 and count==7,(r.stdout,r.stderr)
  runs.append(dict(sanitized=sanitized,command=cmd,exit_code=r.returncode,duration=time.monotonic()-start,passed=count,stdout=r.stdout,stderr=r.stderr))
pathlib.Path(__file__).with_name('standalone.json').write_text(json.dumps(dict(scope='Actual Surface and coordinate tests; GEGL/babl/GLib uninstrumented; LeakSanitizer disabled; not full-app sanitizer',source_sha256={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in inputs},results=runs),indent=2)+'\n')
print('Write producer coordinate7 normal + coordinate7 ASan/UBSan/float-cast-overflow PASS')
