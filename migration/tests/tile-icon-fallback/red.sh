#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=/workspace/scratch/0015fc5d1a03/gimp-painter
S=/workspace/shared/gimp-native-baseline/source
B=/workspace/shared/gimp-native-baseline/build-port
E=/workspace/shared/gimp-icon-fallback
cp "$R/app/tests/test-painter-icon-fallback.c" "$S/app/tests/"
python3 - <<'PY'
from pathlib import Path
r=Path('/workspace/scratch/0015fc5d1a03/gimp-painter/app/tests/meson.build');s=Path('/workspace/shared/gimp-native-baseline/source/app/tests/meson.build');marker='# Native missing-icon paths; wrappers are confined to this test executable.'
text=s.read_text(); block=r.read_text().split(marker,1)[1].split('\nendif',1)[0]+'\nendif\n'
if marker not in text:s.write_text(text+'\n'+marker+block)
else:assert text.split(marker,1)[1].split('\nendif',1)[0]+'\nendif\n'==block
PY
cp "$S/app/widgets/gimppainterlayertiles.cpp" "$E/red-gimppainterlayertiles.cpp"
ninja -C "$B" app/tests/painter-icon-fallback > "$E/red-build.log" 2>&1
set +e
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs --logbase icon-red painter-icon-fallback --test-args='-p /painter-icons/layer-missing-name' > "$E/red-run.log" 2>&1
code=$?
set -e
printf '%s\n' "$code" > "$E/red-exit.txt"
cp "$B/meson-logs/icon-red.json" "$E/red-private.json"
test "$code" -ne 0
grep -q "'dark' should be TRUE" "$E/red-run.log"
