#!/bin/sh
# Invoke under the shared build lock after sourcing the pinned build environment.
set -eu
if [ "$#" -ne 2 ]; then echo "usage: $0 SOURCE BUILD" >&2; exit 2; fi
src=$(realpath "$1");build=$(realpath "$2")
meson compile -C "$build" -j4 gimp-painter-surface painter-tempbuf painter-mypaint-coordinate-boundaries
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase selection-native gimp-painter-surface painter-tempbuf painter-mypaint-coordinate-boundaries
