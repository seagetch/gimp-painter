#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
bash "$base/compile.sh" > "$base/compile.log" 2>&1
make -C "$base/gimp/app/paint" -j4 > "$base/paint-build.log" 2>&1
touch "$base/gimp/app/tests/test-xcf.c"
make -C "$base/gimp/app/tests" -j4 test-xcf 'test_xcf_LDADD=stroke-capture.o $(LDADD) ../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0' > "$base/test-build.log" 2>&1
