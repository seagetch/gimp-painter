#!/usr/bin/env python3
"""Rebuild only the live test driver after its stack-backed static-GBytes fix.

The accepted instrumented production/helper corpus is reused only after all
captured input hashes match, except the one declared test include. No production
object is rebuilt or substituted; the exact original driver/link commands run.
"""
import argparse
import datetime
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[3]
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--overlay',type=Path,required=True)
p.add_argument('--build',type=Path,required=True)
a = p.parse_args(); out=a.overlay.resolve(); build=a.build.resolve()
original=json.loads((out/'report.json').read_text())
allowed=ROOT/'app/tests/test-filter-convolution.inc'
report={'status':'incomplete','scope':__doc__,'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'original_report_sha256':sha(out/'report.json'),'test_only_changed_source':str(allowed),
        'driver_sha256':sha(__file__),'commands':[]}
def publish(): (out/'live-recovery-report.json').write_text(json.dumps(report,indent=2)+'\n')
with Path('/workspace/shared/gimp-painter-build.lock').open('a') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX)
    before=original['input_sha256_before']
    changed=[name for name,value in before.items() if sha(name)!=value]
    if changed != [str(allowed)]: raise RuntimeError('Unexpected snapshot changes: '+repr(changed))
    report['input_sha256_before']={name:sha(name) for name in before}
    report['old_fixture_sha256']=before[str(allowed)]
    report['new_fixture_sha256']=sha(allowed)
    driver_object=str(out/'app_tests_gimp-filter-layer.p_test-gimp-filter-layer.c.o.o')
    def output(command):
        return command[command.index('-o')+1] if '-o' in command else None
    compiles=[c for c in original['commands'] if output(c)==driver_object]
    links=[c for c in original['commands'] if output(c)==str(out/'gimp-filter-layer')]
    if len(compiles)!=1 or len(links)!=1: raise RuntimeError('Cannot identify exact original compile/link commands')
    env=dict(os.environ)
    env['LD_LIBRARY_PATH']=os.pathsep.join(str(x) for x in build.glob('libgimp*') if x.is_dir())+os.pathsep+env.get('LD_LIBRARY_PATH','')
    env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(ROOT),GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
               GIMP_TESTING_PLUGINDIRS=str(build/'plug-ins/common'),GSETTINGS_BACKEND='memory',
               ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',
               UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    for label,command in [('compile-live-driver',compiles[0]),('link-live-driver',links[0])]:
        report['commands'].append(command)
        result=subprocess.run(command,cwd=build,env=env,capture_output=True,text=True,timeout=300)
        (out/(label+'-recovery.log')).write_text(result.stdout+result.stderr)
        if result.returncode: publish();raise RuntimeError(label+' failed')
    live=[c for c in original['commands'] if str(out/'gimp-filter-layer') in c and '-p' in c]
    if len(live)!=1: raise RuntimeError('Cannot identify exact original live command')
    expected={live[0][i+1].split('/')[-1] for i,arg in enumerate(live[0]) if arg=='-p'}
    report['commands'].append(live[0]);report['executable_sha256']=sha(out/'gimp-filter-layer')
    result=subprocess.run(live[0],cwd=build,env=env,capture_output=True,text=True,timeout=600)
    (out/'live-filter-recovery.log').write_text(result.stdout+result.stderr)
    actual=set(re.findall(r'^ok \d+ /gimp-filter-layer/(\S+)$',result.stdout,re.M))
    report.update(exit_code=result.returncode,cases=sorted(actual),expected_cases=sorted(expected),
                  stdout_sha256=hashlib.sha256(result.stdout.encode()).hexdigest(),
                  stderr_sha256=hashlib.sha256(result.stderr.encode()).hexdigest())
    report['input_sha256_after']={name:sha(name) for name in before}
    report['changed_during_run']=[name for name in before if report['input_sha256_before'][name]!=report['input_sha256_after'][name]]
    report['status']='passed' if result.returncode==0 and actual==expected and not report['changed_during_run'] else 'failed'
    report['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();publish()
    if report['status']!='passed': raise RuntimeError('Instrumented live recovery failed; see live-filter-recovery.log')
print(json.dumps({'status':report['status'],'cases':len(actual)}))
