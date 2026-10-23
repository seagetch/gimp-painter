#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=/workspace/scratch/0015fc5d1a03/gimp-painter
S=/workspace/shared/gimp-native-baseline/source
B=/workspace/shared/gimp-native-baseline/build-port
E=/workspace/shared/gimp-icon-fallback
test "$(cat "$E/red-exit.txt")" != 0
grep -q "'dark' should be TRUE" "$E/red-run.log"
cp "$R/app/widgets/gimppainterlayertiles.cpp" "$S/app/widgets/"
cp "$R/app/tests/test-painter-icon-fallback.c" "$S/app/tests/"
ninja -C "$B" app/tests/painter-icon-fallback app/tests/painter-canvas-ui app/tests/painter-layer-ui > "$E/green-build.log" 2>&1
export GIMP_TEST_ICON_SCREENSHOT="$E/portrait-oncanvas-missing-icon.png"
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs --logbase icon-green painter-icon-fallback > "$E/green-run.log" 2>&1
cp "$B/meson-logs/icon-green.json" "$E/green-private.json"
unset GIMP_TEST_ICON_SCREENSHOT
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs --logbase icon-aggregate painter-canvas-ui painter-layer-ui > "$E/aggregate-run.log" 2>&1
cp "$B/meson-logs/icon-aggregate.json" "$E/aggregate-private.json"
python3 "$E/build-asan.py" "$B" --report "$E/asan-build.json" > "$E/asan-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" python3 "$E/run-asan.py" > "$E/asan-run.log" 2>&1
