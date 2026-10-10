from pathlib import Path
import subprocess,json,hashlib,re
root=Path('/workspace/scratch/5b5281e79681/gtk-binding-recovery-20261010/worktree');out=Path(__file__).resolve().parent
binary=Path('/workspace/scratch/5b5281e79681/gimp-native-restore-20261010/build-http/app/tests/painter-perspective-ui')
results=[]
for case in ['property-finalizer-replaces-model']:
 r=subprocess.run([str(binary),'-p','/perspective-ui/'+case],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=90)
 (out/('before-'+case+'.log')).write_text(r.stdout)
 results.append({'case':case,'exit_code':r.returncode,'stdout':r.stdout})
 print(case,r.returncode,flush=True)
(out/'property-before.json').write_text(json.dumps({'results':results,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'source_sha256':{p:hashlib.sha256((root/p).read_bytes()).hexdigest() for p in ['app/tools/gimpperspectiveguidetool.cpp','app/tests/test-painter-perspective-ui.c']}},indent=2)+'\n')
