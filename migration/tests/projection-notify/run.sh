#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
repo=<checkout>
src=<native>/source
build=<native>/build-port
out=<evidence>
cp "$repo/app/tests/test-painter-mypaint-tool.cpp" "$repo/app/tests/test-painter-projection-notify.inc" "$src/app/tests/"
cp "$repo/app/tools/gimppaintermybrushtool.cpp" "$src/app/tools/"
meson compile -C "$build" -j4 painter-mypaint-tool >"$out/build.log" 2>&1
GIMP_TEST_PROJECTION_VISUAL_DIR="$out" python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase projection-visual '--test-args=-p /painter-tool/projection/visible-update' painter-mypaint-tool >"$out/visual.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase projection-notify-regression painter-mypaint-tool gimp-painter-surface >"$out/regression.log" 2>&1
