#!/usr/bin/env python3
"""Focused no-GObject worker admission/storage tests. No environment dump."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];out=a.build.resolve()/'filter-admission-sanitizers';out.mkdir(exist_ok=True)
common=['app/painter/filter-raster.cpp','app/painter/filter-spool.cpp','app/painter/filter-scheduler.cpp']
headers=['app/painter/work-admission.hpp','app/painter/filter-raster.hpp','app/painter/filter-spool.hpp','app/painter/filter-scheduler.hpp']
runs=[];hashes={}
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
for name in ['work-admission','filter-raster','filter-spool','filter-scheduler']:
    sources=common+['app/painter/tests/test-'+name+'.cpp'];exe=out/name
    for src in sources+headers: hashes[src]=hashlib.sha256((root/src).read_bytes()).hexdigest()
    command=['g++','-std=c++14','-Wall','-Wextra','-Werror','-g','-O1','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','-frtti','-I'+str(root/'app/painter')]+[str(root/s) for s in sources]+flags+['-o',str(exe)]
    subprocess.run(command,check=True)
    result=subprocess.run([str(exe)],env=env,capture_output=True,text=True)
    runs.append(dict(target=name,command=command,exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr));print(result.stdout,flush=True)
changed=[s for s,h in hashes.items() if hashlib.sha256((root/s).read_bytes()).hexdigest()!=h]
report=dict(scope='Independent worker admission, disk quota, spool, raster and scheduler; system libraries uninstrumented',source_sha256=hashes,changed_after_compile=changed,sanitizers=['address','undefined'],leak_detection=False,runs=runs,all_passed=not changed and all(r['exit_code']==0 for r in runs))
a.report.write_text(json.dumps(report,indent=2)+'\n');sys.exit(not report['all_passed'])
