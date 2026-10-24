import pathlib,json,shlex,subprocess
W=pathlib.Path('/workspace/shared/gimp-native-preset-gui');B=W/'build';O=pathlib.Path('/workspace/shared/physical-gesture');entries=json.loads((B/'compile_commands.json').read_text());e=next(x for x in entries if x['file'].endswith('/test-painter-physical-gesture.cpp'));raw=shlex.split(e['command']);cmd=[];skip=False
for x in raw:
 if skip:skip=False;continue
 if x in ['-o','-MF','-MT','-MQ']:skip=True;continue
 if x in ['-c','-MD','-MMD']or x==raw[-1]:continue
 cmd.append(x)
results=[]
for name in ['paint-core.hpp','gimp-painter-session.hpp']:
 f=O/('header-'+name+'.cpp');f.write_text('#include "config.h"\n'+('#include "paint/painter-mypaint-surface/'+name+'"\n')*2);c=cmd+['-fsyntax-only','-fno-rtti','-std=c++14',str(f)];q=subprocess.run(c,cwd=B,capture_output=True,text=True);results.append(dict(header=name,command=c,returncode=q.returncode,stdout=q.stdout,stderr=q.stderr));(O/'headers.json').write_text(json.dumps(results,indent=2));assert q.returncode==0,q.stderr
