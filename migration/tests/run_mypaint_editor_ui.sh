#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source the configured GIMP build environment first; requires a real GTK display.
set -u
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
log="${3:-$build/painter-mypaint-editor-$mode.log}"
case "$mode" in
  normal) suffix= ;;
  asan) suffix=-asan ;;
  demo) suffix=; export PAINTER_EDITOR_DEMO=1 ;;
  *) echo "Expected normal, asan, or demo" >&2; exit 2 ;;
esac
cd "$root" || exit 2
exec 9>"${GIMP_PAINTER_BUILD_LOCK:-/tmp/gimp-painter-build.lock}"
flock 9
export GIMP_TESTING_ABS_TOP_SRCDIR="$root"
export GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common"
export UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
binary="$build/app/tests/painter-mypaint-editor$suffix"
{
  printf 'Cloud/native GTK display: %s; mode: %s\n' "${DISPLAY:-unset}" "$mode"
  sha256sum app/widgets/gimppaintermybrusheditor.cpp app/tests/test-painter-mypaint-editor.cpp "$binary"
  "$binary"
  status=$?
  printf 'PAINTER_EDITOR_EXIT=%s\n' "$status"
  exit "$status"
} 2>&1 | tee "$log"
exit "${PIPESTATUS[0]}"
