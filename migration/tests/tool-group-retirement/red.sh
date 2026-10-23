#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
R=/workspace/scratch/0015fc5d1a03/gimp-painter
S=/workspace/shared/gimp-native-baseline/source
B=/workspace/shared/gimp-native-baseline/build-port
E=/workspace/shared/gimp-tool-group-retirement
cp "$R/app/tests/test-painter-perspective-ui.c" "$R/app/tests/test-painter-tool-group-retirement.inc" "$S/app/tests/"
cp "$S/app/tools/gimp-tools.c" "$E/red-gimp-tools.c"
ninja -C "$B" app/tests/painter-perspective-ui > "$E/red-build.log" 2>&1
set +e
python3 "$S/tools/run-painter-xvfb.py" meson test -C "$B" --no-rebuild --print-errorlogs --logbase tool-groups-red painter-perspective-ui --test-args='-p /perspective-ui/retained-groups-deserialize' > "$E/red-run.log" 2>&1
code=$?
set -e
printf '%s\n' "$code" > "$E/red-exit.txt"
cp "$B/meson-logs/tool-groups-red.json" "$E/red-private.json"
test "$code" -ne 0
grep -q 'gimp_tool_group_child_add' "$E/red-run.log"
