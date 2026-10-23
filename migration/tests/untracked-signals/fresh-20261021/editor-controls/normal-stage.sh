#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
src=/workspace/shared/gimp-native-baseline/source
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
build=/workspace/shared/gimp-native-baseline/build-port
out=/workspace/shared/gimp-editor-control-evidence
mkdir -p "$out"
cp "$repo/app/tests/test-painter-mypaint-editor.cpp" "$repo/app/tests/test-painter-editor-control-lifetimes.inc" "$src/app/tests/"
# The isolated registration already exists; never overwrite other workers' stanzas.
grep -q "test('painter-editor-control-lifetimes'" "$src/app/tests/meson.build"
ninja -C "$build" app/tests/painter-mypaint-editor > "$out/normal-build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase editor-controls painter-editor-control-lifetimes > "$out/normal-run.log" 2>&1
cp "$build/meson-logs/editor-controls.json" "$out/normal-testlog.json"
