from pathlib import Path
import json,subprocess,shlex,hashlib
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/unsafe-undo-query');B=Path('/workspace/shared/gimp-clean-aggregate/build');S=B.parent/'source';C=json.loads((B/'compile_commands.json').read_text());commands=[]
paths=['app/core/gimpimage.c','app/core/gimpimage.h','app/core/gimpimage-undo.c','app/paint/painter-mypaint-surface/paint-core.cpp','app/paint/gimppainterpaintgate.cpp','app/tools/gimppaintermybrushtool.cpp']
for p in paths:
 t=W/'source'/p;t.parent.mkdir(parents=True,exist_ok=True);t.write_bytes((R/p).read_bytes())
for p in paths:
 if p.endswith('.h'):continue
 row=next(c for c in C if c['file']=='../source/'+p);cmd=shlex.split(row['command']);cmd[1:1]=['-I'+str(W/'source/app'),'-iquote',str((S/p).parent),'-iquote',str(S/'app/core')];obj=W/('asan-'+Path(p).name+'.o');cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-O1'];cmd[cmd.index('-o')+1]=str(obj);cmd[cmd.index('-MF')+1]=str(obj)+'.d';cmd[-1]=str(W/'source'/p);subprocess.run(cmd,cwd=B,check=True);commands.append(cmd)
(W/'asan-compile.json').write_text(json.dumps(commands,indent=2)+'\n')
