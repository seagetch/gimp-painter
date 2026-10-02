#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Execute the actual GTK dialog tests from the cloud native terminal.
# Builds are separate; share this lock with every build/GUI phase.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath "${1:-$root/build-debian13}")"
mode="${2:-normal}"
case "$mode" in
  normal) binary="$build/app/tests/painter-layer-ui" ;;
  asan) binary="$build/app/tests/painter-layer-ui-asan" ;;
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
export UI_TEST=yes GSETTINGS_BACKEND=memory
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
if [[ "$mode" == asan ]]; then
  python3 - "$root/migration/tests/layer-dialog-store-sanitizer-build.json" "$binary" <<'VERIFY'
import hashlib, json, pathlib, sys
report = json.loads(pathlib.Path(sys.argv[1]).read_text())
assert report['build_status'] == 'passed' and not report['changed_during_build']
assert hashlib.sha256(pathlib.Path(sys.argv[2]).read_bytes()).hexdigest() == report['executable_sha256']
assert hashlib.sha256(pathlib.Path(report['source_archive']).read_bytes()).hexdigest() == report['source_archive_sha256']
print('Instrumented source archive:', report['source_archive_sha256'])
VERIFY
fi
{
  printf 'Painter dialog BindingStore native GTK; mode=%s DISPLAY=%s\n' "$mode" "${DISPLAY:-unset}"
  sha256sum app/dialogs/painter-layer-dialog.cpp app/dialogs/painter-layer-dialog.h \
    app/tests/test-painter-layer-ui.c app/painter/binding-store.cpp \
    app/painter/binding-store.hpp app/painter/connection.hpp "$binary"
  set +e
  "$binary"
  status=$?
  printf 'LAYER_DIALOG_STORE_EXIT=%s\n' "$status"
  exit "$status"
} 2>&1 | tee "$root/migration/tests/layer-dialog-store-$mode.log"
exit "${PIPESTATUS[0]}"
