#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Capture warmed active pipes using pinned old archives, without source edits.
Source /workspace/shared/gimp-legacy-build/env.sh and hold the shared build lock.
"""
import argparse,gzip,hashlib,json,os,pathlib,shlex,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
FIX=ROOT/'migration/fixtures/legacy-mypaint-pipe'
COMMIT='afa43fae3e920210146abed514f136fd49f671b5'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--legacy',type=pathlib.Path,default=ROOT.parent/'gimp-painter-legacy');p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--verify-fixture',action='store_true');a=p.parse_args()
    old=a.legacy.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True);checks={}
    if a.verify_fixture and out==FIX.resolve():raise SystemExit('Verification output must be separate from the fixture')
    expected=gzip.decompress((FIX/'pipe-values.tsv.gz').read_bytes()) if a.verify_fixture else None
    for rel in ['app/core/gimpbrushpipe.c','app/paint/gimpmypaintcore.cpp','app/paint/gimpmypaintcore-surface.cpp','app/paint/gimpmypaintcore-brushfeature.hpp','app/paint/mypaintbrush-brush.hpp','app/core/gimpmypaintbrush-load.cpp','app/core/gimpbrush.c','app/core/gimpbrush-transform.c','app/paint/gimpmypaintcore.hpp','app/paint/gimpmypaintcore-drawablefeature.hpp','app/paint/gimpmypaintoptions.cpp','app/paint/mypaintbrush-stroke.cpp','app/core/gimpmypaintbrush.cpp','app/tools/gimpmypainttool.cpp']:
        path=old/rel
        if path.read_bytes()!=subprocess.check_output(['git','-C',str(old),'show',COMMIT+':'+rel]):raise SystemExit('Pinned behavior changed: '+rel)
        checks[rel]=sha(path)
    with tempfile.TemporaryDirectory(prefix='painter-pipe-capture-') as tmp:
        work=pathlib.Path(tmp);obj=work/'capture-pipe.o';binary=work/'capture-pipe'
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True))
        command=[os.environ.get('CXX','c++'),'-std=c++14','-g','-O2','-include','type_traits','-DPIPE_LEGACY','-I'+str(old),'-I'+str(old/'app'),*flags,'-c',str(FIX/'capture-pipe.cpp'),'-o',str(obj)]
        c=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (out/'compile.log').write_bytes(c.stdout);c.check_returncode()
        cflags=pathlib.Path(os.environ['LEGACY_BUILD_ROOT'],'gimp-cflags.txt').read_text().strip()
        plan=subprocess.check_output(['make','-n','-W','test-xcf.o','test-xcf','CFLAGS='+cflags,'CXXFLAGS=-g -O2 -include type_traits','LIBS=../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0'],cwd=old/'app/tests',text=True)
        link=next(x for x in plan.splitlines() if '--mode=link' in x);link=link[link.index('/bin/bash'):].replace('-o test-xcf test-xcf.o',f'-o {shlex.quote(str(binary))} {shlex.quote(str(obj))}')
        l=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'link.log').write_bytes(l.stdout);l.check_returncode()
        env=dict(os.environ);profile=work/'profile';profile.mkdir();env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(old),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(old),'GIMP2_DIRECTORY':str(profile)})
        run=subprocess.run([str(binary),str(FIX/'session-base.myb')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        values=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'PIPE_'))+b'\n'
        for name,data in [('runtime.log',run.stdout),('runtime.stderr',run.stderr),('build.log',c.stdout+l.stdout),('link.sh',link.encode()),('pipe-values.tsv',values)]:
            (out/(name+'.gz')).write_bytes(gzip.compress(data,mtime=0))
        report={'source_commit':COMMIT,'exit_code':run.returncode,'values_sha256':hashlib.sha256(values).hexdigest(),'compile_command':command,'harness_sha256':sha(FIX/'capture-pipe.cpp'),'behavior_sha256':checks,'binary_sha256':sha(work/'.libs/capture-pipe'),'archives_sha256':{rel:sha(old/rel) for rel in ['app/paint/libapppaint.a','app/core/libappcore.a']},'records':len(values.splitlines()),'cold_limitation':'First selector last_coords on each newly created Surface is not read by the observer; constant selector warms it. Native built-in selectors never read last_coords. Static old core storage defines its otherwise uninitialized options pointer.'}
        if expected is not None:report['reproduced_fixture']=values==expected
        (out/'capture.json').write_text(json.dumps(report,indent=2)+'\n');run.check_returncode()
        if expected is not None and values!=expected:raise SystemExit('Old fixture reproduction differed')
        print('Captured',len(values),'bytes')
if __name__=='__main__':main()
