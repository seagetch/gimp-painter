#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,hashlib,json,argparse
p=argparse.ArgumentParser();p.add_argument('--report',type=Path,required=True);a=p.parse_args();E=Path(__file__).resolve().parent;text=(E/'old-oracle.c').read_text();cases=[('reset-roi-phase','  gint b, sel_b;','  gint b, sel_b; x=0; y=0;'),('ignore-rotation','if (vvals.rotated)','if (0)'),('wrap-additive','dest[b] = MIN (temp, 255);','dest[b] = (guchar) temp;'),('erase-alpha','dest[b] = src[b];','dest[b] = 0;')];out=[]
with tempfile.TemporaryDirectory(prefix='video-mutants-')as td:
 D=Path(td)
 for name,before,after in cases:
  assert text.count(before)==1,(name,text.count(before));m=text.replace(before,after);src=D/(name+'.c');src.write_text(m);exe=D/name;cmd=['cc','-std=c11','-O2',str(src),str(E/'compare.c'),'-I'+str(E),'-o',str(exe)];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr;q=subprocess.run([str(exe)],capture_output=True,text=True,timeout=120);assert q.returncode!=0 and 'Video mismatch'in q.stderr,(name,q.stdout,q.stderr);out.append({'name':name,'source_sha256':hashlib.sha256(text.encode()).hexdigest(),'mutant_sha256':hashlib.sha256(m.encode()).hexdigest(),'returncode':q.returncode,'stdout':q.stdout,'stderr':q.stderr});print('REJECT',name,flush=True)
a.report.write_text(json.dumps(out,indent=2)+'\n')
