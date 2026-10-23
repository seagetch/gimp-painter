#!/usr/bin/env python3
"""Instrument actual TempBuf, pixbuf bridge and Surface sources; never extract models."""
import argparse,hashlib,json,os,pathlib,re,shlex,subprocess,tempfile,time
ROOT=pathlib.Path(__file__).resolve().parents[3];p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);a=p.parse_args();build=a.build.resolve()
cs=['app/core/gimptempbuf.c','libgimpcolor/gimppixbuf.c'];cpp=['app/tests/test-painter-tempbuf.cpp','app/paint/painter-mypaint-surface/gegl-surface.cpp']
headers=['app/core/gimptempbuf.h','libgimpcolor/gimppixbuf.h','app/paint/painter-mypaint-surface/gegl-surface.hpp','app/paint/painter-mypaint-surface/buffer-read.hpp','app/paint/painter-mypaint-surface/native-pixels.hpp','app/paint/painter-mypaint-surface/legacy-pixel-modes.hpp','app/paint/painter-mypaint-surface/gray-alpha-pixels.hpp']
cflags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gegl-0.4','gtk+-3.0'],text=True));libs=shlex.split(subprocess.check_output(['pkg-config','--libs','gegl-0.4','gtk+-3.0'],text=True));runs=[]
with tempfile.TemporaryDirectory(prefix='painter-tempbuf-') as tmp:
 for sanitized in (False,True):
  flags=['-Wall','-Wextra','-g','-O1','-I'+str(ROOT),'-I'+str(ROOT/'app'),'-I'+str(build),'-I'+str(build/'app'),*cflags]
  if sanitized:flags+=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer']
  commands=[];objects=[]
  for source in cs:
   obj=str(pathlib.Path(tmp)/(pathlib.Path(source).stem+'.o'));cmd=[os.environ.get('CC','cc'),'-std=gnu11','-DGIMP_COLOR_COMPILATION',*flags,'-c',str(ROOT/source),'-o',obj];subprocess.run(cmd,check=True,timeout=120);commands.append(cmd);objects.append(obj)
  exe=str(pathlib.Path(tmp)/('sanitized' if sanitized else 'normal'));cmd=[os.environ.get('CXX','c++'),'-std=c++14','-Werror',*flags,*[str(ROOT/s) for s in cpp],*objects,*libs,'-o',exe];subprocess.run(cmd,check=True,timeout=120);commands.append(cmd)
  env=dict(os.environ,GEGL_USE_OPENCL='no',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
  start=time.monotonic();r=subprocess.run([exe],capture_output=True,text=True,env=env,timeout=60);count=len(re.findall(r'^ok \d+ /',r.stdout,re.M));assert r.returncode==0 and count==6,(r.stdout,r.stderr)
  runs.append(dict(sanitized=sanitized,commands=commands,exit_code=r.returncode,duration=time.monotonic()-start,passed=count,stdout=r.stdout,stderr=r.stderr))
report=dict(scope='Actual TempBuf + pixbuf bridge C, Surface C++ and tests; GEGL/babl/GLib/GdkPixbuf uninstrumented; LeakSanitizer disabled; not full-app sanitizer',source_sha256={s:hashlib.sha256((ROOT/s).read_bytes()).hexdigest() for s in cs+cpp+headers},configuration_sha256={'config.h':hashlib.sha256((build/'config.h').read_bytes()).hexdigest()},results=runs)
pathlib.Path(__file__).with_name('standalone.json').write_text(json.dumps(report,indent=2)+'\n');print('TempBuf actual production normal6+sanitized6 PASS')
