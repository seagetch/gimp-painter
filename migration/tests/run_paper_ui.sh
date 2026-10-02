#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source the configured environment first. Requires a real native GTK display.
set -eu
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
case "$mode" in
  normal) binary="$build/app/tests/painter-paper-ui" ;;
  asan) binary="$build/paper-sanitizers/painter-paper-ui" ;;
  *) echo 'Expected normal or asan' >&2; exit 2 ;;
esac
cd "$root"
exec 9>"/workspace/shared/gimp-painter-build.lock"
flock 9
export GIMP_TESTING_ABS_TOP_SRCDIR="$root"
export GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common"
export GIMP3_DIRECTORY="$build/paper-ui-profile"
export UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
mkdir -p "$GIMP3_DIRECTORY"
{
  printf 'Native GTK paper tests; DISPLAY=%s; mode=%s\n' "${DISPLAY:-unset}" "$mode"
  sha256sum app/paint/gimppainterpaper.cpp app/paint/gimppainterpaper-paste.cpp app/paint/gimpbrushcore.c app/paint/gimpbrushcore-loops.cc app/paint/gimppaintoptions.h app/tools/gimppaintoptions-gui.c app/tools/gimpfillbrushtool.cpp app/tools/gimppaintersmudgetool.cpp app/tests/test-painter-paper-ui.cpp "$binary"
  set +e
  "$binary"
  status=$?
  printf 'PAPER_UI_EXIT=%s\n' "$status"
  exit "$status"
} 2>&1 | tee "$build/paper-ui-$mode.log"
exit "${PIPESTATUS[0]}"
