#!/usr/bin/env bash
# Use the pinned Debian13 GEGL/Babl environment; do not overwrite accepted results.
set -euo pipefail
if [ "$#" != 2 ]; then
  echo "usage: $0 EMPTY_OUTPUT_DIRECTORY VERIFIED_FIXTURES_JSON" >&2
  exit 2
fi
probe_source=$(cd "$(dirname "$0")" && pwd)
probe_output=$(realpath -m "$1")
mkdir "$probe_output"
cp "$probe_source/gegl-probe.c" "$probe_output/gegl-probe.c"
cp "$probe_source/run-gegl-probe.py" "$probe_output/run-gegl-probe.py"
cc -std=c11 -O2 -fPIC -shared "$probe_output/gegl-probe.c" -o "$probe_output/gegl-probe.so" $(pkg-config --cflags --libs gegl-0.4)
GEGL_SWAP=RAM python3 "$probe_output/run-gegl-probe.py" "$2"
