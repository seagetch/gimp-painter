#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-deps-debian13/env.sh
export PYTHONDONTWRITEBYTECODE=1 TMPDIR=/workspace/shared/gimp-value-invert-owner-tmp
flock /workspace/shared/gimp-painter-build.lock ninja -C /workspace/shared/gimp-value-invert-owner-build -j6
