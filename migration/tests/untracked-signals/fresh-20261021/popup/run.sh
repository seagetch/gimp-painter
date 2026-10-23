#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/popup-signal-asan
ninja -C "$build" app/tests/painter-popup-handles app/tests/editor-dropdown app/tests/mypaint-selector-contracts > "$out/normal-build.log" 2>&1
python3 "$out/build.py" "$build" --report "$out/build.json" > "$out/build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" python3 "$out/run-tests.py" > "$out/run.log" 2>&1
