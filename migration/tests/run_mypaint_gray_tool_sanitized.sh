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
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
sha256sum app/tools/gimppaintermybrushtool.cpp app/tests/test-painter-mypaint-tool.cpp app/paint/painter-mypaint-surface/gegl-surface.cpp build-debian13/app/tests/painter-mypaint-tool-asan > /workspace/shared/painter-mypaint-gray-tool-sanitized-source.txt
set -o pipefail
build-debian13/app/tests/painter-mypaint-tool-asan 2>&1 | tee /workspace/shared/painter-mypaint-gray-tool-sanitized.log
printf 'PAINTER_GRAY_TOOL_ASAN_EXIT=%s\n' "${PIPESTATUS[0]}" | tee -a /workspace/shared/painter-mypaint-gray-tool-sanitized.log
