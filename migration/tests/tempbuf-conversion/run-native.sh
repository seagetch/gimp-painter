#!/bin/sh
# Source the pinned native build environment before invoking this script.
# Call under the shared native-build lock. No source-tree synchronization is done.
set -eu
if [ "$#" -ne 2 ]; then echo "usage: $0 SOURCE BUILD" >&2; exit 2; fi
src=$(realpath "$1")
build=$(realpath "$2")
meson setup --reconfigure "$build" "$src"
meson compile -C "$build" -j4 painter-tempbuf gimp-painter-surface painter-mypaint-coordinate-boundaries
python3 "$src/tools/run-painter-xvfb.py" meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs --logbase tempbuf-native painter-tempbuf gimp-painter-surface painter-mypaint-coordinate-boundaries
# Copy only selected Meson result fields, never its inherited environment.
python3 - "$build/meson-logs/tempbuf-native.json" <<'PY'
import json,sys
fields=('name','result','returncode','duration','stdout','stderr','command')
rows=[json.loads(line) for line in open(sys.argv[1]) if line.strip()]
print(json.dumps([{key:row[key] for key in fields if key in row} for row in rows],indent=2))
PY
