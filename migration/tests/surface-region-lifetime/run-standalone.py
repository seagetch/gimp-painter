#!/usr/bin/env python3
"""Compile real Surface and existing11+internal3 ownership cases privately."""
import argparse,hashlib,json,os,pathlib,re,shlex,subprocess,tempfile,time
ROOT=pathlib.Path(__file__).resolve().parents[3]
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
sources=['app/paint/painter-mypaint-surface/gegl-surface.cpp','app/paint/painter-mypaint-surface/tests/region-lifetime.cpp']
headers=['app/paint/painter-mypaint-surface/tests/region-lifetime-internal.inc','app/paint/painter-mypaint-surface/gegl-surface.hpp','app/paint/painter-mypaint-surface/buffer-read.hpp','app/paint/painter-mypaint-surface/native-pixels.hpp','app/paint/painter-mypaint-surface/legacy-pixel-modes.hpp','app/paint/painter-mypaint-surface/gray-alpha-pixels.hpp']
# Seal all project-local quoted-include dependencies, not only the top-level files.
pending=list(sources+headers); closure=set()
while pending:
 n=pending.pop()
 if n in closure:continue
 closure.add(n)
 for quoted in re.findall(r'^\s*#\s*include\s*"([^"]+)"',(ROOT/n).read_text(),re.M):
  candidates=[(ROOT/n).parent/quoted,ROOT/'app'/quoted]
  for candidate in candidates:
   if candidate.is_file():
    pending.append(str(candidate.resolve().relative_to(ROOT)));break
sha={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest()for n in sorted(closure)}
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gegl-0.4'],text=True));results=[]
with tempfile.TemporaryDirectory(prefix='surface-regions-')as tmp:
 for sanitized in (False,True):
  exe=str(pathlib.Path(tmp)/('asan'if sanitized else'normal'))
  command=[os.environ.get('CXX','c++'),'-std=c++14','-Wall','-Wextra','-Werror','-O1','-g','-DGIMP_TEST_REGION_INTERNALS','-I'+str(ROOT/'app'),*[str(ROOT/n)for n in sources],*flags,'-Wl,--wrap=gegl_buffer_new','-Wl,--wrap=gegl_buffer_dup','-o',exe]
  if sanitized:command+=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer']
  compiled=subprocess.run(command,capture_output=True,text=True,timeout=120)
  (a.output/('asan-build.log'if sanitized else'normal-build.log')).write_text(compiled.stdout+compiled.stderr);assert compiled.returncode==0,compiled.stderr
  env=dict(os.environ,HOME=tmp,XDG_CONFIG_HOME=tmp+'/config',XDG_CACHE_HOME=tmp+'/cache',TMPDIR=tmp,GEGL_SWAP=tmp+'/swap',GEGL_USE_OPENCL='no',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  start=time.monotonic();r=subprocess.run([exe],env=env,capture_output=True,text=True,timeout=60);count=len(re.findall(r'^ok \d+ ',r.stdout,re.M))
  result=dict(sanitized=sanitized,command=command,returncode=r.returncode,duration=time.monotonic()-start,timeout=60,passed=count,stdout=r.stdout,stderr=r.stderr,executable_sha256=hashlib.sha256(pathlib.Path(exe).read_bytes()).hexdigest());results.append(result)
  (a.output/'results.json').write_text(json.dumps({'source_sha256':sha,'results':results,'LSan':False},indent=2)+'\n')
  assert r.returncode==0 and count==14,(r.stdout,r.stderr)
  assert not any(hashlib.sha256((ROOT/n).read_bytes()).hexdigest()!=h for n,h in sha.items())
print('PASS actual Surface normal14 + ASan/UBSan14; explicit internal buffer finalization')
