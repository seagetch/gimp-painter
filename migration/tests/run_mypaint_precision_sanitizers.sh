#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)";cd "$root"
exec 9>/workspace/shared/gimp-painter-build.lock;flock 9
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps;source tools/linux-debian13-env.sh
ninja -C build-debian13 -j3 app/tests/painter-mypaint-precision app/tests/painter-mypaint-tool app/paint/painter-mypaint-surface/painter-mypaint-native-precision > migration/tests/mypaint-precision-final-build.log 2>&1
python3 migration/tests/build_mypaint_precision_sanitizers.py build-debian13 --report migration/tests/mypaint-precision-sanitizers-build.json > migration/tests/mypaint-precision-sanitizers-build.log 2>&1
python3 migration/tests/build_mypaint_tool_sanitizers.py build-debian13 --report migration/tests/mypaint-precision-tool-sanitizers-build.json > migration/tests/mypaint-precision-tool-sanitizers-build.log 2>&1
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$root/build-debian13" GIMP_TESTING_PLUGINDIRS="$root/build-debian13/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
build-debian13/app/tests/painter-mypaint-precision-asan > migration/tests/mypaint-precision-sanitizers.log 2>&1
c++ -std=c++14 -g -O1 -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer -Iapp app/paint/painter-mypaint-surface/tests/native-precision.cpp app/paint/painter-mypaint-surface/gegl-surface.cpp -o build-debian13/app/paint/painter-mypaint-surface/painter-mypaint-native-precision-asan $(pkg-config --cflags --libs gegl-0.4 lcms2)
build-debian13/app/paint/painter-mypaint-surface/painter-mypaint-native-precision-asan > migration/tests/mypaint-precision-backend-sanitizers.log 2>&1
printf 'PRECISION_FOCUSED_SANITIZERS_EXIT=0\n'
