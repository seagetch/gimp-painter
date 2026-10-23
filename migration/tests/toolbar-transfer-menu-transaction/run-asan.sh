#!/bin/bash
set -eu
source /workspace/shared/gimp-native-baseline/env.sh
exec 9>"$GIMP_PAINTER_BUILD_LOCK"; flock 9
cd /workspace/shared/gimp-native-baseline
repo=/workspace/scratch/0015fc5d1a03/gimp-painter
mkdir -p toolbar-transfer-evidence/asan
meson test -C build-port painter-public-header-compilation painter-additional-header-routes --no-rebuild --print-errorlogs --logbase toolbar-header-closure >toolbar-transfer-evidence/header-closure.log 2>&1
cp "$repo/migration/tests/build_toolbar_transfer_sanitizers.py" source/migration/tests/
python3 source/migration/tests/build_toolbar_transfer_sanitizers.py build-port --report toolbar-transfer-evidence/asan/build.json >toolbar-transfer-evidence/asan/build.log 2>&1
python3 source/tools/run-painter-xvfb.py python3 toolbar-transfer-evidence/run-asan.py >toolbar-transfer-evidence/asan/run.log 2>&1
