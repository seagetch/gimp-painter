#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source the target dependency environment first; run on a real GTK display.
# Build binaries with build_perspective_sanitizers.py before this runner.
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="${1:-$root/build-debian13}"
output="${2:-$root/migration/tests}"
[ -n "${DISPLAY:-}" ] || { echo 'A GTK display is required'; exit 77; }
mkdir -p "$output"
export GIMP_TESTING_ABS_TOP_SRCDIR="$root"
export GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common"
export UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
for test in painter-perspective-ui painter-perspective-events painter-navigation-events; do
  for suffix in '' -asan; do
    log="$output/perspective-final-${test}${suffix}.txt"
    "$build/app/tests/${test}${suffix}" 2>&1 | tee "$log"
    status=${PIPESTATUS[0]}
    printf 'PERSPECTIVE_FINAL_EXIT=%s\n' "$status" | tee -a "$log"
    if [ "$status" -ne 0 ]; then exit "$status"; fi
  done
done
printf 'PERSPECTIVE_ALL_NATIVE_EXIT=0\n'
