#!/usr/bin/env python3
"""Reproduce the isolated source-oracle frame check; never runs old GIMP."""
import argparse,hashlib,json,shlex,struct,subprocess,tempfile
from pathlib import Path
p=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--report',type=Path,required=True);a=ap.parse_args()
s=(p/'writer-oracle.c').read_text();metadata=json.loads((p/'writer-source.json').read_text())
for name,body in metadata['exact_functions'].items():assert body in s,name
with tempfile.TemporaryDirectory(prefix='red-eye-writer-') as d:
 exe=str(Path(d)/'writer');cmd=['cc',str(p/'writer-oracle.c'),'-o',exe]+shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gobject-2.0'],text=True))
 c=subprocess.run(cmd,capture_output=True,text=True);assert c.returncode==0,c.stderr
 run=subprocess.run([exe],capture_output=True,text=True);assert run.returncode==0,run.stderr;rows=[]
 for line in run.stdout.splitlines():
  if not line.startswith('WIRE '):continue
  _,r,t,h=line.split();r=int(r);t=int(t);name=b'plug-in-red-eye-removal\0'
  expected=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,r&0xffffffff)+struct.pack('>IIII',1,0,10,0)+struct.pack('>III',2,4,t&0xffffffff)+b'\0'*8
  assert len(expected)==76 and expected.hex()==h;rows.append({'run_mode':r,'threshold':t,'hex':h})
 assert len(rows)==8
 a.report.write_text(json.dumps({'exit_code':run.returncode,'cases':rows,'stdout':run.stdout,'stderr':run.stderr,'compile_command':cmd,'compile_stderr':c.stderr,'independent_python_struct_equal':True,'source_sha256':hashlib.sha256(s.encode()).hexdigest()},indent=2)+'\n')
print('PASS8 exact-old-emitter frames versus independent BE serializer;source oracle only')
