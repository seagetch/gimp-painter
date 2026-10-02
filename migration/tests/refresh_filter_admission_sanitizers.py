#!/usr/bin/env python3
"""Refresh changed private Filter harness units; never replace production objects.

Run under /workspace/shared/gimp-painter-build.lock after the normal native build.
Headers or missing RTTI closure require the complete builder instead.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
from painter_sanitizer_scope import bridge_rtti_sources
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args();a.report=a.report.resolve()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();r=json.loads(a.report.read_text());old=a.report.read_bytes()
hashes=r['source_sha256'];changed=[s for s,h in hashes.items() if hashlib.sha256((root/s).read_bytes()).hexdigest()!=h]
if any(Path(s).suffix in ('.h','.hpp') for s in changed): raise SystemExit('Header changed; rerun full normal and private builds')
if not bridge_rtti_sources(root,build) <= set(r['sources']): raise SystemExit('New RTTI source; rerun full private builder')
refresh=[]
for command in r['commands']:
    if '-c' not in command: continue
    src=(build/command[command.index('-c')+1]).resolve().relative_to(root).as_posix()
    if src in changed: subprocess.run(command,cwd=build,check=True);refresh.append(src)
if set(refresh)!=set(changed): raise SystemExit('Changed source has no private compile command')
subprocess.run(r['commands'][-1],cwd=build,check=True)
start={s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in hashes}
env=dict(os.environ,GIMP_TESTING_ABS_TOP_SRCDIR=str(root),GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),GIMP_TESTING_PLUGINDIRS=str(build/'plug-ins/common'),UI_TEST='yes',ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
result=subprocess.run([str(build/'app/tests/gimp-filter-layer-asan')],cwd=build,env=env,capture_output=True,text=True)
r.update(source_sha256=start,changed_after_compile=[s for s,h in start.items() if hashlib.sha256((root/s).read_bytes()).hexdigest()!=h],exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr)
prior=a.report.with_suffix('.previous.json.gz')
if prior.exists(): raise SystemExit('Prior evidence already exists; preserve history before new refresh')
prior.write_bytes(gzip.compress(old,mtime=0));r['refresh']={'sources':refresh,'previous_report':str(prior.relative_to(root)),'previous_sha256':hashlib.sha256(old).hexdigest()}
if not r['changed_after_compile']:
    archive=a.report.with_suffix('.sources.tar.gz')
    with tarfile.open(archive,'w:gz') as t:
        for s in sorted(start):t.add(root/s,arcname=s)
    r['source_archive']={'path':str(archive.relative_to(root)),'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
a.report.write_text(json.dumps(r,indent=2)+'\n');print(result.stdout);print(result.stderr,file=sys.stderr);sys.exit(result.returncode or bool(r['changed_after_compile']))
