#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
set +u
source "$base/build/env.sh"
set -u
variant=${1:?builtin or scripted}
cp "$base/fixture-sources/test-xcf-$variant.c" "$base/gimp/app/tests/test-xcf.c"
cp "$base/harness-staging/app/core/gimpfilterlayer.cpp" "$base/gimp/app/core/gimpfilterlayer.cpp"
make -C "$base/gimp/app/core" -j4 > "$base/harness-$variant-core-build.log" 2>&1
make -C "$base/gimp/app/tests" -j4 test-xcf 'test_xcf_LDADD=$(LDADD) ../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0' > "$base/harness-$variant-build.log" 2>&1
sha256sum "$base/gimp/app/tests/.libs/test-xcf" > "$base/harness-$variant-binary.sha256"
mkdir -p "$base/capture-$variant/profile/scripts" "$base/capture-$variant/fixtures"
if [ "$variant" = scripted ]; then
 cp "$base/fixture-sources/legacy-fixture-edge-drawable.scm" "$base/capture-$variant/profile/scripts/"
fi
export GIMP2_DIRECTORY="$base/capture-$variant/profile"
export GIMP_PAINTER_FIXTURE_DIR="$base/capture-$variant/fixtures"
export GIMP_PAINTER_CAPTURE_FILTER=1
set +e
timeout 60 "$base/gimp/app/tests/test-xcf" > "$base/capture-$variant/capture.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$base/capture-$variant/exit-code"
exit "$status"
