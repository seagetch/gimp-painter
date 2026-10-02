#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
case "$mode" in normal) suffix=;; asan) suffix=-asan;; demo) suffix=; export PAINTER_CANVAS_DEMO=1;; *) exit 2;; esac
cd "$root" || exit 2
exec 9>"${GIMP_PAINTER_BUILD_LOCK:-/workspace/shared/gimp-painter-build.lock}"; flock 9
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
binary="$build/app/tests/painter-canvas-ui$suffix"
if [[ "$mode" == asan ]]; then
  python3 - "$build/canvas-ui-sanitizer-build.json" "$binary" <<'VERIFY'
import hashlib, json, pathlib, sys
report=json.loads(pathlib.Path(sys.argv[1]).read_text())
assert not report["changed_during_build"], "Source changed during sanitizer build"
for name,digest in report["sources_sha256"].items():
    assert hashlib.sha256(pathlib.Path(name).read_bytes()).hexdigest()==digest, "Sanitizer source drift: "+name
assert hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest()==report["executable_sha256"], "Sanitizer executable drift"
VERIFY
  [[ $? == 0 ]] || exit 2
fi
{
  printf 'Cloud/native GTK display: %s; mode: %s\n' "${DISPLAY:-unset}" "$mode"
  printf 'Requested GTK scale: %s\n' "${GDK_SCALE:-1}"
  sha256sum app/widgets/gimppainterlayertiles.cpp app/display/gimppaintercanvasui.cpp app/tests/test-painter-canvas-ui.c "$binary"
  "$binary"; status=$?
  printf 'PAINTER_CANVAS_EXIT=%s\n' "$status"
  exit "$status"
} 2>&1 | tee "$build/painter-canvas-ui-$mode.log"
exit "${PIPESTATUS[0]}"
