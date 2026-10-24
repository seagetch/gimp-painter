#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-clean-aggregate/env.sh
B=/workspace/shared/gimp-clean-aggregate/build
export LD_LIBRARY_PATH="$B/libgimpmath:$B/libgimpbase:$B/libgimpcolor:$B/libgimpconfig:$B/libgimpmodule:$B/libgimpthumb:$B/libgimpwidgets:${LD_LIBRARY_PATH:-}"
exec 9>/workspace/shared/gimp-clean-aggregate/build.lock
flock -s 9
python3 - <<'PY'
from pathlib import Path
import hashlib,json
s=Path('/workspace/shared/gimp-clean-aggregate/source');m=s.parent/'exact-0a775df4135052a2c851c9a5dfa37cea25887268/prebuild-tracked-source-sha256.json';j=json.loads(m.read_text())
assert all(hashlib.sha256((s/p).read_bytes()).hexdigest()==h for p,h in j.items())
PY
python3 /workspace/shared/build-video-editor-private.py sanitizer
python3 /workspace/scratch/0015fc5d1a03/gimp-painter/tools/run-painter-xvfb.py python3 /workspace/shared/run-video-editor-private.py sanitizer
