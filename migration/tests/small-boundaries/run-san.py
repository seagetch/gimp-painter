import json,pathlib,os,subprocess,sys,time
s,b,e=map(pathlib.Path,sys.argv[1:]);env=dict(os.environ);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(s),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(b),'GIMP_TESTING_PLUGINDIRS':str(b/'plug-ins/common'),'UI_TEST':'yes','GSETTINGS_BACKEND':'memory','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
rows=[]
for target,args in [('gimp-painter-surface',['-p','/small-boundaries/native-tiny-edges']),('painter-tempbuf',[])]:
 cmd=[str(b/'small-boundaries-sanitizers'/target),*args];start=time.monotonic();r=subprocess.run(cmd,cwd=b,env=env,capture_output=True,text=True,timeout=90);rows.append(dict(target=target,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,duration_seconds=time.monotonic()-start));assert r.returncode==0,rows[-1]
(e/'sanitizer.json').write_text(json.dumps(rows,indent=2)+'\n')
