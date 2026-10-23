#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
for brush in classic/pen classic/textured_ink deevad/watercolor_expressive; do
 name=${brush##*/};mkdir -p "$base/captures/$name" "$base/profile-$name"
 export GIMP2_DIRECTORY="$base/profile-$name" STROKE_CAPTURE_DIR="$base/captures/$name" STROKE_BRUSH="$base/gimp/data/mypaint-brushes/$brush.myb"
 set +e
 timeout 60 "$base/gimp/app/tests/test-xcf" > "$STROKE_CAPTURE_DIR/capture.log" 2>&1
 status=$?
 set -e
 echo $status > "$STROKE_CAPTURE_DIR/exit-code"
done
