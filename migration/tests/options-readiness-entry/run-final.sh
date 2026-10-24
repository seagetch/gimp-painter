#!/bin/bash
set -euo pipefail
R=/workspace/scratch/0015fc5d1a03/gimp-painter;W=/workspace/shared/gimp-clean-aggregate;E=/workspace/shared/options-readiness
exec 9>"$W/build.lock";flock 9
source "$W/env.sh"
python3 - "$R" "$W/source" "$E" <<'PY'
from pathlib import Path
import json,hashlib,sys,tarfile
r,s,e=map(Path,sys.argv[1:]);sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();assert not (e/'reviewed-red').exists();(e/'green').rename(e/'reviewed-red');(e/'green').mkdir();m=json.loads((e/'pre-adoption-red/source-manifest.json').read_text());assert all(sha(s/p)==h for p,h in m['input_sha256'].items())
for p in m['owned_sha256']:
 data=(r/p).read_bytes()
 if (s/p).read_bytes()!=data:(s/p).write_bytes(data)
m['scope']='explicit final readiness overlay on exact2ef source after source-bound same-request and constructor/blocked-handler reds';m['owned_sha256']={p:sha(s/p) for p in m['owned_sha256']};m['input_sha256']={p:sha(s/p) for p in m['input_sha256']};m['complete_source_sha256']={str(p.relative_to(s)):sha(p) for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts};(e/'green/source-manifest.json').write_text(json.dumps(m,indent=2)+'\n')
with tarfile.open(e/'green/tested-inputs.tar.gz','w:gz') as t:
 for p in m['input_sha256']:t.add(s/p,arcname=p)
PY
ninja -C "$W/build" -j2 app/tests/mypaint-options-readiness app/tests/mypaint-options-mode app/tests/gimp-painter-options app/tests/mypaint-enum-public libgimpbase/mypaint-enum-library-only >"$E/green/build.log" 2>&1
python3 "$W/source/tools/run-painter-xvfb.py" meson test -C "$W/build" mypaint-options-readiness mypaint-options-mode gimp-painter-options mypaint-enum-public mypaint-enum-library-only --no-rebuild --print-errorlogs --logbase options-readiness >"$E/green/normal-portrait.log" 2>&1
python3 - "$W/build/meson-logs/options-readiness.json" "$E/green/normal.json" <<'PY'
import json,sys
json.dump([{k:r.get(k) for k in ['name','result','returncode','stdout','stderr','duration','command']} for r in map(json.loads,open(sys.argv[1]))],open(sys.argv[2],'w'),indent=2)
PY
python3 "$W/source/tools/check_painter_headers.py" --build-dir "$W/build" --report "$E/green/headers.json" >"$E/green/headers.log" 2>&1
python3 - "$W/source" "$E/green" <<'PY'
from pathlib import Path
import json,hashlib,sys
s,e=map(Path,sys.argv[1:]);m=json.loads((e/'source-manifest.json').read_text());after={str(p.relative_to(s)):hashlib.sha256(p.read_bytes()).hexdigest() for p in s.rglob('*') if p.is_file() and '__pycache__' not in p.parts and '.git' not in p.parts};assert after==m['complete_source_sha256'];(e/'source-after.json').write_text(json.dumps({'all_source_unchanged':True})+'\n')
PY
