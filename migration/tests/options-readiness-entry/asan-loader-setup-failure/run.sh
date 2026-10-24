#!/bin/bash
set -euo pipefail
W=/workspace/shared/gimp-clean-aggregate;E=/workspace/shared/options-readiness/green
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 /workspace/shared/build-options-readiness-asan.py >"$E/asan-build.log" 2>&1
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
python3 "$W/source/tools/run-painter-xvfb.py" python3 - "$E" <<'PY' >"$E/asan-portrait.log" 2>&1
from pathlib import Path
import sys,subprocess,json,hashlib
E=Path(sys.argv[1]);p=subprocess.run([str(E/'asan-private/mypaint-options-readiness')],capture_output=True,text=True,timeout=90);(E/'asan-results.json').write_text(json.dumps({'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout,p.stderr);assert p.returncode==0
m=json.loads((E/'asan-build.json').read_text());assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==h for p,h in m['dependency_objects'].items());(E/'asan-post.json').write_text(json.dumps({'dependency_objects_unchanged':True})+'\n')
PY
