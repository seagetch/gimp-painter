#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-deps-debian13/env.sh
export TMPDIR=/workspace/shared/gimp-value-invert-owner-tmp
cd /workspace/shared/gimp-painter-value-invert-owner
flock /workspace/shared/gimp-painter-build.lock ninja -C /workspace/shared/gimp-value-invert-owner-build -j6 app/tests/gimp-filter-layer app/tests/painter-xcf-open app/tests/painter-xcf-roundtrip app/painter/painter-filter-scheduler app/painter/painter-filter-owner-gates app/painter/painter-filter-native-kernels app/painter/painter-filter-context-native app/painter/painter-filter-point-shadow app/painter/painter-filter-sharpen app/painter/painter-filter-sharpen-shadow app/painter/painter-filter-sobel app/painter/painter-filter-context app/painter/painter-filter-import-completion
python3 -B tools/run-painter-xvfb.py bash -c 'exec 9>/workspace/shared/gimp-painter-build.lock; flock -x 9; python3 -B /workspace/shared/gimp-value-invert-owner-regression.py'
