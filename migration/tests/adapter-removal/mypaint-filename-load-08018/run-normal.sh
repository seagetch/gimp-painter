#!/bin/bash
set -eu
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
cd /workspace/shared/gimp-native-baseline
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
for file in app/core/{gimppaintermybrush.cpp,gimppaintermybrush-handle.hpp,gimpmypaintbrush-load.h} app/tests/{test-gimp-painter-mybrush.c,test-painter-mybrush-duplicate-handle.cpp,test-painter-mybrush-filename.inc} migration/inventory/additional-c-header-routes.json; do cp "$repo/$file" "source/$file"; done
meson compile -C build-port gimp-painter-mybrush painter-public-headers-c painter-public-headers-cpp >mybrush-filename-evidence/build.log 2>&1
python3 source/tools/run-painter-xvfb.py meson test -C build-port gimp-painter-mybrush painter-public-header-compilation painter-additional-header-routes --no-rebuild --print-errorlogs --logbase mybrush-filename-normal >mybrush-filename-evidence/normal.log 2>&1
