#!/usr/bin/env python3
import hashlib,json,resource,subprocess,tempfile
from pathlib import Path
p=Path(__file__).resolve().parent;r=p.parents[2];source=(r/'app/painter/filter-colorify-kernel.hpp').read_text();report={}
mutations=[('combined-luminance','const unsigned luminance = tables_.luminance[0][input[0]] +\n                               tables_.luminance[1][input[1]] +\n                               tables_.luminance[2][input[2]];','const unsigned luminance = static_cast<unsigned>(input[0]*0.2126 + input[1]*0.7152 + input[2]*0.0722);','matrix'),('ignored-rounding-precondition','if (std::fegetround () != FE_TONEAREST)','if (false)','contracts')]
with tempfile.TemporaryDirectory(prefix='colorify-mutants-') as d:
 t=Path(d);h=t/'app/painter/filter-colorify-kernel.hpp';h.parent.mkdir(parents=True);obj=t/'compare.o';subprocess.run(['cc','-std=c11','-O2','-c',str(r/'migration/tests/colorify-foundation/compare.c'),'-o',str(obj)],check=True)
 for name,find,replace,case in mutations:
  assert find in source;changed=source.replace(find,replace);h.write_text(changed);exe=t/name;inputs=[str(p/'adapter.cpp'),str(obj)] if case=='matrix' else [str(p/'contracts.cpp')];cmd=['c++','-std=c++17','-O2','-I'+str(t),'-I'+str(r),*inputs,'-lm','-o',str(exe)];subprocess.run(cmd,check=True);run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=90,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));assert run.returncode!=0;report[name]={'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr,'replace_from':find,'replace_to':replace,'mutated_header_sha256':hashlib.sha256(changed.encode()).hexdigest()}
(p/'mutations.json').write_text(json.dumps(report,indent=2)+'\n');print('PASS2 production kernel causal mutants rejected')
