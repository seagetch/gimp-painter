#!/usr/bin/env python3
"""Instrument isolated bounded fill and both test executables; hold shared build lock.
Uses the configured Meson flags without modifying production objects/dependencies.
"""
import argparse,gzip,hashlib,json,os,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();out=build/'bounded-fill-sanitizers';out.mkdir(exist_ok=True)
entries=json.loads((build/'compile_commands.json').read_text());flags=['-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-O1'];objects={};commands=[]
sources=['app/paint/painter-bounded-fill/search.cpp','app/painter/gimp-painter-error.cpp','app/paint/painter-bounded-fill/tests/test-search.cpp','app/paint/painter-bounded-fill/tests/trace.cpp']
for source in sources:
 e=next(e for e in entries if (Path(e['directory'])/e['file']).resolve()==root/source);cmd=shlex.split(e['command']);clean=[];skip=False
 for arg in cmd:
  if skip:skip=False;continue
  if arg in ['-MF','-MQ','-MT']:skip=True;continue
  if arg in ['-MD','-MMD']:continue
  clean.append(arg)
 obj=out/(Path(source).name+'.o');clean[clean.index('-o')+1]=str(obj);clean+=flags;commands.append(clean);subprocess.run(clean,cwd=build,check=True);objects[source]=str(obj)
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'};runs=[]
for part in ['test','trace']:
 target='app/paint/painter-bounded-fill/painter-bounded-fill-'+part
 cmd=shlex.split(subprocess.check_output(['ninja','-t','commands',target],cwd=build,text=True).strip().splitlines()[-1]);exe=out/part;cmd[cmd.index('-o')+1]=str(exe)
 cmd=[x for x in cmd if not x.endswith('libpainter-bounded-fill.a') and not x.endswith('libapppainter.a') and not x.endswith('.o')];cmd[1:1]=[objects[sources[0]],objects[sources[1]],objects[sources[2 if part=='test' else 3]],*flags];commands.append(cmd);subprocess.run(cmd,cwd=build,check=True)
 run=subprocess.run([str(exe)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE);assert run.returncode==0,run.stderr.decode()
 if part=='trace':
  expected=gzip.decompress((root/'migration/fixtures/legacy-bounded-fill/masks.tsv.gz').read_bytes());assert run.stdout==expected
  stdout=f'Exact old runtime match: {len(expected)} bytes, {len(expected.splitlines())} records'
 else:stdout=run.stdout.decode()
 runs.append({'test':part,'exit_code':run.returncode,'stdout':stdout,'stderr':run.stderr.decode()})
a.report.write_text(json.dumps({'scope':'Bounded fill search, error bridge and tests; GEGL/GLib uninstrumented; leak detection disabled','sanitizers':flags,'commands':commands,'results':runs,'source_sha256':{s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in sources}},indent=2)+'\n');print('Bounded fill invariant and 128 legacy search/grow scenarios pass ASan/UBSan/float-cast-overflow')
