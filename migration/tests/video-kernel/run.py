#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,hashlib,json,os,tempfile,time
p=argparse.ArgumentParser();p.add_argument('--report',type=Path,required=True);a=p.parse_args();E=Path(__file__).resolve().parent;R=E.parents[2];F=E.with_name('video-foundation');files=[R/'app/painter/filter-video-kernel.hpp',E/'adapter.cpp',E/'contracts.cpp',F/'compare.c',F/'reference-patterns.h'];h={str(f.relative_to(R)):hashlib.sha256(f.read_bytes()).hexdigest()for f in files};report={'scope':'isolated immutable Video product kernel only;no native owner/transport/runtime','source_sha256':h,'runs':{}}
with tempfile.TemporaryDirectory(prefix='video-kernel-')as td:
 D=Path(td)
 for mode,extra in [('normal',['-O2']),('sanitizer',['-O1','-g0','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  ccmd=['cc','-std=c11',*extra,'-c',str(F/'compare.c'),'-o',str(D/'compare.o')];q=subprocess.run(ccmd,capture_output=True,text=True);assert q.returncode==0,q.stderr
  for kind,inputs in [('matrix',[str(E/'adapter.cpp'),str(D/'compare.o')]),('contracts',[str(E/'contracts.cpp')])]:
   exe=D/(mode+'-'+kind);cmd=['c++','-std=c++14','-fno-rtti','-Wall','-Wextra','-Werror','-pedantic','-I'+str(R/'app/painter'),*extra,*inputs,'-o',str(exe)];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');start=time.monotonic();q=subprocess.run([str(exe)],capture_output=True,text=True,env=env,timeout=120);report['runs'][mode+'-'+kind]={'compile':cmd,'reference_compile':ccmd,'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'seconds':time.monotonic()-start};a.report.write_text(json.dumps(report,indent=2)+'\n');print(mode,kind,q.returncode,q.stdout,q.stderr,flush=True);assert q.returncode==0
 report['header_probes']=[]
 for count in [1,2]:
  src=D/'header.cpp';src.write_text('#include "filter-video-kernel.hpp"\n'*count+'int main(){}\n');cmd=['c++','-std=c++14','-fno-rtti','-Wall','-Wextra','-Werror','-pedantic','-I'+str(R/'app/painter'),'-fsyntax-only',str(src)];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr;report['header_probes'].append({'include_count':count,'command':cmd,'returncode':q.returncode,'source':src.read_text(),'stderr':q.stderr})
report['changed_after_run']=[n for n,v in h.items()if hashlib.sha256((R/n).read_bytes()).hexdigest()!=v];assert not report['changed_after_run'];a.report.write_text(json.dumps(report,indent=2)+'\n')
