#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Source the documented Debian environment first. Run the runtime mode on GTK.
set -uo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(realpath -m "${1:-$root/build-http-disabled}")"
mode="${2:-run}"
cd "$root" || exit 2
exec 9>"${GIMP_PAINTER_BUILD_LOCK:-/workspace/shared/gimp-painter-build.lock}"; flock 9
if [[ "$mode" == build ]]; then
  wrapper="${build}-pkg-config-no-soup"
  cat > "$wrapper" <<'WRAPPER'
#!/usr/bin/env bash
for argument in "$@"; do
  case "$argument" in libsoup*) echo 'Unexpected Soup dependency in disabled build' >&2; exit 1;; esac
done
exec /usr/bin/pkg-config "$@"
WRAPPER
  chmod +x "$wrapper"
  if [[ -f "$build/meson-private/coredata.dat" ]]; then
    meson configure "$build" -Dpainter-http=disabled || exit $?
  else
    PKG_CONFIG="$wrapper" meson setup "$build" --auto-features=disabled -Dlibunwind=false \
      -Dpainter-http=disabled --prefix="${GIMP_BUILD_PREFIX:-$(dirname "$root")/.build-prefix-debian13}" || exit $?
  fi
  ninja -C "$build" -j6 app/gimp-3.0 app/gimp-console-3.0 \
    app/tests/gimp-painter-surface app/tests/painter-xcf-roundtrip \
    app/tests/painter-canvas-ui menus/image-menu.ui menus/dockable-menu.ui || exit $?
  python3 migration/tests/verify_http_disabled.py "$build" --report "$build/http-disabled-link.json"
  exit $?
fi
[[ "$mode" == run ]] || exit 2
export GSETTINGS_BACKEND=memory
export GIMP_TESTING_ABS_TOP_SRCDIR="$root" GIMP_TESTING_ABS_TOP_BUILDDIR="$build"
export GIMP_TESTING_PLUGINDIRS="$build/plug-ins/common" UI_TEST=yes
unset GIMP_PAINTER_HTTP_ENABLE GIMP_PAINTER_HTTP_TOKEN GIMP_PAINTER_HTTP_PORT GIMP_PAINTER_HTTP_WEBHOOK_ORIGIN
{
  "$build/app/tests/gimp-painter-surface"; paint=$?
  "$build/app/tests/painter-xcf-roundtrip"; xcf=$?
  "$build/app/tests/painter-canvas-ui"; ui=$?
  printf 'HTTP_DISABLED_PAINT_EXIT=%s XCF_EXIT=%s UI_EXIT=%s\n' "$paint" "$xcf" "$ui"
  [[ "$paint" == 0 && "$xcf" == 0 && "$ui" == 0 ]]
} 2>&1 | tee "$build/http-disabled-native.log"
exit "${PIPESTATUS[0]}"
