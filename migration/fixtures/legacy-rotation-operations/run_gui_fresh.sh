#!/bin/bash
set -eu
base=$(cd "$(dirname "$0")" && pwd)
# Proposed replay helper; actual capture used sealed run_gui.sh.
[ "$#" -eq 1 ] || { echo "usage: $0 NEW_OUTPUT_NAME" >&2; exit 2; }
case "$1" in ''|.*|*[!A-Za-z0-9_-]*) echo "Use a simple new output-directory name." >&2; exit 2;; esac
out="$base/$1"
# mkdir is atomic and refuses every existing file/directory/symlink. Never delete evidence.
mkdir -- "$out" || { echo "Output already exists or cannot be created; refusing to reuse it." >&2; exit 2; }
source "$base/../legacy-filter-positive-20261015/build/env.sh"
mkdir -- "$out/profile";out=$(cd "$out" && pwd)
export GIMP2_DIRECTORY="$out/profile" CAPTURE_DIR="$out"
export GIMP_TESTING_ABS_TOP_SRCDIR="$base/gimp" GIMP_TESTING_ABS_TOP_BUILDDIR="$base/gimp"
python3 "$base/run_authenticated_display.py" --out "$out/display" --timeout 90 -- "$base/gimp/app/tests/test-ui"
