#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK";flock 9
repo=<checkout>
src=<native>/source
build=<native>/build-port
out=<evidence>
cmp "$repo/app/paint/painter-mypaint-surface/paint-core.cpp" "$src/app/paint/painter-mypaint-surface/paint-core.cpp"
python3 "$out/run-mutations.py" --source "$src" --build "$build" --output "$out" >"$out/mutation-run.log" 2>&1
meson compile -C "$build" -j4 gimp-painter-surface painter-mypaint-coordinate-boundaries painter-mypaint-snapshot-isolation painter-mypaint-work-buffer >"$out/build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase dirty-native gimp-painter-surface painter-mypaint-coordinate-boundaries painter-mypaint-snapshot-isolation painter-mypaint-work-buffer >"$out/run.log" 2>&1
