#!/usr/bin/env python3
from pathlib import Path
import tempfile,subprocess,hashlib,json,argparse
p=argparse.ArgumentParser();p.add_argument('--report',type=Path,required=True);a=p.parse_args();E=Path(__file__).resolve().parent;R=E.parents[2];F=E.with_name('video-foundation');source=(R/'app/painter/filter-video-kernel.hpp').read_text();cases=[('discard-origin','origin_x_ + relative_x','relative_x','matrix','Video mismatch'),('ignore-rotation','options_.rotated ?','false ?','matrix','Video mismatch'),('wrong-saturation','value > 255 ? 255 : value','value > 255 ? 254 : value','matrix','Video mismatch'),('missing-pattern-admission','options.pattern < 0 || options.pattern >= 9 || ','','contracts','Video bad pattern accepted'),('erase-alpha','result[3] = input[3]','result[3] = 0','matrix','Video mismatch')];out=[]
with tempfile.TemporaryDirectory(prefix='video-kernel-mutants-')as td:
 D=Path(td);q=subprocess.run(['cc','-std=c11','-O2','-c',str(F/'compare.c'),'-o',str(D/'compare.o')],capture_output=True,text=True);assert q.returncode==0,q.stderr
 for name,before,after,kind,needle in cases:
  assert source.count(before)==(2 if name=='ignore-rotation'else 1);mutant=source.replace(before,after);(D/'filter-video-kernel.hpp').write_text(mutant);exe=D/name;cmd=['c++','-std=c++14','-fno-rtti','-O2','-I'+str(D),str(E/('adapter.cpp'if kind=='matrix'else'contracts.cpp'))];
  if kind=='matrix':cmd.append(str(D/'compare.o'))
  cmd+=['-o',str(exe)];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr;q=subprocess.run([str(exe)],capture_output=True,text=True,timeout=120);assert q.returncode!=0 and needle in q.stderr,(name,q.stdout,q.stderr);out.append({'name':name,'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'mutant_sha256':hashlib.sha256(mutant.encode()).hexdigest(),'returncode':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'kind':kind});print('REJECT',name,flush=True)
a.report.write_text(json.dumps(out,indent=2)+'\n')
