#!/bin/sh
# Caller must hold the selected build/profile lock and source its pinned env.
set -eu
[ "$#" = 3 ] || { echo "usage: $0 SOURCE BUILD OUTPUT_DIRECTORY" >&2; exit 2; }
src=$(realpath "$1"); build=$(realpath "$2"); out=$(realpath "$3")
meson compile -C "$build" -j4 painter-mypaint-tool
GIMP_TEST_LAYER_MASK_VISUAL_DIR="$out" python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase layer-mask-visual '--test-args=-p /painter-tool/layer-mask/visual-capture' painter-mypaint-tool
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase layer-mask-ui-regression painter-mypaint-tool gimp-painter-surface
