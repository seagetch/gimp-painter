#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
for scene in clone filter; do
 out="$base/${1:-captures}/$scene"; mkdir -p "$out" "$out/profile"
 export HOME="$out/profile"
 export GIMP2_DIRECTORY="$out/profile" GIMP_PAINTER_FIXTURE_DIR="$out" GIMP_PAINTER_CAPTURE_FILTER=1
 unset OPERATIONS_FILTER
 if [ "$scene" = filter ]; then export OPERATIONS_FILTER=1; fi
 set +e
 timeout 120 "$base/gimp/app/tests/test-xcf" > "$out/capture.log" 2>&1
 status=$?
 set -e
 echo "$status" > "$out/exit-code"
done
