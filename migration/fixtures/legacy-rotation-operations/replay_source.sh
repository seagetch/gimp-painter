#!/bin/bash
set -eu
base=$(cd "$(dirname "$0")" && pwd)
source "$base/../legacy-filter-positive-20261015/build/env.sh"
python3 "$base/prepare_source_capture.py"
mkdir -p "$base/source-captures" "$base/repeat-source-captures"
cc -std=gnu11 -O0 $(pkg-config --cflags cairo) "$base/source-capture.c" $(pkg-config --libs cairo) -lm -o "$base/source-capture"
"$base/source-capture" > "$base/source-captures/rotation.tsv"
cc -std=gnu11 -O2 $(pkg-config --cflags cairo) "$base/source-capture.c" $(pkg-config --libs cairo) -lm -o "$base/source-capture-O2"
"$base/source-capture-O2" > "$base/repeat-source-captures/rotation.tsv"
cmp "$base/source-captures/rotation.tsv" "$base/repeat-source-captures/rotation.tsv"
sha256sum "$base/source-capture" "$base/source-capture-O2" > "$base/source-binaries.sha256"
