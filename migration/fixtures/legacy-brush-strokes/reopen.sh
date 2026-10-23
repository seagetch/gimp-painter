#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
for name in pen textured_ink watercolor_expressive; do
 export GIMP2_DIRECTORY="$base/profile-$name" STROKE_CAPTURE_DIR="$base/captures/$name" STROKE_REOPEN="$base/captures/$name/final.xcf"
 timeout 60 "$base/gimp/app/tests/test-xcf" > "$STROKE_CAPTURE_DIR/reopen.log" 2>&1
 cmp "$STROKE_CAPTURE_DIR/final.rgba" "$STROKE_CAPTURE_DIR/reopened.rgba"
 echo "original reader RGBA exact match" > "$STROKE_CAPTURE_DIR/reopen-result.txt"
done
