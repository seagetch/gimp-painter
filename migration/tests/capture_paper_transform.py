#!/usr/bin/env python3
"""Capture actual old Smudge color-blending, alpha and size dynamics via existing pinned legacy archive, without editing its sources.
Source the prepared legacy env.sh before running. Hold the shared build lock.
"""
import argparse,gzip,hashlib,json,os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];old=root.parent/'gimp-painter-legacy';out=root/'migration/fixtures/legacy-paper-transform';commit='afa43fae3e920210146abed514f136fd49f671b5'
parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path);args=parser.parse_args();capture=root/'migration/fixtures/legacy-paper/capture.c'
if args.output:out=args.output;out.mkdir(parents=True,exist_ok=True)
source=old/'app/paint/gimpbrushcore.c'
assert source.read_bytes()==subprocess.check_output(['git','-C',str(old),'show',commit+':app/paint/gimpbrushcore.c'])
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True));cflags=Path(os.environ['LEGACY_BUILD_ROOT'],'gimp-cflags.txt').read_text().strip()
plan=subprocess.check_output(['make','-n','-W','test-xcf.o','test-xcf','CFLAGS='+cflags,'CXXFLAGS=-g -O2 -include type_traits','LIBS=../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0'],cwd=old/'app/tests',text=True)
line=next(x for x in plan.splitlines() if '--mode=link' in x);line=line[line.index('/bin/bash'):]
report={'sources_sha256':{x:hashlib.sha256((old/x).read_bytes()).hexdigest() for x in ['app/paint/gimpbrushcore.c','app/paint/gimppaintoptions.c','app/paint-funcs/paint-funcs-generic.h']},'paint_archive_sha256':hashlib.sha256((old/'app/paint/libapppaint.a').read_bytes()).hexdigest(),'kind':'actual pinned ordinary BrushCore paper mask and paint/Undo runtime','commit':commit,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'archive_sha256':hashlib.sha256((old/'app/core/libappcore.a').read_bytes()).hexdigest(),'scope':'1296 paired actual masks:4 paper channel layouts,3 bitmap and3 generated shapes,3 transforms,3 mask modes,6 positions; no undefined negative-y legacy path is executed' }
with tempfile.TemporaryDirectory(prefix='paper-oracle-') as tmp:
 work=Path(tmp);obj=work/'capture.o';binary=work/'capture'
 compile=[os.environ.get('CC','cc'),'-std=gnu99','-g','-O2','-I'+str(old),'-I'+str(old/'app'),'-I'+str(old/'app/tests'),*flags,'-c',str(capture),'-o',str(obj)]
 result=subprocess.run(compile,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True);(out/'runtime-build.log').write_bytes(result.stdout)
 link=line.replace('-o test-xcf test-xcf.o',f'-o {shlex.quote(str(binary))} {shlex.quote(str(obj))}')
 result=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
 with (out/'runtime-build.log').open('ab') as f:f.write(result.stdout)
 run=subprocess.run([str(binary)],env={**os.environ,"PAINTER_PAPER_TRANSFORM_FIXTURE":"1"},stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
 normalized=b'\n'.join(line for line in run.stdout.splitlines() if line.startswith(b'PAPER_MASK ') or line.startswith(b'PAPER_STROKE ') or line==b'PAPER_CAPTURE_COMPLETE')+b'\n'
 assert run.returncode==0 and len(normalized.splitlines())==2593
 for name,data in [('runtime-capture.log.gz',run.stdout),('pixels.tsv.gz',normalized)]:
  with gzip.GzipFile(out/name,'wb',mtime=0) as f:f.write(data)
 (out/'runtime-capture.stderr').write_bytes(run.stderr)
 report.update(exit_code=run.returncode,compile_command=compile,link_command=link,binary_sha256=hashlib.sha256((work/'.libs/capture').read_bytes()).hexdigest())
report['harness_source']=str(capture.relative_to(root));report['harness_sha256']=hashlib.sha256(capture.read_bytes()).hexdigest()
(out/'runtime.json').write_text(json.dumps(report,indent=2)+'\n');print('captured bytes',len(run.stdout));print(run.stderr.decode());print('exit',run.returncode)
