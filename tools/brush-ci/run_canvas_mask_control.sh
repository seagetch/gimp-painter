#!/usr/bin/env bash
# Independent bounded proof; existing common107 and all original gates remain.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
src=$(realpath "${1:?source required}"); build=$(realpath "${2:?build required}")
run=$(realpath "${3:?private scratch required}"); out=$(realpath "${4:?artifact directory required}")
mode=${5:?normal or sanitizer required}
[[ $mode == normal || $mode == sanitizer ]] || exit 2
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary hosted Actions only' >&2; exit 2; }
: "${BRUSH_CI_RUNTIME_PINS:?automatic source seal required}"
[[ -z ${LD_PRELOAD:-} && -z ${LD_AUDIT:-} ]] || { echo 'BLOCKED: injected loader provider' >&2; exit 2; }
python3 -B - "$run" "${RUNNER_TEMP:?}" <<'PY'
import os,pathlib,re,stat,sys
run,root=map(pathlib.Path,sys.argv[1:]); info=run.lstat()
assert run.parent==root.resolve() and re.fullmatch(r'painter-brush-ci\.[A-Za-z0-9]{8}',run.name)
assert stat.S_ISDIR(info.st_mode) and not run.is_symlink() and info.st_uid==os.getuid() and info.st_mode&0o077==0
PY
evidence="$out/canvas-mask-provider-$mode"
mkdir "$evidence"
python3 -B "$here/canvas_mask_provider.py" --provider --source "$src" --build "$build" \
  --mode "$mode" --output "$evidence/provider.json"
failed=0
labels=(fixed-focused)
[[ $mode != normal ]] || labels+=(old-focused fixed-full107)
for label in "${labels[@]}"; do
  target=canvas-mask-fixed-provider
  [[ $label != old-focused ]] || target=canvas-mask-old-provider
  profile="$run/profile-canvas-mask-$mode-$label"
  mkdir "$profile"
  mkdir "$profile"/{home,config,cache,data,state,runtime,tmp,gimp-temp,gimp-cache}
  chmod 700 "$profile" "$profile"/{home,config,cache,data,state,runtime,tmp,gimp-temp,gimp-cache}
  args=()
  [[ $label == fixed-full107 ]] || args=(--test-args '-p /painter-canvas-ui/renderer_mask_cache_and_release')
  status=0
  (
    export HOME="$profile/home" XDG_CONFIG_HOME="$profile/config" XDG_CACHE_HOME="$profile/cache"
    export XDG_DATA_HOME="$profile/data" XDG_STATE_HOME="$profile/state" XDG_RUNTIME_DIR="$profile/runtime"
    export TMPDIR="$profile/tmp" GIMP3_TEMPDIR="$profile/gimp-temp" GIMP3_CACHEDIR="$profile/gimp-cache"
    export GDK_BACKEND=x11 GDK_SCALE=1 GDK_DPI_SCALE=1 PYTHONDONTWRITEBYTECODE=1
    unset DISPLAY WAYLAND_DISPLAY GIMP3_DIRECTORY GSETTINGS_BACKEND NO_AT_BRIDGE LSAN_OPTIONS
    unset PAINTER_CANVAS_DEMO GIMP_TESTING_PAINTER_SESSION_DIRECTORY GIMP_TESTING_PAINTER_SESSION_PAIR
    unset GIMP_TEST_LAYER_UI_FILTER GIMP_TEST_COLORIFY_ONCANVAS
    unset GIMP_TEST_TOOLBAR_PAINTER GIMP_TEST_TOOLBAR_SINGLE GIMP_TEST_TOOLBAR_SESSION_DIRECTORY GIMP_TEST_TOOLBAR_SESSION_PASS
    unset GIMP_TEST_COMMON_POPUP_SCREENSHOTS GIMP_TEST_COMMON_POPUP_DIAGNOSTICS
    unset GIMP_TEST_COMMON_POPUP_DOCK_TRACE GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES
    unset GIMP_TEST_COMMON_LAYER_POPUP GIMP_TEST_FILTER_LAYER_POPUP GIMP_TEST_OVERLAY_PAINT_GATE
    unset GIMP_BRUSH_POPUP_CAPTURE GIMP_BRUSH_POPUP_BOTTOM_CAPTURE
    unset ASAN_OPTIONS UBSAN_OPTIONS
    if [[ $mode == sanitizer ]]; then
      export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1
      export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
    fi
    xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
      dbus-run-session -- meson test -C "$build" --no-rebuild --num-processes 1 \
        --print-errorlogs --logbase "canvas-mask-$mode-$label" "${args[@]}" "$target"
  ) > "$run/canvas-mask-$mode-$label-console.log" 2>&1 || status=$?
  # Old should_fail makes Meson status0 expected, but raw child must still be -5.
  [[ $status == 0 ]] || failed=1
  selection=focused
  [[ $label != fixed-full107 ]] || selection=full107
  python3 -B "$here/canvas_mask_provider.py" --observe-log "$build/meson-logs/canvas-mask-$mode-$label.json" \
    --build "$build" --mode "$mode" --target "$target" --selection "$selection" --shell-exit "$status" \
    --output "$evidence/$label-status.json" || failed=1
done
args=(--aggregate --provider-receipt "$evidence/provider.json"
      --candidate-log "$build/meson-logs/canvas-mask-$mode-fixed-focused.json"
      --output "$evidence/aggregate.json")
if [[ $mode == normal ]]; then
  args+=(--old-log "$build/meson-logs/canvas-mask-normal-old-focused.json"
         --full-log "$build/meson-logs/canvas-mask-normal-fixed-full107.json")
fi
python3 -B "$here/canvas_mask_provider.py" "${args[@]}" || failed=1
exit "$failed"
