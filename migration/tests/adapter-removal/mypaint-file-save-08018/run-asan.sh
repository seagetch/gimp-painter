#!/bin/bash
set -eu
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
cd /workspace/shared/gimp-native-baseline
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
mkdir -p mybrush-save-evidence/asan
cp "$repo/migration/tests/build_mybrush_save_sanitizers.py" source/migration/tests/
python3 source/migration/tests/build_mybrush_save_sanitizers.py build-port --report mybrush-save-evidence/asan/build.json >mybrush-save-evidence/asan/build.log 2>&1
python3 source/tools/run-painter-xvfb.py python3 mybrush-save-evidence/run-asan.py >mybrush-save-evidence/asan/run.log 2>&1
