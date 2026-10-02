#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run from a fresh cloud desktop terminal. Never reuse the user's open GIMP.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
mode="${1:-normal}"
build="$root/build-debian13"
case "$mode" in
  normal) binary="$build/app/tests/painter-layer-ui";;
  asan) binary="$build/app/tests/painter-layer-ui-asan";;
  *) exit 2;;
esac
cd "$root"
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
if [[ "$mode" == normal ]]; then
  ninja -C "$build" app/tests/painter-layer-ui > migration/tests/filter-native-ui-build.log 2>&1
fi
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes GSETTINGS_BACKEND=memory
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
{
  printf 'Filter aliases/point mappings native GTK; mode=%s\n' "$mode"
  sha256sum app/dialogs/painter-layer-dialog.cpp app/tests/test-painter-layer-ui.c "$binary"
  set +e
  "$binary"
  status=$?
  printf 'FILTER_NATIVE_UI_EXIT=%s\n' "$status"
  exit "$status"
} 2>&1 | tee "$root/migration/tests/filter-native-ui-$mode.log"
exit "${PIPESTATUS[0]}"
