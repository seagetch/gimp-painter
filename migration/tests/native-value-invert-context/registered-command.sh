#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-deps-debian13/env.sh
export TMPDIR=/workspace/shared/gimp-value-invert-owner-tmp
export PYTHONDONTWRITEBYTECODE=1
cd /workspace/shared/gimp-painter-value-invert-owner
python3 -B tools/run-painter-xvfb.py bash -c 'exec 9>/workspace/shared/gimp-painter-build.lock; flock -x 9; python3 -B /workspace/shared/gimp-value-invert-owner-regression-full-build.py'
