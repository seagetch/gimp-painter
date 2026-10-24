import json, pathlib, shlex, subprocess, os
B=pathlib.Path(os.environ['BUILD']); O=pathlib.Path(os.environ['OUT'])
baseline=os.environ.get('BASELINE_VPTR')=='1'
label='asan-vptr-baseline' if baseline else 'asan'
names=['app/widgets/gimppaintermybrusheditor.cpp','app/core/gimppaintermybrush.cpp','app/paint/painter-mypaint-surface/gimp-painter-options.cpp','app/painter/binding-store.cpp','app/painter/gimp-painter-binding.cpp','app/tests/test-painter-mypaint-editor.cpp']
db=json.loads((B/'compile_commands.json').read_text()); commands=[]; objects=[]
for name in names:
 c=next(x for x in db if x['file'].endswith('/'+name)); args=shlex.split(c['command']); obj=O/(label+'-'+pathlib.Path(name).name+'.o'); args[args.index('-o')+1]=str(obj)

 if baseline and name=='app/widgets/gimppaintermybrusheditor.cpp': args[-1]=str(O/'baseline-editor.cpp')
 args=[x for x in args if x not in ['-O2','-O3']]; args[1:1]=['-fsanitize=address,undefined','-fno-sanitize=vptr','-fno-omit-frame-pointer','-O1','-g0'];
 if baseline: args.remove('-fno-sanitize=vptr')
 subprocess.run(args,cwd=B,check=True); commands.append(args);objects.append(str(obj))
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-mypaint-editor'],cwd=B,text=True).splitlines()[-1]);link[link.index('-o')+1]=str(B/('app/tests/painter-editor-duplicate-'+label))
link.remove('app/tests/painter-mypaint-editor.p/test-painter-mypaint-editor.cpp.o');link[1:1]=['-fsanitize=address,undefined']+objects
subprocess.run(link,cwd=B,check=True)
(O/(label+'-build.private.json')).write_text(json.dumps({'compile':commands,'link':link,'scope':names,'limitation':'Six scoped translation units; remaining application/dependencies uninstrumented. LSan disabled. vptr exclusion only for qualified green; baseline control includes vptr. Full-vptr startup diagnostic remains an unresolved gate; mixed RTTI/-fno-rtti instrumentation incompatibility is suspected, not proven.'},indent=2)+'\n')
