#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK";flock 9
repo=<checkout>
src=<native>/source
build=<native>/build-port
out=<evidence>
sha256sum "$build/app/tests/gimp-painter-surface" "$build/app/tests/gimp-painter-surface.p/test-gimp-painter-surface.cpp.o" >"$out/freshness-before.txt"
cp "$repo/app/tests/test-gimp-painter-surface.cpp" "$repo/app/tests/test-painter-surface-dirty-handoff.inc" "$src/app/tests/"
meson compile -C "$build" -j4 gimp-painter-surface painter-mypaint-coordinate-boundaries painter-mypaint-snapshot-isolation painter-mypaint-work-buffer >"$out/build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase dirty-native gimp-painter-surface painter-mypaint-coordinate-boundaries painter-mypaint-snapshot-isolation painter-mypaint-work-buffer >"$out/run.log" 2>&1

sha256sum "$build/app/tests/gimp-painter-surface" "$build/app/tests/gimp-painter-surface.p/test-gimp-painter-surface.cpp.o" >"$out/freshness-after.txt"
