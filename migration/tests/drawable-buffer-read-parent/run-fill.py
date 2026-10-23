#!/usr/bin/env python3
"""Compile actual bounded-fill production source; dependencies remain uninstrumented."""
import hashlib,json,os,pathlib,re,shlex,subprocess,tempfile,time
ROOT=pathlib.Path(__file__).resolve().parents[3]
sources=['app/paint/painter-bounded-fill/search.cpp','app/paint/painter-bounded-fill/tests/test-search.cpp']
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True))
results=[]
with tempfile.TemporaryDirectory(prefix='painter-read-domains-') as tmp:
 for sanitize in (False,True):
  exe=str(pathlib.Path(tmp)/('fill-sanitized' if sanitize else 'fill-normal'))
  cmd=[os.environ.get('CXX','c++'),'-std=c++14','-Wall','-Wextra','-Werror','-O1','-g','-I'+str(ROOT/'app'),'-I'+str(ROOT/'app/paint/painter-bounded-fill')]
  if sanitize:cmd+=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer']
  cmd += [str(ROOT/s) for s in sources]+flags+['-o',exe]
  subprocess.run(cmd,check=True,timeout=120)
  env=dict(os.environ,GEGL_USE_OPENCL='no',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  start=time.monotonic();run=subprocess.run([exe],capture_output=True,text=True,env=env,timeout=60)
  count=len(re.findall(r'^ok \d+ /',run.stdout,re.M))
  results.append(dict(sanitized=sanitize,command=cmd,exit_code=run.returncode,duration=time.monotonic()-start,passed=count,stdout=run.stdout,stderr=run.stderr))
  assert run.returncode==0 and count==7,results[-1]
report=dict(scope='Bounded-fill production source + seven test groups; GEGL/GLib uninstrumented; LeakSanitizer disabled; no full-app sanitizer claim',source_sha256={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in sources},results=results)
pathlib.Path(__file__).with_name('fill-normal-sanitizers.json').write_text(json.dumps(report,indent=2)+'\n')
print('normal 7/7; ASan/UBSan/float-cast-overflow 7/7')
