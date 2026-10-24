#!/usr/bin/env python3
import argparse,hashlib,json,subprocess,tempfile,time
from pathlib import Path
p=Path(__file__).resolve().parent
ap=argparse.ArgumentParser();ap.add_argument('--report',type=Path,required=True);a=ap.parse_args();old=json.loads((p/'old-source.json').read_text());source=(p/'old-oracle.c').read_text()
assert old['pixel_function'] in source and old['lookup_initialization'] in source
report={'scope':'exact extracted old C pixel/LUT code versus independent integer IEEE754+rational luminance oracle;no old application/runtime','source_sha256':{f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in [p/'old-oracle.c',p/'compare.c',p/'old-source.json',p/'run.py']},'runs':{}}
with tempfile.TemporaryDirectory(prefix='colorify-foundation-') as temp:
 for name,flags in [('normal',['-O2']),('sanitizer',['-O1','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer'])]:
  exe=str(Path(temp)/name);cmd=['cc','-std=c11',*flags,str(p/'old-oracle.c'),str(p/'compare.c'),'-lm','-o',exe];c=subprocess.run(cmd,capture_output=True,text=True);assert c.returncode==0,c.stderr
  import os
  env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');t=time.monotonic();r=subprocess.run([exe],capture_output=True,text=True,env=env,timeout=180);report['runs'][name]={'command':cmd,'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'seconds':time.monotonic()-t};a.report.write_text(json.dumps(report,indent=2)+'\n');assert r.returncode==0,r.stderr;print(name,r.stdout,flush=True)
 for name,find,replace in [('rounded-tint','i * cvals.color.r;','i * cvals.color.r + 0.5;'),('combined-luminance','lum_red_lookup[src[0]]   +\n        lum_green_lookup[src[1]] +\n        lum_blue_lookup[src[2]]','(gint)(src[0]*0.2126 + src[1]*0.7152 + src[2]*0.0722)')]:
  assert find in source;mut=Path(temp)/(name+'.c');mut.write_text(source.replace(find,replace));exe=str(Path(temp)/name);subprocess.run(['cc','-std=c11','-O2',str(mut),str(p/'compare.c'),'-lm','-o',exe],check=True);r=subprocess.run([exe],capture_output=True,text=True,timeout=90);assert r.returncode!=0 and 'mismatch' in r.stderr;report['runs'][name]={'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr};a.report.write_text(json.dumps(report,indent=2)+'\n')
