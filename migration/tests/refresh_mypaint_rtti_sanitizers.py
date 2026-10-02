#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Refresh changed RTTI-only units of a completed private model harness.

Hold /workspace/shared/gimp-painter-build.lock. Instrumented sources and recorded
native ABI headers must remain identical. A compressed predecessor, commands,
and fresh runtime/hash checks record the new result. No production replacement.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);args=p.parse_args()
root=Path(__file__).resolve().parents[2];build=args.build.resolve();path=args.report.resolve()
raw=path.read_bytes();report=json.loads(raw)
if report['exit_code'] != 0:raise SystemExit('Previous runtime did not pass')
def sha(name):return hashlib.sha256((root/name).read_bytes()).hexdigest()
initial=report['sources_sha256'];current={n:sha(n) for n in initial}
changed={n for n in initial if initial[n]!=current[n]}
if not changed:raise SystemExit('No changed source requires refresh')
if not changed <= set(report['rtti_compatibility_only_sources']):raise SystemExit('Instrumented sources changed; full rebuild required')
if not set(report.get('changed_during_run',[])) <= set(report['rtti_compatibility_only_sources']):raise SystemExit('Prior build had a non-RTTI source/header change')
headers={**report['native_abi_headers_sha256'],**report.get('algorithm_headers_sha256',{})}
if any(sha(n)!=h for n,h in headers.items()):raise SystemExit('Native ABI header changed; full rebuild required')
prior=path.with_name(path.stem+'-pre-refresh.json.gz');prior.write_bytes(gzip.compress(raw,mtime=0))
commands=[]
for name in sorted(changed):
    candidates=[c for c in report['commands'] if '-c' in c and
                (build/c[c.index('-c')+1]).resolve()==root/name]
    if len(candidates)!=1:raise SystemExit('Cannot identify unique compile command: '+name)
    commands += candidates
archives=[c for c in report['commands'] if c[:2]==['ar','crsT']]
link=report['commands'][-1]
exe=Path(link[link.index('-o')+1]);assert exe.is_relative_to(build)
for command in commands:subprocess.run(command,cwd=build,check=True)
for command in archives:
    archive=Path(command[2]);assert archive.is_relative_to(build)
    archive.unlink(missing_ok=True);subprocess.run(command,cwd=build,check=True)
subprocess.run(link,cwd=build,check=True)
env=dict(os.environ);env.update({'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
result=subprocess.run([str(exe)],cwd=build,env=env,capture_output=True,text=True)
if 'pipe_oracle' in report:
    from compare_mypaint_pipe import compare
    report['pipe_oracle']=compare(gzip.decompress((root/'migration/fixtures/legacy-mypaint-pipe/pipe-values.tsv.gz').read_bytes()),result.stdout.encode())
    result.returncode=result.returncode or int(not report['pipe_oracle']['equal'])
    result.stdout='\n'.join(line for line in result.stdout.splitlines() if not line.startswith('PIPE_'))+'\n'
for color_model in ('rgb','gray'):
    if color_model+'_oracle' not in report:continue
    prefix=color_model.upper()+'_SESSION_'
    actual=b'\n'.join(line.encode() for line in result.stdout.splitlines() if line.startswith(prefix))+b'\n'
    expected=gzip.decompress((root/('migration/fixtures/legacy-mypaint-'+color_model+'-session/session-values.tsv.gz')).read_bytes())
    report[color_model+'_oracle']={'equal':actual==expected,'records':len(actual.splitlines()),'bytes':len(actual),'sha256':hashlib.sha256(actual).hexdigest()}
    result.stdout='\n'.join(line for line in result.stdout.splitlines() if not line.startswith(prefix))+'\n'
    result.returncode=result.returncode or int(actual!=expected)
report['refresh']={'predecessor':prior.name,'predecessor_sha256':hashlib.sha256(prior.read_bytes()).hexdigest(),'prior_changed_during_run':report.get('changed_during_run',[]),'rtti_sources_recompiled':sorted(changed),'commands':commands+archives+[link],'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
report.update({'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr,'sources_sha256':current,'changed_during_run':[n for n,h in {**current,**headers}.items() if sha(n)!=h],'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()})
path.write_text(json.dumps(report,indent=2)+'\n');print(result.stdout)
if result.returncode:print(result.stderr,file=sys.stderr)
sys.exit(result.returncode or bool(report['changed_during_run']))
