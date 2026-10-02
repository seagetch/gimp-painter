#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Capture the unchanged pinned native preview using already built old archives.
Source /workspace/shared/gimp-legacy-build/env.sh first. Hold the shared build
lock. No old source or archive is modified or rebuilt by this runner.
"""
import argparse,gzip,hashlib,json,os,pathlib,shlex,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--legacy',type=pathlib.Path,default=ROOT.parent/'gimp-painter-legacy');p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
old=a.legacy.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
pin='afa43fae3e920210146abed514f136fd49f671b5'
features=['app/core/gimpmypaintbrush.cpp','app/core/gimpmypaintbrush-private.hpp','app/core/gimpmypaintbrush-load.cpp','app/paint/gimpmypaintcore-surface.cpp','app/paint/gimpmypaintcore-surface.hpp','app/paint/mypaintbrush-brush.hpp','app/paint/mypaintbrush-surface.hpp','app/core/mypaintbrush-mapping.hpp']
hashes={}
for source in features:
    data=(old/source).read_bytes();expected=subprocess.check_output(['git','show',pin+':'+source],cwd=old)
    if data!=expected:raise SystemExit('Pinned feature source modified: '+source)
    hashes[source]=hashlib.sha256(data).hexdigest()
source=ROOT/'migration/fixtures/legacy-mypaint-preview/capture-preview.cpp';obj=out/'capture-preview.o';binary=out/'capture-preview'
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True))
compile=[os.environ.get('CXX','c++'),'-std=c++14','-g','-O2','-include','type_traits','-I'+str(old),'-I'+str(old/'app'),*flags,'-c',str(source),'-o',str(obj)]
c=subprocess.run(compile,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'build.log').write_bytes(c.stdout)
if c.returncode:raise SystemExit(c.returncode)
link=gzip.decompress((ROOT/'migration/fixtures/legacy-mypaint-preview/link.sh.gz').read_bytes()).decode().replace('/workspace/shared/painter-mypaint-preview-oracle/capture-preview.o',str(obj)).replace('/workspace/shared/painter-mypaint-preview-oracle/capture-preview',str(binary))
(out/'link.sh').write_text(link)
l=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
(out/'build.log').write_bytes(c.stdout+l.stdout)
if l.returncode:raise SystemExit(l.returncode)
profile=out/'profile';profile.mkdir(exist_ok=True)
env=dict(os.environ);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(old),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(old),'GIMP2_DIRECTORY':str(profile)})
base=ROOT/'migration/fixtures/legacy-mypaint-session/session-base.myb'
run=subprocess.run([str(binary),str(base)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
(out/'runtime.log').write_bytes(run.stdout);(out/'runtime.stderr').write_bytes(run.stderr)
values=b'\n'.join(line for line in run.stdout.splitlines() if line.startswith(b'PREVIEW_'))+b'\n'
(out/'preview-values.tsv.gz').write_bytes(gzip.compress(values,mtime=0))
archives={}
for arg in shlex.split(link):
    path=(old/'app/tests'/arg).resolve()
    if arg.endswith('.a') and path.is_file():archives[str(path.relative_to(old))]=hashlib.sha256(path.read_bytes()).hexdigest()
real=out/'.libs/capture-preview'
report={'pin':pin,'returncode':run.returncode,'records':len(values.splitlines()),'bytes':len(values),'values_sha256':hashlib.sha256(values).hexdigest(),'feature_sha256':hashes,'harness_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'base_sha256':hashlib.sha256(base.read_bytes()).hexdigest(),'archives_sha256':archives,'executable_sha256':hashlib.sha256(real.read_bytes()).hexdigest(),'compile':compile,'link':link,'scope':'Real unchanged get_new_preview,16 synthetic bitmap/paper/smudge/nonincremental combinations and independent repeat per scene; fixed256x256 native size. Not arbitrary brush resources/ICC/precision/tablet proof.'}
(out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k]for k in ['returncode','records','bytes']}))
raise SystemExit(run.returncode)
