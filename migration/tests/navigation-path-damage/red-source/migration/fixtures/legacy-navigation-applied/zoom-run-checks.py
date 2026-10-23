#!/usr/bin/env python3
"""Compile source-only old arithmetic, never launch the old application."""
from pathlib import Path
import hashlib,json,os,shlex,shutil,subprocess,tempfile
HERE=Path(__file__).resolve().parent
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0','gobject-2.0','cairo'],text=True));runs=[];mutations=[]
with tempfile.TemporaryDirectory(prefix='legacy-applied-zoom-') as tmp:
 tmp=Path(tmp)
 for sanitized in (False,True):
  exe=tmp/('sanitized' if sanitized else 'normal');cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-O1',str(HERE/'zoom-checks.c'),*flags,'-lm','-o',str(exe)]
  if sanitized:cmd+=['-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
  subprocess.run(cmd,check=True,timeout=120)
  r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=60,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));assert r.returncode==0,(r.stdout,r.stderr)
  runs.append(dict(sanitized=sanitized,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr))
 for name,old,new,test in [
  ('round-instead-of-truncate','target_offset_x = scale_x * image_focus_x - viewport_x;','target_offset_x = RINT(scale_x * image_focus_x - viewport_x);','/legacy-applied-zoom/pointer-queue-event'),
  ('preclamp-request','real_new_scale = new_scale;','real_new_scale = CLAMP(new_scale,ZOOM_MIN,ZOOM_MAX);','/legacy-applied-zoom/clamps'),
  ('omit-inverse','if (shell->rotate_angle != 0.0 || shell->mirrored) {','if (FALSE) {','/legacy-applied-zoom/rotation-mirror'),
  ('ignore-queued-x','canvas_pointer_x = point->x;','canvas_pointer_x = legacy_az_active->pointer_input.pointer_x;','/legacy-applied-zoom/pointer-queue-event')]:
  d=tmp/name;d.mkdir()
  for p in HERE.glob('zoom-*.inc'):shutil.copyfile(p,d/p.name)
  shutil.copyfile(HERE/'zoom-checks.c',d/'zoom-checks.c');p=d/'zoom-functions.inc';s=p.read_text();assert s.count(old)==1,(name,s.count(old));p.write_text(s.replace(old,new));exe=d/'probe'
  subprocess.run(['cc','-std=c11','-O1',str(d/'zoom-checks.c'),*flags,'-lm','-o',str(exe)],check=True,timeout=120)
  r=subprocess.run([str(exe),'-p',test],capture_output=True,text=True,timeout=30);assert r.returncode!=0,(name,r.stdout,r.stderr)
  mutations.append(dict(name=name,before=old,after=new,selected_test=test,exit_code=r.returncode,stdout=r.stdout.replace(str(tmp),'<temporary>'),stderr=r.stderr.replace(str(tmp),'<temporary>')))
report=dict(scope='Unchanged extracted old-source arithmetic with explicit boundary adapter; no old application/runtime. Seven normal+focused ASan/UBSan groups,7680 controlled portrait scenarios per run. GLib/GObject/Cairo uninstrumented;LSan off.',runs=runs,negative_controls=mutations,source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(HERE.glob('zoom-*')) if p.is_file() and p.name!='zoom-check-results.json'})
(HERE/'zoom-check-results.json').write_text(json.dumps(report,indent=2)+'\n');print('7 normal +7 focused sanitizer groups;7680 scenarios each;4 source-mutation controls PASS')
