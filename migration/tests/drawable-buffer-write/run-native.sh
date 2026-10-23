#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"
flock 9
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/buffer-write-evidence
for f in app/paint/painter-mypaint-surface/tests/coordinate-boundaries.cpp app/paint/painter-mypaint-surface/tests/write-domains.inc; do cp "$repo/$f" "$src/$f"; done
meson compile -C "$build" -j4 app/paint/painter-mypaint-surface/painter-mypaint-coordinate-boundaries app/paint/painter-mypaint-surface/painter-mypaint-native-precision app/tests/gimp-painter-surface app/tests/painter-roi-transfers 2>&1 | tee "$out/build.log"
set +e
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs painter-mypaint-coordinate-boundaries painter-mypaint-native-precision gimp-painter-surface painter-roi-transfers 2>&1 | tee "$out/run.log"
status=${PIPESTATUS[0]}
set -e
python3 - "$build/meson-logs/testlog.json" "$out/registered-native.json" <<'PY'
import json,sys
rows=[json.loads(x) for x in open(sys.argv[1]) if x.strip()]
for row in rows:row.pop('env',None)
json.dump(rows,open(sys.argv[2],'w'),indent=2)
PY
printf '%s\n' "$status" > "$out/exit-status"
exit "$status"
