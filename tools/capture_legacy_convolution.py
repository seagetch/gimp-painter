#!/usr/bin/env python3
"""Capture convolution using pinned old GIMP and its unmodified plug-in.

The harness writes synthetic tiles and calls the real PDB or real FilterLayer.
Observation hooks copy input/shadow/final bytes only. No substitute convolution
produces expected pixels. Source the restored legacy env.sh before --capture.
"""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import hashlib
import gzip
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

import capture_legacy_filter_context as context

ROOT = Path(__file__).resolve().parents[1]
COMMIT = context.COMMIT
SOURCES = tuple(dict.fromkeys(context.SOURCES + (
    "plug-ins/common/convolution-matrix.c", "app/core/gimpparamspecs.c",
    "app/plug-in/gimppluginprocedure.c", "app/plug-in/gimppluginmanager-call.c",
    "libgimp/gimppixelrgn.c", "libgimp/gimpdrawable.c")))
sha = context.sha


def cases(diagnostics=False):
    identity = [0.] * 25; identity[12] = 1.
    asymmetric = [0.] * 25; asymmetric[5] = 1.
    mixed = [0.] * 25
    for i, v in ((0, .125), (7, -1.25), (12, 2.75), (19, .625), (24, -.25)): mixed[i] = v
    positive = [1.] * 25
    zero = [0.] * 25; zero[7] = -1.; zero[12] = 1.
    negative = [0.] * 25; negative[12] = -1.
    rounding = identity.copy(); rounding[12] = 1.0000000596046446
    kernels = [
        ("identity", identity, 1., 0., [1]*5),
        ("asymmetric", asymmetric, 1., 0., [1]*5),
        ("mixed", mixed, 1.375, 13.25, [1]*5),
        ("positive-normalized", positive, 25., 0., [1]*5),
        ("zero-normalized", zero, 1., 128., [1]*5),
        ("negative-normalized", negative, 1., 255., [1]*5),
        ("selectors", mixed, 2.25, -9.75, [0, 1, 0, 1, 0]),
        ("none", mixed, 1.375, 27.5, [0]*5),
        ("float-rounding", rounding, 1.0000000596046446, .49999999, [1]*5),
    ]
    result = []
    def add(components, width, height, kernel, border=0, alpha=0, selection=0, live=0, final=0, za=0, suffix=""):
        label,matrix,divisor,offset,channels = kernel
        ident = f"{'live' if live else 'pdb'}-c{components}-{width}x{height}-{label}-b{border}-a{alpha}-s{selection}-f{final}-z{za}{suffix}"
        result.append(dict(id=ident, components=components, width=width, height=height,
            kernel=label, matrix=matrix, divisor=divisor, offset=offset, channels=channels,
            border=border, alpha_weight=alpha, selection=selection, live=live,
            final_selection=final, zero_alpha=za,
            expected_status=3 if width >= 3 and height >= 3 else 0))
    if diagnostics:
        for components in (2, 4):
            for repeat in range(3):
                add(components,9,8,kernels[2],border=2,selection=4,suffix=f"-repeat{repeat}")
                add(components,9,8,kernels[2],border=0,selection=3,suffix=f"-repeat{repeat}")
        return result
    for components in range(1,5):
        for kernel in kernels:
            for border in range(3):
                for alpha in (0,1): add(components,9,8,kernel,border,alpha)
        for width,height in ((1,1),(2,3),(3,3),(4,4)):
            for border in ((0,) if width < 3 else range(3)):
                add(components,width,height,kernels[1],border)
        for selection in (1,2,4,5):
            for border in (0,1): add(components,9,8,kernels[2],border,selection=selection)
        if components in (2,4):
            for border in range(3):
                for alpha in (0,1): add(components,9,8,kernels[2],border,alpha,za=1)
    for components in (2,4):
        for selection,final in ((0,0),(1,0),(2,0),(0,1),(0,2),(1,2)):
            add(components,53,41,kernels[2],0,1,selection,1,final)
    return result


def write_header(work, tests):
    fields = ('width','height','components','alpha_weight','border','selection','live','final_selection','zero_alpha')
    lines = ['typedef struct { const char *id; int '+', '.join(fields)+'; double matrix[25], divisor, offset; gint32 channels[5]; } CaptureCase;',
             'static const CaptureCase capture_cases[] = {']
    for case in tests:
        lines.append('  {' + ','.join([json.dumps(case['id'])]+[str(case[k]) for k in fields]+[
            '{'+','.join(str(v) for v in case['matrix'])+'}',str(case['divisor']),str(case['offset']),
            '{'+','.join(str(v) for v in case['channels'])+'}'])+'},')
    lines.append('};')
    (work/'legacy-convolution-cases.h').write_text('\n'.join(lines)+'\n')


def build(source, work, out):
    cwd = source/'app/tests'
    dry = subprocess.check_output(['make','-n','-W','test-xcf.c','test-xcf'],cwd=cwd,text=True)
    compile_line = next(line for line in dry.splitlines() if ' -c -o test-xcf.o ' in line)
    flags = shlex.split(compile_line[compile_line.index('/usr/bin/gcc '):].split(' -MT ')[0])[1:]
    includes = [flag for flag in flags if flag.startswith(('-I','-D')) or flag == '-pthread']
    includes += ['-I'+str(source/'app/core'),'-I'+str(source/'app/pdb'),'-I'+str(work)]
    paths = context.instrument(source,work,out)
    commands,objects = [],[]
    for path in [ROOT/'migration/tests/legacy-convolution-capture.c']+paths:
        output=work/(path.stem+'.o')
        cmd = (['g++','-std=c++17','-include','type_traits'] if path.suffix == '.cpp' else ['gcc'])+includes
        cmd += ['-g','-O1','-fcommon','-c',str(path),'-o',str(output)]
        commands.append(cmd); objects.append(str(output))
    link_line = next(line for line in dry.splitlines() if '--mode=link ' in line)
    cmd = shlex.split(link_line[link_line.index('/bin/bash '):])
    index=cmd.index('-o'); executable=work/'legacy-convolution'
    cmd[index:index+3]=['-o',str(executable)]+objects
    cmd.insert(cmd.index('../../app/core/libappcore.a'),'../../app/presets/libapppresets.a')
    cmd += ['-lstdc++']; commands.append(cmd)
    with (out/'build.log').open('w') as log:
        for cmd in commands:
            log.write(shlex.join(cmd)+'\n'); log.flush()
            subprocess.run(cmd,cwd=cwd,stdout=log,stderr=subprocess.STDOUT,check=True)
    return executable,commands


def run(executable,prefix,out,timeout):
    with tempfile.TemporaryDirectory(prefix='gimp-old-convolution-profile-') as tmp:
        home=Path(tmp)
        for name in ('profile','plugins','empty','temp','cache','config','data'): (home/name).mkdir(mode=0o700)
        (home/'plugins/convolution-matrix').symlink_to(prefix/'lib/gimp/2.0/plug-ins/convolution-matrix')
        (home/'profile/gimprc').write_text(''.join(f'({name} {json.dumps(str(home/path))})\n' for name,path in (
            ('plug-in-path','plugins'),('module-path','empty'),('interpreter-path','empty'),
            ('environ-path','empty'),('temp-path','temp'),('swap-path','temp'))))
        env=dict(os.environ)
        for key in tuple(env):
            if key.startswith('GIMP_TESTING_') or key in ('GIMP_PLUGIN_DEBUG','GIMP_PLUGIN_DEBUG_WRAP'): del env[key]
        env.update(HOME=str(home),GIMP2_DIRECTORY=str(home/'profile'),XDG_CONFIG_HOME=str(home/'config'),
                   XDG_CACHE_HOME=str(home/'cache'),XDG_DATA_HOME=str(home/'data'))
        command=[str(executable),str(out)]
        with (out/'capture.log').open('w') as log:
            result=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=timeout)
        return result.returncode,command


def package(out,tests,code):
    results={r[0]:r[1:] for r in (line.split('\t') for line in (out/'results.tsv').read_text().splitlines())}
    bounds={r[0]:list(map(int,r[1:])) for r in (line.split('\t') for line in (out/'bounds.tsv').read_text().splitlines())}
    rois={r[0]:list(map(int,r[1:])) for r in (line.split("\t") for line in (out/"rois.tsv").read_text().splitlines())}
    errors=[]; manifest=[]; offsets={}
    with (out/'pixels.bin').open('wb') as data:
        for test in tests:
            case=dict(test); ident=case['id']; case['buffers']={}
            if ident not in results:
                errors.append('Missing completed case '+ident); continue
            status,components,starts,ends,merges=map(int,results[ident])
            case.update(status=status,output_components=components,starts=starts,ends=ends,merges=merges,initial_roi=rois.get(ident))
            # GIMP_PDB_SUCCESS is 3 in old GimpPDBStatusType; execution error is 0.
            expected=3 if test['width']>=3 and test['height']>=3 else 0
            if status!=expected: errors.append('Unexpected old status '+ident)
            for path in sorted(out.glob(ident+'-*.raw')):
                label=path.name[len(ident)+1:-4]; raw=path.read_bytes(); digest=hashlib.sha256(raw).hexdigest()
                key=(digest,len(raw))
                if key not in offsets:
                    offsets[key]=data.tell(); data.write(raw)
                case['buffers'][label]=dict(offset=offsets[key],length=len(raw),sha256=digest)
            for key in ('source','output'):
                if key not in case['buffers']: errors.append('Missing buffer '+ident+' '+key)
            if expected==3:
                if merges != 1: errors.append('Unexpected merge count '+ident)
                for key in ('merge1-input','merge1-shadow','merge1-mask','merge1-output'):
                    if key not in case['buffers']: errors.append('Missing merge buffer '+ident+' '+key)
                case['merge_bounds']=bounds.get(ident+'-merge1')
            if case['live'] and (starts,ends)!=(1,1): errors.append('Missing actual FilterLayer start/end '+ident)
            manifest.append(case)
    if code: errors.append('Harness exit '+str(code))
    if 'LEGACY_CONVOLUTION_CAPTURE_COMPLETE' not in (out/'capture.log').read_text(errors='replace'):
        errors.append('Missing completion marker')
    doc=dict(schema_version=1,pixel_file='pixels.bin',pixel_sha256=sha(out/'pixels.bin'),cases=manifest,
        buffer_description='Raw interleaved bytes, exact old tile data; offsets deduplicate identical buffers',
        channel_order=['gray','red','green','blue','alpha'],
        bounds_columns=['width','height','offset_x','offset_y','selection_empty','selection_x1','selection_y1',
                        'selection_x2','selection_y2','selected','roi_x1','roi_y1','roi_x2','roi_y2'])
    (out/'fixtures.json').write_text(json.dumps(doc,indent=2)+'\n')
    if not errors:
        for path in out.glob('*.raw'): path.unlink()
        (out/'capture-plan.json').unlink()
    return errors,len(manifest)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',action='store_true')
    parser.add_argument('--case-pattern',default='')
    parser.add_argument('--diagnostics',action='store_true')
    parser.add_argument('--output',type=Path,default=ROOT/'migration/fixtures/legacy-convolution')
    parser.add_argument('--legacy-source',type=Path,default=Path('/workspace/shared/gimp-legacy-convolution/source/gimp'))
    parser.add_argument('--legacy-prefix',type=Path,default=Path('/workspace/shared/gimp-legacy-build/prefix'))
    parser.add_argument('--work',type=Path,default=Path('/workspace/shared/gimp-legacy-convolution/harness'))
    parser.add_argument('--timeout',type=float,default=300)
    args=parser.parse_args(); out=args.output.resolve(); work=args.work.resolve(); source=args.legacy_source.resolve()
    if (out/'capture-report.json').exists(): raise SystemExit('Sealed evidence already exists; choose new --output')
    assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()==COMMIT
    hashes={}
    for name in SOURCES:
        pinned=subprocess.check_output(['git','-C',str(source),'show',COMMIT+':'+name])
        hashes[name]=sha(source/name)
        assert hashlib.sha256(pinned).hexdigest()==hashes[name], 'Changed oracle source '+name
    out.mkdir(parents=True,exist_ok=True); work.mkdir(parents=True,exist_ok=True)
    tests=[c for c in cases(args.diagnostics) if args.case_pattern in c['id']]; write_header(work,tests)
    if not tests: raise SystemExit('No matching cases')
    (out/'capture-plan.json').write_text(json.dumps(tests,indent=2)+'\n')
    if not args.capture: print('Prepared',len(tests),'real old PDB/FilterLayer cases'); return 0
    started=datetime.now(timezone.utc).isoformat()
    executable,commands=build(source,work,out)
    binaries=[work/'.libs/legacy-convolution',args.legacy_prefix/'bin/gimp-2.8',
              args.legacy_prefix/'lib/gimp/2.0/plug-ins/convolution-matrix']
    binary_hashes={str(path):sha(path) for path in binaries}
    code,command=run(executable,args.legacy_prefix,out,args.timeout)
    errors,count=package(out,tests,code)
    manifest=json.loads((out/'fixtures.json').read_text())['cases']
    index={c['id']:c for c in manifest}
    alpha_pairs=[]; forced_borders=[]
    for case in manifest:
        if case['alpha_weight']==0:
            other=index.get(case['id'].replace('-a0-', '-a1-'))
            if other:
                alpha_pairs.append(case['buffers']['output']['sha256']==other['buffers']['output']['sha256'])
        if case['components'] in (1,3) and case['border']==0:
            for border in (1,2):
                other=index.get(case['id'].replace('-b0-', f'-b{border}-'))
                if other:
                    forced_borders.append(case['buffers']['output']['sha256']==other['buffers']['output']['sha256'])
    for filename in ('build.log','capture.log'):
        path=out/filename
        (out/(filename+'.gz')).write_bytes(gzip.compress(path.read_bytes(),mtime=0))
        path.unlink()
    report=dict(schema_version=1,status='passed' if not errors else 'failed',source_commit=COMMIT,
        paired_alpha_observations=dict(pairs=len(alpha_pairs),identical=sum(alpha_pairs)),
        no_alpha_border_observations=dict(pairs=len(forced_borders),identical=sum(forced_borders)),
        started_utc=started,finished_utc=datetime.now(timezone.utc).isoformat(),source_sha256=hashes,
        executable_sha256=binary_hashes,build_commands=commands,command=command,exit_code=code,
        diagnostics=args.diagnostics,case_count=count,expected_cases=len(tests),errors=errors,
        capture_script_sha256=sha(Path(__file__)),harness_sha256=sha(ROOT/'migration/tests/legacy-convolution-capture.c'),
        instrumentation_sha256=sha(out/'observation-hooks.patch'),
        profile_policy='Fresh isolated profile with only the original convolution-matrix plug-in',
        files_sha256={p.name:sha(p) for p in sorted(out.iterdir()) if p.is_file()})
    (out/'capture-report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],count,errors); return 0 if not errors else 1

if __name__=='__main__': raise SystemExit(main())
