#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,subprocess,shlex,struct,tempfile,os
P=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--report',type=Path,required=True);a=ap.parse_args()
s=(P/'writer-oracle.c').read_text();meta=json.loads((P/'writer-source.json').read_text())
for n,b in meta['exact_functions'].items():assert b in s,n
runs=[-2147483648,-123,-1,0,1,2,123,2147483647]
def admit(r):
 assert r.returncode==0 and not r.stderr,r.stderr
 rows=[]
 for line in r.stdout.splitlines():
  tag,run,h=line.split();assert tag=='WIRE';run=int(run);name=b'plug-in-normalize\0';wire=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,run&0xffffffff)+struct.pack('>IIII',1,0,10,0)+b'\0'*8
  assert len(wire)==58 and bytes.fromhex(h)==wire;rows.append(run)
 assert rows==runs
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gobject-2.0'],text=True));report={'source_sha256':hashlib.sha256(s.encode()).hexdigest(),'runs':{},'mutants':[]}
with tempfile.TemporaryDirectory(prefix='normalize-writer-')as d:
 for mode,extra in [('normal',[]),('sanitizer',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  exe=Path(d)/mode;cmd=['cc','-std=c11','-O1','-g0',*extra,str(P/'writer-oracle.c'),'-o',str(exe),*flags];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');r=subprocess.run([str(exe)],env=env,capture_output=True,text=True);admit(r);report['runs'][mode]={'compile':cmd,'compile_stderr':q.stderr,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'binary_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
 for name,old,new in [('wrong-run-tag','value_type = XCF_FILTER_INT32;','value_type = XCF_FILTER_INT16;'),('wrong-drawable-tag','value_type = XCF_FILTER_DRAWABLE;','value_type = XCF_FILTER_UNSUPPORTED;')]:
  assert s.count(old)==1;src=Path(d)/(name+'.c');src.write_text(s.replace(old,new));exe=Path(d)/name;cmd=['cc','-std=c11',str(src),'-o',str(exe),*flags];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr;r=subprocess.run([str(exe)],capture_output=True,text=True)
  try:admit(r)
  except AssertionError:pass
  else:raise AssertionError('mutant admitted')
  report['mutants'].append({'name':name,'old':old,'new':new,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'rejected_by_independent_wire_oracle':True})
a.report.write_text(json.dumps(report,indent=2)+'\n');print('PASS8 exact58-byte frames normal/ASan,2 independent-oracle mutations; source-derived only')
