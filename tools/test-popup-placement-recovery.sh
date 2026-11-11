#!/usr/bin/env bash
# Source the pinned dependency environment first. Local diagnostics, not CI.
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
config=${1:?usage: $0 configured-build-directory output-directory}
out=${2:?usage: $0 configured-build-directory output-directory}
mkdir -p "$out"
out=$(realpath "$out")
config=$(realpath "$config")
tmp=$(mktemp -d)
pid=
trap '[[ -z "$pid" ]] || kill "$pid" 2>/dev/null || true; rm -rf "$tmp"' EXIT
cd "$repo"
git show 74819d68427a0d68647b13310e5ed6399b88044b:app/widgets/popupper.cpp > "$tmp/popupper-old.cpp"
python3 - "$tmp/popupper-window-size.cpp" <<'PY'
import sys
s = open('app/widgets/popupper.cpp').read()
start = s.index('void gimp_painter_popup_show_over')
a, b = s[:start], s[start:]
old = 'gtk_widget_get_preferred_size (popup, nullptr, &natural);'
assert b.count(old) == 1
b = b.replace(old, 'gtk_window_get_size (GTK_WINDOW (popup), &natural.width, &natural.height);')
open(sys.argv[1], 'w').write(a + b)
PY
for variant in fixed old window-size; do
  source=app/widgets/popupper.cpp
  [[ "$variant" == fixed ]] || source="$tmp/popupper-$variant.cpp"
  g++ -std=c++14 -I"$config" -Iapp -Iapp/widgets $(pkg-config --cflags gtk+-3.0) \
    app/tests/test-popup-placement-recovery.cpp "$source" \
    app/painter/binding-store.cpp app/painter/gimp-painter-binding.cpp \
    app/painter/gimp-painter-error.cpp $(pkg-config --libs gtk+-3.0) \
    -o "$tmp/test-$variant" > "$out/build-$variant.log" 2>&1
done
umask 077
auth="$tmp/xauth"
touch "$auth"
display=${POPUP_TEST_DISPLAY:-119}
xauth -f "$auth" add "127.0.0.1:$display" MIT-MAGIC-COOKIE-1 "$(python3 -c 'import secrets; print(secrets.token_hex(16))')"
Xvfb ":$display" -screen 0 1080x1920x24 -listen tcp -nolisten unix -nolisten local -pn -auth "$auth" >"$out/xvfb.log" 2>&1 & pid=$!
sleep 2
kill -0 "$pid"
export DISPLAY="127.0.0.1:$display" XAUTHORITY="$auth" GDK_BACKEND=x11
export XDG_CACHE_HOME="$tmp/cache"
mkdir -p "$XDG_CACHE_HOME"
ulimit -c 0
for scale in 1 2; do
  GDK_SCALE=$scale "$tmp/test-fixed" > "$out/fixed-scale$scale.log" 2>&1
  for variant in old window-size; do
    case=neither-side
    [[ "$variant" == old ]] || case=preferred-not-window-size
    set +e
    GDK_SCALE=$scale "$tmp/test-$variant" -p "/popup-placement/$case" > "$out/$variant-scale$scale.log" 2>&1
    status=$?
    set -e
    [[ "$status" != 0 ]]
    grep -q 'assertion failed (.* == expected_' "$out/$variant-scale$scale.log"
    printf '%s scale=%s intentional RED exit=%s\n' "$variant" "$scale" "$status"
  done
  printf 'fixed scale=%s GREEN\n' "$scale"
done
