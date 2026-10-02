#!/usr/bin/env bash
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root" || exit 2
exec 9>/workspace/shared/gimp-painter-build.lock;flock 9
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$root/build-debian13" GIMP_TESTING_PLUGINDIRS="$root/build-debian13/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mode="${1:-normal}";scope="${2:-precision}"
args=(-p /painter-tool/precision-native-save-roundtrip);suffix=""
if [[ "$scope" == all ]];then args=();suffix="-all";elif [[ "$scope" != precision ]];then echo "Expected precision or all scope" >&2;exit 2;fi
binary="$root/build-debian13/app/tests/painter-mypaint-tool";[[ "$mode" == asan ]] && binary+=-asan
{ sha256sum "$binary"; "$binary" "${args[@]}"; code=$?;printf 'PRECISION_GTK_%s_EXIT=%s\n' "$mode" "$code";exit "$code"; } 2>&1 | tee "migration/tests/mypaint-precision-tool-$mode$suffix.log"
exit "${PIPESTATUS[0]}"
