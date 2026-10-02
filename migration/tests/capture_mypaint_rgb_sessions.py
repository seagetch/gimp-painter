#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Optional reproduction of the warmed real old RGB-u8 full-session oracle.

Source the legacy build environment first. Never edits/rebuilds old feature
sources or archives. Cold crash probes are intentionally not repeated by this
runner. Its temporary harness substitutes only the recorded absolute input path.
"""
import argparse,gzip,hashlib,json,os,pathlib,shlex,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
FIX=ROOT/'migration/fixtures/legacy-mypaint-rgb-session'
def main():
    p=argparse.ArgumentParser();p.add_argument('--legacy',type=pathlib.Path,default=ROOT.parent/'gimp-painter-legacy');p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
    old=a.legacy.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    reference=json.loads((FIX/'old-capture-report.json').read_text());checks={}
    for rel,expected in reference['source_feature_sha256'].items():
        b=(old/rel).read_bytes();actual=hashlib.sha256(b).hexdigest()
        if actual!=expected:raise SystemExit('Pinned behavior source changed: '+rel)
        checks[rel]=actual
    with tempfile.TemporaryDirectory(prefix='painter-rgb-session-recapture-') as tmp:
        work=pathlib.Path(tmp);base=work/'session-base.myb';base.write_bytes((FIX/base.name).read_bytes())
        source=work/'capture-rgb.cpp';source.write_text((FIX/source.name).read_text().replace('/workspace/shared/painter-mypaint-rgb-oracle/session-base.myb',str(base)))
        obj=work/'capture-rgb.o';binary=work/'capture-rgb'
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True))
        compile=[os.environ.get('CXX','c++'),'-std=c++14','-g','-O2','-include','type_traits','-I'+str(old),'-I'+str(old/'app'),*flags,'-c',str(source),'-o',str(obj)]
        c=subprocess.run(compile,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
        cflags=pathlib.Path(os.environ['LEGACY_BUILD_ROOT'],'gimp-cflags.txt').read_text().strip()
        plan=subprocess.check_output(['make','-n','-W','test-xcf.o','test-xcf','CFLAGS='+cflags,'CXXFLAGS=-g -O2 -include type_traits','LIBS=../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0'],cwd=old/'app/tests',text=True)
        link=next(x for x in plan.splitlines() if '--mode=link' in x);link=link[link.index('/bin/bash'):].replace('-o test-xcf test-xcf.o',f'-o {shlex.quote(str(binary))} {shlex.quote(str(obj))}')
        l=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
        env=dict(os.environ);profile=work/'profile';profile.mkdir()
        env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(old),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(old),'GIMP2_DIRECTORY':str(profile)})
        run=subprocess.run([str(binary)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
        values=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'RGB_SESSION_'))+b'\n'
        expected=gzip.decompress((FIX/'session-values.tsv.gz').read_bytes())
        for name,data in [('runtime.log',run.stdout),('runtime.stderr',run.stderr),('build.log',c.stdout+l.stdout),('link.sh',link.encode()),('values.tsv',values)]:
            (out/(name+'.gz')).write_bytes(gzip.compress(data,mtime=0))
        report={'equal':values==expected,'values_sha256':hashlib.sha256(values).hexdigest(),'compile_command':compile,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'source_overlay':'Only the absolute session-base.myb input path is replaced in a temporary harness copy.','behavior_sha256':checks,'binary_sha256':hashlib.sha256((work/'.libs/capture-rgb').read_bytes()).hexdigest()}
        (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        if not report['equal']:raise SystemExit('Warmed old oracle differs; preserved reproduction output for review')
        print('Exact reproduction:',len(values),'bytes')
if __name__=='__main__':main()
