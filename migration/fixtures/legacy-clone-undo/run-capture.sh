#!/usr/bin/env bash
set -euo pipefail
source /workspace/shared/gimp-legacy-build/env.sh
export GIMP2_DIRECTORY=/workspace/shared/gimp-legacy-build/runtime-home/profile
export XDG_CACHE_HOME=/workspace/shared/gimp-legacy-build/runtime-cache
export GEGL_PATH=/workspace/shared/gimp-legacy-build/prefix/lib/gegl-0.3
export GIMP_TESTING_ABS_TOP_SRCDIR=/workspace/scratch/5b5281e79681/gimp-painter-legacy
export GIMP_TESTING_ABS_TOP_BUILDDIR=/workspace/scratch/5b5281e79681/gimp-painter-legacy
export GIMP_PAINTER_CAPTURE_CLONE_UNDO=1
export LC_ALL=C
unset GIMP_PAINTER_FIXTURE_DIR GIMP_PAINTER_CAPTURE_FILTER
cd /workspace/scratch/5b5281e79681/gimp-painter-legacy
timeout 60s app/tests/test-xcf
