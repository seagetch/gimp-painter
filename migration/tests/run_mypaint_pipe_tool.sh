#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Execute on the cloud's real native GTK display. Build separately, under the
# same lock. Preserve the GUI log and source hashes for each actual run.
set -eu
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
case "$mode" in
  normal) binary="$build/app/tests/painter-mypaint-tool" ;;
  asan) binary="$build/app/tests/painter-mypaint-tool-asan" ;;
  *) echo 'Expected normal or asan' >&2; exit 2 ;;
esac
cd "$root"
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
export GIMP_TESTING_ABS_TOP_SRCDIR="$root"
export GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common"
export UI_TEST=yes
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
set -o pipefail
{
  printf 'MyPaint pipe/release native GTK; mode=%s DISPLAY=%s\n' "$mode" "${DISPLAY:-unset}"
  sha256sum app/paint/painter-mypaint-surface/paint-core.cpp app/paint/painter-mypaint-surface/gimp-resources.cpp app/tools/gimppaintermybrushtool.cpp app/tests/test-painter-mypaint-tool.cpp "$binary"
  set +e
  "$binary"
  status=$?
  printf 'MYPAINT_PIPE_TOOL_EXIT=%s\n' "$status"
  exit "$status"
} 2>&1 | tee "$root/migration/tests/mypaint-pipe-tool-$mode.log"
exit "${PIPESTATUS[0]}"
