#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"
flock 9
out=/workspace/shared/gimp-editor-page-asan
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
python3 "$src/migration/tests/build_mypaint_editor_sanitizers.py" "$build" --report "$out/build.json" > "$out/build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" python3 "$out/run-tests.py" > "$out/run.log" 2>&1
