#!/usr/bin/env python3
"""Run native ordinary paper kernels and exact old Paintbrush/Smudge/Fill replay.
Hold /workspace/shared/gimp-painter-build.lock; this runner never acquires a nested lock.
"""
import argparse,gzip,hashlib,json,os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--asan',action='store_true');p.add_argument('--report',type=Path,required=True);a=p.parse_args();build=a.build.resolve();binaries=build/('paper-sanitizers' if a.asan else 'app/tests')
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GIMP3_DIRECTORY':str(build/'paper-native-profile'),'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
Path(env['GIMP3_DIRECTORY']).mkdir(exist_ok=True)
def normalize(raw,kind):
    out=[]
    for row in raw.splitlines():
        fields=row.split()
        if not fields:continue
        if kind=='paper':take=fields[0] in [b'PAPER_STROKE',b'PAPER_CAPTURE_COMPLETE'] or (fields[0]==b'PAPER_MASK' and fields[2]==b'1')
        elif kind=='smudge':take=fields[0] in [b'SMUDGE',b'SMUDGE_CAPTURE_COMPLETE']
        else:take=fields[0] in [b'BRUSH',b'FILL_BRUSH_CAPTURE_COMPLETE']
        if take:out.append(row)
    return b'\n'.join(out)+b'\n'
cases=[('painter-paper',[],{},None,None),('painter-paper-trace',[],{},'legacy-paper','paper'),('painter-paper-trace',[],{'PAINTER_PAPER_MODES_FIXTURE':'1'},'legacy-paper-modes','paper')]
cases += [('painter-smudge-trace',args,{'PAINTER_SMUDGE_PAPER_FIXTURE':'1'},'legacy-paper-smudge','smudge') for args in [[],['owned']]]
cases += [('painter-fill-brush-trace',args,{'PAINTER_FILL_PAPER_FIXTURE':'1'},'legacy-paper-fill','fill') for args in [[],['queued'],['async']]]
report={'scope':'Native paper12cases, real pinned paper masks and24base/384defined-mode Paintbrush scenes,48Smudge scenes through2owner routes,12Fill scenes through3owner routes; no GUI/tablet/other-platform claim','sanitizers':a.asan,'runs':[]}
for name,args,extra,fixture,kind in cases:
    binary=binaries/name;run=subprocess.run([str(binary),*args],cwd=build,env={**env,**extra},stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=180)
    record={'binary':name,'args':args,'fixture':fixture,'exit_code':run.returncode,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'stderr':run.stderr.decode()}
    if fixture:
        expected=normalize(gzip.decompress((root/'migration/fixtures'/fixture/'pixels.tsv.gz').read_bytes()),kind);actual=normalize(run.stdout,kind)
        record.update(exact=expected==actual,expected_bytes=len(expected),records=len(expected.splitlines()),actual_sha256=hashlib.sha256(actual).hexdigest(),expected_sha256=hashlib.sha256(expected).hexdigest())
    else:record['stdout']=run.stdout.decode()
    report['runs'].append(record);print(name,args,run.returncode,record.get('exact'),flush=True)
    if run.returncode or record.get('exact') is False:break
report['success']=len(report['runs'])==len(cases) and all(r['exit_code']==0 and r.get('exact',True) for r in report['runs'])
a.report.write_text(json.dumps(report,indent=2)+'\n')
if not report['success']:raise SystemExit(1)
