#!/bin/bash
set -euo pipefail
W=/workspace/shared/gimp-clean-aggregate;R=/workspace/scratch/0015fc5d1a03/gimp-painter;E=/workspace/shared/options-readiness/pre-adoption-red
exec 9>"$W/build.lock";flock 9;source "$W/env.sh";mkdir -p "$E"
cp "$R/app/tests/test-mypaint-options-readiness.cpp" "$W/source/app/tests/test-mypaint-options-readiness.cpp"
python3 - <<'PY'
from pathlib import Path
import json,hashlib,tarfile
W=Path('/workspace/shared/gimp-clean-aggregate/source');E=Path('/workspace/shared/options-readiness/pre-adoption-red');m=json.loads(Path('/workspace/shared/options-readiness/green/source-manifest.json').read_text());m['scope']='same reviewed precursor, supplemental before-parent-adoption subclass case';sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();m['owned_sha256']={p:sha(W/p) for p in m['owned_sha256']};m['input_sha256']={p:sha(W/p) for p in m['input_sha256']};m['complete_source_sha256']={str(p.relative_to(W)):sha(p) for p in W.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts};(E/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(E/'tested-inputs.tar.gz','w:gz') as t:
 for p in m['input_sha256']:t.add(W/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/mypaint-options-readiness >"$E/build.log" 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$W/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$W/build" GIMP_TESTING_PLUGINDIRS="$W/build/plug-ins/common" UI_TEST=yes
python3 "$W/source/tools/run-painter-xvfb.py" python3 - "$W/build/app/tests/mypaint-options-readiness" "$E" <<'PY' >"$E/portrait.log" 2>&1
from pathlib import Path
import subprocess,sys,json,hashlib
p=subprocess.run([sys.argv[1],'-p','/original-options-readiness/constructor-before-adoption-same'],capture_output=True,text=True,timeout=60);E=Path(sys.argv[2]);(E/'result.json').write_text(json.dumps({'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr},indent=2)+'\n');print(p.stdout,p.stderr);assert p.returncode!=0 and 'options_readiness_c_consumer(o)' in p.stdout+p.stderr
m=json.loads((E/'source-manifest.json').read_text());S=Path('/workspace/shared/gimp-clean-aggregate/source');assert all(hashlib.sha256((S/p).read_bytes()).hexdigest()==h for p,h in m['input_sha256'].items());(E/'source-after.json').write_text(json.dumps({'recorded_inputs_unchanged':True})+'\n')
PY
