#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused scalar Gray equations; hold the shared migration build lock."""
import argparse,hashlib,json,os,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
r=Path(__file__).resolve().parents[2];b=a.build.resolve();out=b/'mypaint-gray-equations-sanitizers';out.mkdir(exist_ok=True)
source='app/paint/painter-mypaint-surface/tests/gray-alpha-equations.cpp'
names=[source,'app/paint/painter-mypaint-surface/gray-alpha-pixels.hpp','app/paint/painter-mypaint-surface/legacy-pixel-modes.hpp','app/paint/painter-mypaint-surface/legacy-pixel.hpp']
hashes={n:hashlib.sha256((r/n).read_bytes()).hexdigest() for n in names}
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
exe=out/'gray-alpha-equations';command=['c++','-std=c++14','-g','-O1','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-I'+str(r/'app'),str(r/source),'-o',str(exe),*flags]
subprocess.run(command,cwd=b,check=True);env=dict(os.environ);env.update({'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'})
run=subprocess.run([str(exe)],env=env,cwd=b,capture_output=True,text=True)
changed=[n for n,h in hashes.items() if hashlib.sha256((r/n).read_bytes()).hexdigest()!=h]
report={'scope':'180 synthetic scalar Gray-alpha versus existing RGBA red/alpha equations and weighted sampling; exact two-byte alpha accessor. No old Gray-alpha equivalence claim.','sanitizers':['address','undefined','float-cast-overflow'],'leak_detection':False,'compile_command':command,'sources_sha256':hashes,'changed_during_run':changed,'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
a.report.write_text(json.dumps(report,indent=2)+'\n');print(run.stdout);raise SystemExit(run.returncode or bool(changed))
