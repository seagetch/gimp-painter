import json,pathlib,subprocess,tempfile,shlex,os
root=pathlib.Path('<checkout>');out=pathlib.Path('<evidence>');rows=[]
header=root/'app/paint/painter-mypaint-surface/work-budget.hpp';original=header.read_text()
code='''#include "work-budget.hpp"
#include <cassert>
int main(){using namespace GimpPainter::MyPaint; assert(WorkBudget::require(8192,1024)==WorkBudget::limit);bool caught=false;try{WorkBudget::require(8192,1024,1);}catch(const std::length_error&){caught=true;}assert(caught);}
'''
for name,old,new in [('removed-admission','if (provider_bytes > budget ||','if (false && (provider_bytes > budget ||'),('off-by-one','width > (budget - provider_bytes)','width >= (budget - provider_bytes)')]:
 with tempfile.TemporaryDirectory(prefix='work-budget-mutant-') as d:
  p=pathlib.Path(d);mut=original.replace(old,new)
  if name=='removed-admission':mut=mut.replace('/ height))','/ height)))')
  assert mut!=original;(p/'work-budget.hpp').write_text(mut);(p/'probe.cpp').write_text(code)
  subprocess.run(['c++','-std=c++14',str(p/'probe.cpp'),'-o',str(p/'probe')],check=True)
  r=subprocess.run([str(p/'probe')],capture_output=True,text=True);assert r.returncode!=0
  rows.append(dict(name=name,production_file=str(header.relative_to(root)),mutation_from=old,mutation_to=new,exit_code=r.returncode,stderr=r.stderr.replace(d,'<temporary>'),scope='Actual production budget helper in private copy; arithmetic assertions only, no giant allocation or simulated OOM'))
(out/'mutation-results.json').write_text(json.dumps(rows,indent=2)+'\n');print('Two production helper mutations rejected without allocating large buffers')
