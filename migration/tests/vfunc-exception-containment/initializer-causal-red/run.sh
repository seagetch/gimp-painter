#!/bin/bash
set -euo pipefail
source <native>/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
out=<native>/vfunc-initializer-red
export HOME="$out/home" XDG_CONFIG_HOME="$out/config" XDG_CACHE_HOME="$out/cache" XDG_DATA_HOME="$out/data" XDG_STATE_HOME="$out/state" XDG_RUNTIME_DIR="$out/runtime" TMPDIR="$out/tmp"
mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR" "$TMPDIR";chmod 700 "$XDG_RUNTIME_DIR" "$TMPDIR"
python3 <native>/source/tools/run-painter-xvfb.py python3 "$out/build-run.py"
