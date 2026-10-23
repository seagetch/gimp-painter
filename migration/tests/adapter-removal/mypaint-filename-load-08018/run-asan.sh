#!/bin/bash
set -eu
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
cd /workspace/shared/gimp-native-baseline
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
mkdir -p mybrush-filename-evidence/asan
cp "$repo/migration/tests/build_mybrush_filename_sanitizers.py" source/migration/tests/
python3 source/migration/tests/build_mybrush_filename_sanitizers.py build-port --report mybrush-filename-evidence/asan/build.json >mybrush-filename-evidence/asan/build.log 2>&1
python3 source/tools/run-painter-xvfb.py python3 mybrush-filename-evidence/run-asan.py >mybrush-filename-evidence/asan/run.log 2>&1
