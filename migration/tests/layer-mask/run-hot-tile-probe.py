#!/usr/bin/env python3
"""Compile actual native C tile validation; isolate GEGL swap from shared tests."""
import argparse,hashlib,json,os,pathlib,shlex,subprocess,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);a=p.parse_args();here=pathlib.Path(__file__).resolve().parent;root=here.parents[2]
sources=['migration/tests/layer-mask/hot-tile-probe.c','app/gegl/gimptilehandlervalidate.c','app/core/gimpchunkiterator.c']
packages=['gegl-0.4','cairo','gio-2.0','gdk-pixbuf-2.0'];flags=shlex.split(subprocess.check_output(['pkg-config','--cflags',*packages],text=True));libs=shlex.split(subprocess.check_output(['pkg-config','--libs',*packages],text=True));rows=[]
with tempfile.TemporaryDirectory(prefix='painter-hot-tile-') as d:
 for sanitized in (False,True):
  exe=str(pathlib.Path(d)/('sanitized' if sanitized else 'normal'));cmd=[os.environ.get('CC','cc'),'-std=gnu11','-g','-O1','-ffunction-sections','-fdata-sections','-I'+str(root),'-I'+str(root/'app'),'-I'+str(a.build.resolve()),*flags,*[str(root/s) for s in sources],'-Wl,--gc-sections',*libs,'-Wl,--no-export-dynamic','-lm','-o',exe]
  if sanitized:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  subprocess.run(cmd,check=True,timeout=120)
  start=time.monotonic();r=subprocess.run([exe],capture_output=True,text=True,timeout=30,env=dict(os.environ,GEGL_SWAP='RAM',GEGL_USE_OPENCL='no',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  assert r.returncode==0 and 'flushed 255 0 0 255' in r.stdout,(r.stdout,r.stderr)
  rows.append(dict(sanitized=sanitized,command=cmd,exit_code=r.returncode,duration=time.monotonic()-start,stdout=r.stdout,stderr=r.stderr))
report=dict(scope='Actual C tile validator and chunk iterator plus one mechanism probe; unused unrelated exports dead-stripped, no stubs; dependencies uninstrumented; LSan off; not GimpProjection/full-app sanitizer',gegl_version=subprocess.check_output(['pkg-config','--modversion','gegl-0.4'],text=True).strip(),source_sha256={s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in sources},runs=rows)
(here/'hot-tile-probe.json').write_text(json.dumps(report,indent=2)+'\n');print('Actual C hot-tile mechanism normal +focused ASan/UBSan PASS')
