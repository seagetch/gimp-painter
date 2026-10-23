#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/gimp-editor-control-evidence
python3 "$out/build-asan.py" "$build" --report "$out/asan-build.json" > "$out/asan-build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" python3 "$out/run-asan.py" > "$out/asan-run.log" 2>&1
