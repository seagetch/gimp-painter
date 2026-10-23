#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=/workspace/scratch/0015fc5d1a03/gimp-painter
S=/workspace/shared/gimp-native-baseline/source
B=/workspace/shared/gimp-native-baseline/build-port
E=/workspace/shared/gimp-tool-group-retirement
# Only promote after the actual unmodified production path reproduced the failure.
test "$(cat "$E/red-exit.txt")" != 0
grep -q 'gimp_tool_group_child_add' "$E/red-run.log"
cp "$R/app/tools/gimp-tools.c" "$S/app/tools/"
cp "$R/app/tests/test-painter-perspective-ui.c" "$R/app/tests/test-painter-tool-group-retirement.inc" "$S/app/tests/"
ninja -C "$B" app/tests/painter-perspective-ui > "$E/green-build.log" 2>&1
export GIMP_TEST_PERSPECTIVE_SCREENSHOT="$E/portrait-oncanvas-after-reset.png"
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs --logbase tool-groups-green painter-perspective-ui > "$E/green-run.log" 2>&1
cp "$B/meson-logs/tool-groups-green.json" "$E/green-private.json"
unset GIMP_TEST_PERSPECTIVE_SCREENSHOT
python3 "$E/build-asan.py" "$B" --report "$E/asan-build.json" > "$E/asan-build.log" 2>&1
python3 "$S/tools/run-painter-xvfb.py" python3 "$E/run-asan.py" > "$E/asan-run.log" 2>&1
