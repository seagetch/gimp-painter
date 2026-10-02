#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Run the real legacy application .myb reader without altering the checkout.
Requires the previously built pinned application. Source its env.sh first.
The link plan comes from make's dry run; its original test executable is neither
rebuilt nor replaced. All compiler output lives in a disposable directory.
"""
import argparse, gzip, hashlib, json, os, pathlib, shlex, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[2]
COMMIT = 'afa43fae3e920210146abed514f136fd49f671b5'
def main():
    p=argparse.ArgumentParser()
    p.add_argument('--legacy', type=pathlib.Path, default=ROOT.parent/'gimp-painter-legacy')
    p.add_argument('--output', type=pathlib.Path, default=ROOT/'migration/fixtures/legacy-mypaint')
    a=p.parse_args(); old=a.legacy.resolve(); out=a.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    checks={}
    for rel in ['app/core/gimpmypaintbrush-load.cpp','app/core/gimpmypaintbrush.cpp','app/core/gimpmypaintbrush-private.hpp','app/core/mypaintbrush-mapping.hpp','app/core/mypaintbrush-brushsettings.c']:
        b=(old/rel).read_bytes()
        if b != subprocess.check_output(['git','-C',str(old),'show',COMMIT+':'+rel]):
            raise SystemExit('Oracle behavior source changed: '+rel)
        checks[rel]=hashlib.sha256(b).hexdigest()
    with tempfile.TemporaryDirectory(prefix='painter-resource-oracle-') as tmp:
        work=pathlib.Path(tmp); obj=work/'capture-resource.o'; binary=work/'capture-resource'
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True))
        compile=[os.environ.get('CXX','c++'),'-std=c++14','-g','-O2','-include','type_traits','-I'+str(old),'-I'+str(old/'app'),*flags,'-c',ROOT/'migration/fixtures/legacy-mypaint/capture-resource.cpp','-o',obj]
        c=subprocess.run([str(x) for x in compile],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
        cflags=pathlib.Path(os.environ['LEGACY_BUILD_ROOT'],'gimp-cflags.txt').read_text().strip()
        plan=subprocess.check_output(['make','-n','-W','test-xcf.o','test-xcf','CFLAGS='+cflags,'CXXFLAGS=-g -O2 -include type_traits','LIBS=../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0'],cwd=old/'app/tests',text=True)
        line=next(x for x in plan.splitlines() if '--mode=link' in x)
        link=line[line.index('/bin/bash'):].replace('-o test-xcf test-xcf.o',f'-o {shlex.quote(str(binary))} {shlex.quote(str(obj))}')
        l=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
        run=subprocess.run([str(binary),str(ROOT/'data/painter-mypaint-brushes')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
        values=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'MYB '))+b'\n'
        if sum(x.startswith(b'MYB B ') for x in values.splitlines()) != 177: raise SystemExit('Incomplete resource capture')
        for name,data in [('resource-values.tsv',values),('resource-capture.log',run.stdout),('resource-capture.stderr',run.stderr),('build.log',c.stdout+l.stdout),('link.sh',link.encode())]:
            (out/(name+'.gz')).write_bytes(gzip.compress(data,mtime=0))
        report={'source_commit':COMMIT,'behavior_source_sha256':checks,'resources':177,'records':len(values.splitlines()),'binary_sha256':hashlib.sha256((work/'.libs/capture-resource').read_bytes()).hexdigest(),'core_archive_sha256':hashlib.sha256((old/'app/core/libappcore.a').read_bytes()).hexdigest(),'oracle_values_sha256':hashlib.sha256(values).hexdigest(),'compile_command':[str(x) for x in compile],'scope':'Actual rebuilt legacy application loader/private model; no port decoder. Headless resource capture, not rendered pixels or GUI/tablet. Uses documented legacy build overlays.'}
        (out/'resource-capture.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))
if __name__=='__main__': main()
