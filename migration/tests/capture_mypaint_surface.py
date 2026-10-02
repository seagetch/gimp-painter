#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Capture real pinned Surface/generated-mask code, with synthetic stimuli.
Source the legacy build env.sh first. Reuses archives, never edits/rebuilds old
feature sources. This is not an old full drawable/paint-session pixel oracle.
"""
import argparse, gzip, hashlib, json, os, pathlib, shlex, subprocess, tempfile
ROOT=pathlib.Path(__file__).resolve().parents[2]
COMMIT='afa43fae3e920210146abed514f136fd49f671b5'
def sha(data): return hashlib.sha256(data).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--legacy',type=pathlib.Path,default=ROOT.parent/'gimp-painter-legacy');p.add_argument('--output',type=pathlib.Path,default=ROOT/'migration/fixtures/legacy-mypaint-surface');a=p.parse_args()
    old=a.legacy.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
    sources=['app/paint/gimpmypaintcore-surface.cpp','app/paint/gimpmypaintcore-surface.hpp','app/paint/gimpmypaintcore-brushfeature.hpp','app/paint/gimpmypaintcore-drawablefeature.hpp','app/paint-funcs/mypaint-brushmodes.hpp','app/base/pixel.hpp','app/core/gimpbrush-transform.c','app/core/gimpbrushgenerated.c','app/core/gimpbrush.c','app/base/temp-buf.c']
    checks={}
    for rel in sources:
        b=(old/rel).read_bytes()
        if b!=subprocess.check_output(['git','-C',str(old),'show',COMMIT+':'+rel]):raise SystemExit('Oracle behavior source changed: '+rel)
        checks[rel]=sha(b)
    report={'source_commit':COMMIT,'behavior_source_sha256':checks,'scope':'Actual old TempBuf Surface and GimpBrushGenerated transform; synthetic pixels/dabs/resources. No port rendering/decoder. Not old full drawable paint-session/Undo or GUI/tablet evidence. Reuses documented legacy build overlays.','archives':{rel:sha((old/rel).read_bytes()) for rel in ['app/paint/libapppaint.a','app/core/libappcore.a','app/base/libappbase.a','app/paint-funcs/libapppaint-funcs.a']},'captures':{}}
    with tempfile.TemporaryDirectory(prefix='painter-surface-oracle-') as tmp:
        work=pathlib.Path(tmp)
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3'],text=True))
        cflags=pathlib.Path(os.environ['LEGACY_BUILD_ROOT'],'gimp-cflags.txt').read_text().strip()
        plan=subprocess.check_output(['make','-n','-W','test-xcf.o','test-xcf','CFLAGS='+cflags,'CXXFLAGS=-g -O2 -include type_traits','LIBS=../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0'],cwd=old/'app/tests',text=True)
        line=next(x for x in plan.splitlines() if '--mode=link' in x);line=line[line.index('/bin/bash'):]
        for part,count in [('surface',148),('generated',72)]:
            name='capture-'+part;obj=work/(name+'.o');binary=work/name
            compile=[os.environ.get('CXX','c++'),'-std=c++14','-g','-O2','-include','type_traits','-I'+str(old),'-I'+str(old/'app'),*flags,'-c',ROOT/('migration/fixtures/legacy-mypaint-surface/'+name+'.cpp'),'-o',obj]
            c=subprocess.run([str(x) for x in compile],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
            link=line.replace('-o test-xcf test-xcf.o',f'-o {shlex.quote(str(binary))} {shlex.quote(str(obj))}')
            l=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT,check=True)
            run=subprocess.run([str(binary)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
            tags=(b'PIX ',b'SAMPLE ',b'DAB ') if part=='surface' else (b'GENERATED ',)
            values=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(tags))+b'\n'
            if len(values.splitlines())!=count:raise SystemExit('Incomplete '+part+' capture')
            files=[(part+'-values.tsv',values),(part+'-capture.log',run.stdout),(part+'-capture.stderr',run.stderr),(part+'-build.log',c.stdout+l.stdout),(part+'-link.sh',link.encode())]
            for name,data in files:(out/(name+'.gz')).write_bytes(gzip.compress(data,mtime=0))
            if part=='surface':(out/'shape-masks.tsv').write_bytes(b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(b'MASK '))+b'\n')
            report['captures'][part]={'records':count,'binary_sha256':sha((work/'.libs'/binary.name).read_bytes()),'values_sha256':sha(values),'compile_command':[str(x) for x in compile]}
    (out/'capture.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
