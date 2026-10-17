#!/bin/bash
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
cd /workspace/shared/gimp-painter-value-invert-owner
flock /workspace/shared/gimp-painter-build.lock python3 -B /workspace/shared/gimp-value-invert-owner-controls.py
