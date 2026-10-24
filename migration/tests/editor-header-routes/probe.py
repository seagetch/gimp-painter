from pathlib import Path
import subprocess,json,shlex,hashlib
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/gimp-clean-aggregate');B=W/'build';S=W/'source';O=Path('/workspace/shared/editor-header-routes');data=json.loads((B/'compile_commands.json').read_text());out=[]
pre=(S/'migration/tests/painter-headers/prologue.h').read_text().replace('#ifdef __cplusplus\nextern "C" {\n#endif\n','')
for header in ['app/actions/mypaint-brush-editor-actions.h','app/widgets/gimpmypaintbrusheditor.h','app/widgets/gimpmypaintbrusheditor.hpp','app/widgets/gimppaintermybrusheditor.h','app/widgets/gimppaintermybrusheditor.hpp']:
 for lang in (['cpp'] if header.endswith('.hpp') else ['c','cpp']):
  for count in [1,2]:
   context=pre if '/actions/' in header or header.endswith('gimppaintermybrusheditor.hpp') else ''
   p=O/(header.replace('/','_')+str(count)+'.'+lang);p.write_text(context+('#include "'+str(S/header)+'"\n')*count)
   template=next(x for x in data if x['file'].endswith('probe-000.c' if lang=='c' else 'probe-001.cpp'))
   c=shlex.split(template['command']);c[-1]=str(p);c[c.index('-o')+1]=str(p)+'.o';c[c.index('-MF')+1]=str(p)+'.d'
   x=subprocess.run(c,cwd=B,capture_output=True,text=True);out.append(dict(header=header,language=lang,count=count,command=c,returncode=x.returncode,stdout=x.stdout,stderr=x.stderr));print(header,lang,count,x.returncode)
(O/'probes.json').write_text(json.dumps(out,indent=2)+'\n')
