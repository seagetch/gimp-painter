#!/bin/bash
set -eu
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
out="$base/${1:-gui-captures}";mkdir -p "$out/profile";out=$(cd "$out" && pwd)
export GIMP2_DIRECTORY="$out/profile" CAPTURE_DIR="$out"
export GIMP_TESTING_ABS_TOP_SRCDIR="$base/gimp" GIMP_TESTING_ABS_TOP_BUILDDIR="$base/gimp"
python3 "$base/run_authenticated_display.py" --out "$out/display" --timeout 90 -- "$base/gimp/app/tests/test-ui"
