#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
case "$mode" in normal) binary="$build/app/tests/painter-compact-options";; asan) binary="$build/compact-options-sanitizers/painter-compact-options";; *) exit 2;; esac
cd "$root" || exit 2
exec 9>"${GIMP_PAINTER_BUILD_LOCK:-/workspace/shared/gimp-painter-build.lock}"; flock 9
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
if [[ "$mode" == asan ]]; then
  python3 - "$build/compact-options-sanitizer-build.json" "$binary" <<'VERIFY'
import hashlib,json,pathlib,sys
r=json.loads(pathlib.Path(sys.argv[1]).read_text())
assert not r['changed_during_build']
for n,h in r['sources_sha256'].items():assert hashlib.sha256(pathlib.Path(n).read_bytes()).hexdigest()==h,n
assert hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest()==r['executable_sha256']
VERIFY
  [[ $? == 0 ]] || exit 2
fi
{
  printf 'Cloud/native GTK display: %s; mode: %s; scale: %s\n' "${DISPLAY:-unset}" "$mode" "${GDK_SCALE:-1}"
  sha256sum app/widgets/gimppaintercompactoptions.cpp app/display/gimppaintercanvasui.cpp app/tests/test-painter-compact-options.c "$binary"
  "$binary"; status=$?
  printf 'PAINTER_COMPACT_EXIT=%s\n' "$status"; exit "$status"
} 2>&1 | tee "$build/painter-compact-options-$mode.log"
exit "${PIPESTATUS[0]}"
