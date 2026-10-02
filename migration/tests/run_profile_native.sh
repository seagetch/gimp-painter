#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="${1:-$root/build-debian13}"
mode="${2:-normal}"
case "$mode" in
  normal) binary="$build/app/tests/painter-profile-native"; log="$build/painter-profile-native.log" ;;
  asan) binary="$build/profile-sanitizers/painter-profile-native"; log="$build/painter-profile-native-asan.log" ;;
  *) echo "Expected normal or asan" >&2; exit 2 ;;
esac
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
export GIMP_TESTING_ABS_TOP_SRCDIR="$root"
export GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common"
export UI_TEST=yes
cd "$root"
exec 9>"${GIMP_PAINTER_BUILD_LOCK:-/workspace/shared/gimp-painter-build.lock}"
flock 9
{
  sha256sum app/core/gimppainterprofile.cpp app/core/gimppainterprofile.h app/core/gimp-user-install.c app/tools/gimp-tools.c app/tests/test-painter-profile-native.cpp "$binary"
  "$binary"
  printf 'PAINTER_PROFILE_EXIT=0\n'
} 2>&1 | tee "$log"
