#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,json,hashlib,struct,resource
P=Path(__file__).resolve().parent;O=Path('/workspace/shared/color-exchange-wire-foundation');O.mkdir(exist_ok=True)
flags=subprocess.check_output(['pkg-config','--cflags','--libs','gobject-2.0'],text=True).split();results={};source=P/'writer-oracle.c';before=hashlib.sha256(source.read_bytes()).hexdigest()
name=b'plug-in-exchange\0';frame=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,(-123)&0xffffffff)+struct.pack('>IIII',1,0,10,0)+b''.join(struct.pack('>II',4,1)+b'\0'for _ in range(9))+struct.pack('>II',0,0)
assert len(frame)==138
for kind,extra in [('normal',[]),('sanitizer',['-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
 exe=O/kind;cmd=['cc','-std=c11','-O1','-g0',*extra,str(source),'-o',str(exe),*flags];r=subprocess.run(cmd,capture_output=True,text=True);(P/(kind+'-build.log')).write_text(r.stdout+r.stderr);assert r.returncode==0,r.stderr
 env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 r=subprocess.run([str(exe)],env=env,capture_output=True,text=True,timeout=30,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)))
 j=dict(compile=cmd,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr);results[kind]=j;(P/'results.json').write_text(json.dumps(dict(source_sha256=before,runs=results),indent=2)+'\n');assert r.returncode==0,r.stderr
 lines=[x.split()for x in r.stdout.splitlines()if x.startswith('WIRE ')];assert len(lines)==256
 for k,row in enumerate(lines):assert int(row[1])==k and bytes.fromhex(row[2])==frame
 assert 'criticals=2304;256 frames,2304 channel values'in r.stdout
 assert not r.stderr
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
# Mutate only literal body/type contract, leaving host assertions intact.
text=source.read_text();mutations=[]
for kind,old,new in [('unsigned-getter','guint8 write_value = (guint8)g_value_get_int(value);','guint8 write_value = (guint8)g_value_get_uint(value);'),('wrong-int8-parent','g_type_register_static (G_TYPE_UINT, "GimpInt8"','g_type_register_static (G_TYPE_INT, "GimpInt8"')]:
 assert text.count(old)==1;candidate=O/(kind+'.c');candidate.write_text(text.replace(old,new));exe=O/kind;cmd=['cc','-std=c11','-O1',str(candidate),'-o',str(exe),*flags];b=subprocess.run(cmd,capture_output=True,text=True);assert b.returncode==0,b.stderr
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30,preexec_fn=lambda:resource.setrlimit(resource.RLIMIT_CORE,(0,0)));mutations.append(dict(name=kind,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr,source_sha256=before));assert r.returncode!=0
(P/'mutations.json').write_text(json.dumps(mutations,indent=2)+'\n')
print('PASS current-host literal old writer:256 exact138-byte frames,2304 expected signed-getter criticals and encodedzero channels per normal/ASan;2 mutants rejected')
