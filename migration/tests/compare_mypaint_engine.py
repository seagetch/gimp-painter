#!/usr/bin/env python3
"""Compile and compare pinned old evaluator against the C++14 engine port.
This is real old-source execution with deterministic synthetic Surface responses,
not old application's drawable rendering or tablet/UI testing.
Source the exact legacy env.sh before invoking; no network access is needed.
"""
import argparse, gzip, hashlib, json, os, pathlib, shlex, subprocess, tempfile
ROOT = pathlib.Path(__file__).resolve().parents[2]
def run(args, **kwargs):
    return subprocess.run([str(a) for a in args], check=True, **kwargs)
def main():
    p = argparse.ArgumentParser()
    p.add_argument('--legacy', type=pathlib.Path, default=ROOT.parent/'gimp-painter-legacy')
    p.add_argument('--output', type=pathlib.Path, default=ROOT/'migration/tests/mypaint-engine-comparison.json')
    args = p.parse_args(); legacy = args.legacy.resolve()
    commit = 'afa43fae3e920210146abed514f136fd49f671b5'
    checks = {}
    for rel in ['app/paint/mypaintbrush-brush.hpp','app/paint/mypaintbrush-surface.hpp','app/core/mypaintbrush-mapping.hpp','libgimpcolor/gimpcolorspace.c']:
        expected = subprocess.check_output(['git','-C',str(legacy),'show',commit+':'+rel])
        actual = (legacy/rel).read_bytes()
        if expected != actual: raise SystemExit('Pinned oracle source was modified: '+rel)
        checks[rel] = hashlib.sha256(actual).hexdigest()
    pkg = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gimp-2.0','json-glib-1.0'],text=True))
    with tempfile.TemporaryDirectory(prefix='painter-mypaint-oracle-') as tmp:
        work = pathlib.Path(tmp)
        includes = ['-I'+str(ROOT/'app'),'-I'+str(ROOT/'app/paint/painter-mypaint'),'-I'+str(ROOT/'build-debian13/app/paint/painter-brush-settings')]
        common = [ROOT/'app/paint/painter-mypaint/tests/engine-trace.cpp',ROOT/'app/paint/painter-mypaint/resource.cpp',ROOT/'app/painter/gimp-painter-error.cpp']
        outputs = {}; stats = {}
        for name in ['legacy','port']:
            command = [os.environ.get('CXX','g++'),'-std=c++14','-O2','-fno-rtti','-fexceptions',*includes,*common]
            if name == 'legacy': command += ['-DPAINTER_LEGACY_ORACLE','-I'+str(legacy),'-I'+str(legacy/'app')]
            else: command += [ROOT/'app/paint/painter-mypaint/engine.cpp']
            command += ['-o',work/name,*pkg]
            run(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            with (work/(name+'.trace')).open('wb') as out:
                result = run([work/name,ROOT/'data/painter-mypaint-brushes'],stdout=out,stderr=subprocess.PIPE)
            outputs[name] = (work/(name+'.trace')).read_bytes(); stats[name] = result.stderr.decode().strip()
        a,b = outputs['legacy'],outputs['port']
        (ROOT/'migration/fixtures/legacy-mypaint/engine.trace.gz').write_bytes(gzip.compress(a,mtime=0))
        if a != b:
            import difflib
            (ROOT/'migration/tests/mypaint-engine-difference.diff').write_text(''.join(difflib.unified_diff(a.decode().splitlines(True),b.decode().splitlines(True),fromfile='legacy',tofile='port')))
        report = {'source_commit':commit,'oracle_source_sha256':checks,'equal':a==b,'legacy_sha256':hashlib.sha256(a).hexdigest(),'port_sha256':hashlib.sha256(b).hexdigest(),'trace_bytes':len(a),'stats':stats,'scope':'Compiled original Brush evaluator and installed GIMP 2.8 color library versus port. Shared port Resource decoder; deterministic synthetic Surface responses, actual 177 brush settings plus extended synthetic scenario. Not old application drawable rendering or GUI/tablet validation.'}
        args.output.write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps(report,indent=2))
        if a != b: raise SystemExit(1)
if __name__ == '__main__': main()
