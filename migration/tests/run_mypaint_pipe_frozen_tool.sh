#!/usr/bin/env bash
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"
exec 9>/workspace/shared/gimp-painter-build.lock; flock 9
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$root/build-debian13" GIMP_TESTING_PLUGINDIRS="$root/build-debian13/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
for mode in normal asan; do
 binary="$root/build-debian13/mypaint-pipe-frozen/painter-mypaint-tool"; [[ "$mode" == asan ]] && binary+=-asan
 { sha256sum "$binary"; "$binary"; printf 'FROZEN_PIPE_TOOL_%s_EXIT=%s\n' "$mode" "$?"; } 2>&1 | tee "migration/tests/mypaint-pipe-tool-frozen-$mode.log"
done
