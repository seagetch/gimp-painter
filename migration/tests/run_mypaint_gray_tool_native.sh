#!/usr/bin/env bash
if [[ ${1:-} != locked ]]; then
  exec flock --close /workspace/shared/gimp-painter-build.lock bash "$0" locked
fi
cd /workspace/scratch/5b5281e79681/gimp-painter || exit 2
export GIMP_PAINTER_BUILD_LOCK=/workspace/shared/gimp-painter-build.lock
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
export GIMP_TESTING_ABS_TOP_SRCDIR="$PWD"
export GIMP_TESTING_ABS_TOP_BUILDDIR="$PWD/build-debian13"
export GIMP_TESTING_PLUGINDIRS="$PWD/build-debian13/plug-ins/common"
export UI_TEST=yes
sha256sum app/tools/gimppaintermybrushtool.cpp app/tests/test-painter-mypaint-tool.cpp app/paint/painter-mypaint-surface/gegl-surface.cpp build-debian13/app/tests/painter-mypaint-tool > /workspace/shared/painter-mypaint-gray-tool-source.txt
set -o pipefail
build-debian13/app/tests/painter-mypaint-tool 2>&1 | tee /workspace/shared/painter-mypaint-gray-tool-native.log
printf 'PAINTER_GRAY_TOOL_EXIT=%s\n' "${PIPESTATUS[0]}" | tee -a /workspace/shared/painter-mypaint-gray-tool-native.log
