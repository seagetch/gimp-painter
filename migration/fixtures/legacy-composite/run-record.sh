#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
set +u
source "$base/../legacy-filter-positive-20261015/build/env.sh"
set -u
for scene in anime watercolor; do
 mkdir -p "$base/$scene-profile" "$base/evidence-nonuniform/$scene"
 export GIMP2_DIRECTORY="$base/$scene-profile" GIMP_PAINTER_FIXTURE_DIR="$base/evidence-nonuniform/$scene"
 export GIMP_PAINTER_CAPTURE_FILTER=1
 unset COMPOSITE_WATERCOLOR
 export COMPOSITE_PRESET="$base/gimp/data/layer-presets/new-anime-layer.json"
 if [ "$scene" = watercolor ]; then export COMPOSITE_WATERCOLOR=1; export COMPOSITE_PRESET="$base/gimp/data/layer-presets/watercolor.json"; fi
 set +e
 timeout 60 "$base/gimp/app/tests/test-xcf" > "$base/evidence-nonuniform/$scene/capture.log" 2>&1
 status=$?
 set -e
 echo "$status" > "$base/evidence-nonuniform/$scene/exit-code"
done
