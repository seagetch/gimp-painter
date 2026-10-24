#!/usr/bin/env python3
"""Pinned writer/type functions only; no old application or Video execution."""
from pathlib import Path
import subprocess,tempfile,shlex,json,struct,hashlib,os
D=Path(__file__).resolve().parent;s=(D/'writer-oracle.c').read_text();meta=json.loads((D/'pinned-writer-reference.json').read_text())
for n,b in meta['exact_functions'].items():assert b in s,n
records={}
with tempfile.TemporaryDirectory(prefix='video-wire-')as d:
 for mode in ['normal','sanitizer']:
  exe=Path(d)/mode;cmd=['cc','-O1','-g',str(D/'writer-oracle.c'),'-o',str(exe)]+shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gobject-2.0'],text=True))
  if mode=='sanitizer':cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
  c=subprocess.run(cmd,capture_output=True,text=True);assert c.returncode==0,c.stderr
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  r=subprocess.run([str(exe)],env=env,capture_output=True,text=True);assert r.returncode==0,r.stderr;rows=[]
  for line in r.stdout.splitlines():
   _,run,pattern,add,rot,hexdata=line.split();vals=list(map(int,[run,pattern,add,rot]));name=b'plug-in-video\0'
   expected=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,vals[0]&0xffffffff)+struct.pack('>IIII',1,0,10,0)+b''.join(struct.pack('>III',2,4,x&0xffffffff)for x in vals[1:])+b'\0'*8
   assert len(expected)==90 and expected.hex()==hexdata;rows.append(dict(run=vals[0],pattern=vals[1],additive=vals[2],rotated=vals[3],hex=hexdata))
  assert len(rows)==7;records[mode]=dict(command=cmd,compile_stderr=c.stderr,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,cases=rows)
(D/'results.json').write_text(json.dumps(records,indent=2)+'\n');print('PASS seven90-byte exact old-writer frames normal/sanitizer; independent BE serializer, no Video pixel execution')
