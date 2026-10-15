#!/bin/bash
set -e
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
make -C "$base/gimp/app/tests" -j4 test-xcf 'test_xcf_LDADD=$(LDADD) ../gimp-features.o -lstdc++ ../presets/libapppresets.a -ljson-glib-1.0' > "$base/build.log" 2>&1
sha256sum "$base/gimp/app/tests/.libs/test-xcf" > "$base/binary.sha256"
