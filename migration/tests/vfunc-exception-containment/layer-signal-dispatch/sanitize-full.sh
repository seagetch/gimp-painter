#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK";flock 9
src=<native>/source
out=<native>/layer-dispatch-green
cp <checkout>/migration/tests/build_layer_dispatch_sanitizers.py "$src/migration/tests/"
python3 "$src/migration/tests/build_layer_dispatch_sanitizers.py" <native>/build-port --report "$out/sanitizer-full-build.json" > "$out/sanitizer-full-build.log" 2>&1
export HOME="$out/home" XDG_CONFIG_HOME="$out/config" XDG_CACHE_HOME="$out/cache" XDG_DATA_HOME="$out/data" XDG_STATE_HOME="$out/state" XDG_RUNTIME_DIR="$out/runtime" TMPDIR="$out/tmp"
python3 "$src/tools/run-painter-xvfb.py" python3 "$out/sanitize-run-full.py" > "$out/sanitizer-full-portrait.log" 2>&1
