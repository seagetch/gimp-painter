#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK";flock 9
repo=<checkout>
src=<native>/source
build=<native>/build-port
out=<evidence>
for f in gegl-surface.cpp work-budget.hpp tests/work-buffer.cpp; do cp "$repo/app/paint/painter-mypaint-surface/$f" "$src/app/paint/painter-mypaint-surface/$f"; done
python3 - <<'PY'
from pathlib import Path
main=Path('<checkout>/app/paint/painter-mypaint-surface/meson.build')
p=Path('<native>/source/app/paint/painter-mypaint-surface/meson.build');s=p.read_text();marker='# Bounded per-operation scratch, sparse floating storage, rejection and retry.'
if marker not in s:p.write_text(s+'\n'+marker+main.read_text().split(marker)[1])
PY
meson setup --reconfigure "$build" "$src" >"$out/setup.log" 2>&1
meson compile -C "$build" -j4 painter-mypaint-work-buffer painter-mypaint-coordinate-boundaries painter-mypaint-snapshot-isolation gimp-painter-surface >"$out/build.log" 2>&1
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase work-buffer-native painter-mypaint-work-buffer painter-mypaint-coordinate-boundaries painter-mypaint-snapshot-isolation gimp-painter-surface >"$out/run.log" 2>&1
