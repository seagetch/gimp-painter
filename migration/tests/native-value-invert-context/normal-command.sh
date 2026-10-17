#!/bin/bash
set -euo pipefail
source /workspace/shared/gimp-deps-debian13/env.sh
export TMPDIR=/workspace/shared/gimp-value-invert-owner-tmp
cd /workspace/shared/gimp-painter-value-invert-owner
python3 -B tools/run-painter-xvfb.py python3 -B migration/tests/run_filter_owner_context_fixture_test.py -- bash -c 'exec 9>/workspace/shared/gimp-painter-build.lock; flock -x 9; python3 -B migration/tests/run_value_invert_focused_normal.py /workspace/shared/gimp-value-invert-owner-build --lock-fd 9 --report /workspace/shared/gimp-value-invert-owner-normal-1.json'
