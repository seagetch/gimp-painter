#!/usr/bin/env python3
import argparse,hashlib,json,subprocess,tempfile,os,time
from pathlib import Path
p=Path(__file__).resolve().parent;r=p.parents[2];ap=argparse.ArgumentParser();ap.add_argument('--report',type=Path,required=True);a=ap.parse_args()
paths=['app/painter/filter-colorify-kernel.hpp','migration/tests/colorify-foundation/compare.c','migration/tests/colorify-kernel/adapter.cpp','migration/tests/colorify-kernel/contracts.cpp','migration/tests/colorify-kernel/run.py'];report={'scope':'isolated immutable typed Colorify pixel kernel;no raw/native/storage admission','source_sha256':{x:hashlib.sha256((r/x).read_bytes()).hexdigest() for x in paths},'runs':{}}
with tempfile.TemporaryDirectory(prefix='colorify-kernel-') as d:
 for mode,flags in [('normal',['-O2']),('sanitizer',['-O1','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer'])]:
  obj=str(Path(d)/(mode+'.o'));cmd=['cc','-std=c11',*flags,'-c',str(r/paths[1]),'-o',obj];subprocess.run(cmd,check=True)
  for case in ['matrix','contracts']:
   exe=str(Path(d)/(mode+'-'+case));inputs=[str(p/'adapter.cpp'),obj] if case=='matrix' else [str(p/'contracts.cpp')];command=['c++','-std=c++17',*flags,'-I'+str(r),*inputs,'-lm','-o',exe];c=subprocess.run(command,capture_output=True,text=True);assert c.returncode==0,c.stderr
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');start=time.monotonic();run=subprocess.run([exe],capture_output=True,text=True,env=env,timeout=180);report['runs'][mode+'-'+case]={'compile':command,'compile_stderr':c.stderr,'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr,'seconds':time.monotonic()-start};a.report.write_text(json.dumps(report,indent=2)+'\n');assert run.returncode==0,run.stderr;print(mode,case,run.stdout,flush=True)
report['changed_after_run']=[x for x,h in report['source_sha256'].items() if hashlib.sha256((r/x).read_bytes()).hexdigest()!=h];a.report.write_text(json.dumps(report,indent=2)+'\n');assert not report['changed_after_run']
