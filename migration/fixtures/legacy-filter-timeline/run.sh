#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
export GIMP_TESTING_ABS_TOP_SRCDIR="$base/gimp" GIMP_TESTING_ABS_TOP_BUILDDIR="$base/gimp"
root="$base/${1:-captures}"
mkdir -p "$root"
sha256sum "$base/../legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8" > "$root/original-before.sha256"
for scene in async stack cancel latecancel detach builtin; do
 out="$root/$scene"; mkdir -p "$out/profile"
 set +e
 HOME="$out/profile" GIMP2_DIRECTORY="$out/profile" TIMELINE_SCENE="$scene" python3 "$base/run_authenticated_display.py" --out "$out/display" --timeout 35 -- "$base/gimp/app/tests/test-xcf" > "$out/runner.log" 2>&1
 status=$?
 set -e
 echo "$status" > "$out/exit-code"
 test ! -f "$out/display/command.log" || cp "$out/display/command.log" "$out/capture.log"
done
sha256sum "$base/../legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8" > "$root/original-after.sha256"
