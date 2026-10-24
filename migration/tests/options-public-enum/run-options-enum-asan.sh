#!/bin/bash
set -euo pipefail
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock; flock 9
source /workspace/shared/gimp-clean-aggregate/env.sh
mkdir -p /workspace/shared/options-enum-private-asan
python3 /workspace/shared/build-options-enum-private.py --sanitize > /workspace/shared/options-enum-private-asan/build.log 2>&1
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export LD_LIBRARY_PATH=/workspace/shared/options-enum-private-asan:$(find /workspace/shared/gimp-clean-aggregate/build -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
export GIMP_TESTING_ABS_TOP_SRCDIR=/workspace/shared/gimp-clean-aggregate/source GIMP_TESTING_ABS_TOP_BUILDDIR=/workspace/shared/gimp-clean-aggregate/build GIMP_TESTING_PLUGINDIRS=/workspace/shared/gimp-clean-aggregate/build/plug-ins/common UI_TEST=yes
python3 /workspace/shared/gimp-clean-aggregate/source/tools/run-painter-xvfb.py python3 - <<'PY' > /workspace/shared/options-enum-private-asan/run.log 2>&1
from pathlib import Path
import subprocess,json,time,hashlib
O=Path('/workspace/shared/options-enum-private-asan');results=[]
for name in ['mypaint-enum-library-only','mypaint-enum-public','mypaint-options-mode']:
 exe=O/name;ldd=subprocess.check_output(['ldd',str(exe)],text=True);assert str(O/'libgimpbase-3.0.so.0') in ldd,ldd;(O/(name+'.ldd')).write_text(ldd);start=time.monotonic();p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=120);results.append({'name':name,'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr,'duration':time.monotonic()-start});(O/'results.json').write_text(json.dumps(results,indent=2)+'\n');print(name,p.returncode,p.stdout,p.stderr,flush=True);assert p.returncode==0
build=json.loads((O/'build.json').read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert all(sha(p)==h for p,h in build['dependency_objects'].items());(O/'post-inputs.json').write_text(json.dumps({'dependency_objects_unchanged':True},indent=2)+'\n')
PY
