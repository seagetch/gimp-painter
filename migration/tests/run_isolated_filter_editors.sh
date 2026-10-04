#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run in a fresh cloud-desktop terminal with a real GTK display, after building.
set -eo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="${GIMP_PAINTER_EDITOR_BUILD:-$root/build-installed-filter}"
mode="${1:-normal}"
if (( $# )); then shift; fi
output="${GIMP_PAINTER_EDITOR_OUTPUT:-$root/migration/tests/filter-isolated-editors}"
case "$mode" in
  normal) binary="$build/app/tests/painter-layer-ui";;
  asan) binary="$build/app/tests/painter-layer-ui-asan";;
  *) exit 2;;
esac
mkdir -p "$output"
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
cd "$root"
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes GSETTINGS_BACKEND=memory
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
{
  date -u +%FT%TZ
  printf 'Native GTK display=%s; mode=%s\n' "$DISPLAY" "$mode"
  sha256sum app/dialogs/painter-layer-dialog.cpp app/tests/test-painter-layer-ui.c \
    app/tests/test-isolated-filter-editors.inc "$binary"
} > "$output/$mode-identity.txt"
set +e
timeout 300 dbus-run-session -- "$binary" --verbose "$@" > "$output/$mode.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$output/$mode-exit-code.txt"
tail -45 "$output/$mode.log"
printf 'ISOLATED_EDITORS_%s_EXIT=%s\n' "$mode" "$status"
exit "$status"
