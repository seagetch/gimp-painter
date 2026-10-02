#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Launch from the authorized cloud desktop terminal to include real GTK tests.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$root"
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$root/build-debian13"
export GIMP_TESTING_PLUGINDIRS="$root/build-debian13/plug-ins/common" UI_TEST=yes
export GIMP3_DIRECTORY="$root/build-debian13/geometry-native-profile"
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
python3 - <<'VERIFY'
from pathlib import Path
import hashlib,json
r=json.loads(Path('migration/tests/brush-geometry-sanitizer-build.json').read_text())
assert not r['changed_during_build']
for name,digest in r['sources_sha256'].items():assert hashlib.sha256(Path(name).read_bytes()).hexdigest()==digest,name
for record in r['executables'].values():assert hashlib.sha256(Path(record['path']).read_bytes()).hexdigest()==record['sha256']
VERIFY
printf 'Cloud native GTK DISPLAY=%s\n' "${DISPLAY:-unset}"
[[ -n "${DISPLAY:-}" ]]
private=build-debian13/brush-geometry-sanitizers
"$private/painter-brush-geometry" > migration/tests/brush-geometry-asan-native.log 2>&1
printf 'GEOMETRY_ASAN_UNIT_EXIT=0\n'
python3 migration/tests/compare_brush_geometry.py "$private/painter-brush-geometry-trace" --report migration/tests/brush-geometry-asan.json
python3 migration/tests/compare_brush_geometry.py "$private/painter-brush-geometry-trace" --pixmap --report migration/tests/brush-geometry-pixmap-asan.json
for mode in normal asan; do
  if [[ "$mode" == normal ]]; then binary=build-debian13/app/tests/painter-profile-native; else binary="$private/painter-profile-native"; fi
  "$binary" > "migration/tests/brush-geometry-profile-$mode.log" 2>&1
  printf 'GEOMETRY_PROFILE_%s_EXIT=0\n' "$mode"
done
printf 'BRUSH_GEOMETRY_NATIVE_COMPLETE=0\n'
