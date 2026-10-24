import json, pathlib, shlex, subprocess, os
B=pathlib.Path(os.environ['BUILD']);O=pathlib.Path(os.environ['OUT'])
c=next(x for x in json.loads((B/'compile_commands.json').read_text()) if x['file'].endswith('/app/widgets/gimppaintermybrusheditor.cpp'))
a=shlex.split(c['command']);a[a.index('-o')+1]=str(O/'baseline-editor.o');a[-1]=str(O/'baseline-editor.cpp');subprocess.run(a,cwd=B,check=True)
l=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-mypaint-editor'],cwd=B,text=True).splitlines()[-1]);l[l.index('-o')+1]=str(B/'app/tests/painter-editor-duplicate-red');l[1:1]=[str(O/'baseline-editor.o')];subprocess.run(l,cwd=B,check=True)
