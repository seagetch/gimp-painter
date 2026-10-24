#!/usr/bin/env python3
from pathlib import Path
import argparse,subprocess,hashlib,json,os,time,tempfile
p=argparse.ArgumentParser();p.add_argument('--report',type=Path,required=True);a=p.parse_args();E=Path(__file__).resolve().parent;h={n:hashlib.sha256((E/n).read_bytes()).hexdigest()for n in ['old-oracle.c','compare.c','reference-patterns.h','patterns.json','old-source.json']};report={'scope':'extracted pinned old C only;no oldapplication/nativeFilter execution','source_sha256':h,'runs':{}}
with tempfile.TemporaryDirectory(prefix='video-foundation-')as td:
 for mode,flags in [('normal',['-O2']),('sanitizer',['-O1','-g0','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  exe=Path(td)/mode;cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',*flags,str(E/'old-oracle.c'),str(E/'compare.c'),'-o',str(exe)];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');start=time.monotonic();q=subprocess.run([str(exe)],capture_output=True,text=True,env=env,timeout=120);report['runs'][mode]={'compile':cmd,'exit_code':q.returncode,'stdout':q.stdout,'stderr':q.stderr,'seconds':time.monotonic()-start};a.report.write_text(json.dumps(report,indent=2)+'\n');print(mode,q.returncode,q.stdout,q.stderr,flush=True);assert q.returncode==0
 report['changed_after_run']=[n for n,v in h.items() if hashlib.sha256((E/n).read_bytes()).hexdigest()!=v];assert not report['changed_after_run'];a.report.write_text(json.dumps(report,indent=2)+'\n')
