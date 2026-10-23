#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
src=/workspace/shared/gimp-native-baseline/source
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/gimp-http-app-exit
ninja -C "$build" app/gimp-3.0 > "$out/build.log" 2>&1
meson install -C "$build" --no-rebuild > "$out/install.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" python3 "$out/capture.py" > "$out/run.log" 2>&1
