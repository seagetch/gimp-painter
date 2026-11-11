#!/bin/bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/../../.." && pwd)
build=${1:?pass configured build path}
# Source tools/linux-debian13-env.sh before running this local diagnostic.
out="$repo/migration/tests/popup-placement-recovery"
tmp=$(mktemp -d); pid=
trap '[[ -z "$pid" ]] || kill "$pid" 2>/dev/null || true; rm -rf "$tmp"' EXIT
umask 077; touch "$tmp/auth"
xauth -f "$tmp/auth" add 127.0.0.1:120 MIT-MAGIC-COOKIE-1 "$(python3 -c 'import secrets;print(secrets.token_hex(16))')"
Xvfb :120 -screen 0 ${SCREEN:-1080x1920x24} -listen tcp -nolisten unix -nolisten local -pn -auth "$tmp/auth" > "$out/native-xvfb.log" 2>&1 & pid=$!
sleep 2; kill -0 "$pid"
export DISPLAY=127.0.0.1:120 XAUTHORITY="$tmp/auth" GDK_BACKEND=x11 XDG_CACHE_HOME="$tmp/cache"
export GSETTINGS_BACKEND=memory XDG_RUNTIME_DIR="$tmp"
export XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config"
mkdir -p "$XDG_DATA_HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
export GIMP_TESTING_ABS_TOP_SRCDIR="$repo" GIMP_TESTING_ABS_TOP_BUILDDIR="$build" GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
export GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas
export BABL_PATH="$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu/babl-0.1" GEGL_PATH="$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu/gegl-0.4"
for scale in ${SCALES:-1}; do
 GDK_SCALE=$scale GIMP_INK_POPUP_CAPTURE="$out/ink-portrait-scale$scale.png" "$build/app/tests/painter-compact-options" -p /ink-native-popup/actual-blob > "$out/ink-native-scale$scale.log" 2>&1
 GDK_SCALE=$scale "$build/app/tests/painter-compact-options" -p /ink-native-popup > "$out/ink-suite-scale$scale.log" 2>&1
done
