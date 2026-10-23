#!/bin/sh
# Caller holds the selected build/profile lock and sources the pinned env.
set -eu
[ "$#" = 2 ] || { echo "usage: $0 SOURCE BUILD" >&2; exit 2; }
src=$(realpath "$1"); build=$(realpath "$2")
meson compile -C "$build" -j4 gimp-painter-surface painter-tempbuf painter-mypaint-coordinate-boundaries projection-update-bounds painter-mypaint-tool painter-mypaint-snapshot-isolation
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase snapshot-native gimp-painter-surface painter-tempbuf painter-mypaint-coordinate-boundaries projection-update-bounds painter-mypaint-tool painter-mypaint-snapshot-isolation
