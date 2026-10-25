#!/usr/bin/env bash
# Focused native GUI acceptance on an ordinary authorized CI runner.
# No host/socket-policy changes, TCP-display workaround, install, or deployment.
set -euo pipefail

repo=$(git rev-parse --show-toplevel)
out=$(realpath -m "${1:?Usage: run-painter-ui-ci.sh ARTIFACT_DIRECTORY}")
mode=${2:-candidate}
case "$mode" in candidate|baseline-carrier-red|pre-fix-stack-978|dockbook-stack-3d6f|dockbook-controlled-order-3d6f) ;; *) echo 'Unknown CI mode' >&2; exit 2;; esac
mkdir -p "$out"
run=$(mktemp -d "${RUNNER_TEMP:-/tmp}/painter-native-ui.XXXXXXXX")
src="$run/source"
build="$run/build"
commit=$(git -C "$repo" rev-parse HEAD)
baseline=d4ce384f708090facbf6b08edc38b4ddcf84cff0
# Historical source is immutable even when the publishing candidate is fixed.
stack_commit=978461dde961ee6f11067b8f9c68085bc442a600
stack_tree=5e0d6678c6eda0f042e458ec5cb3c3c21a6d53a2
stack_test_sha256=88f76f3e98c09168a1a8a9da1363808c25191fafdd1d88d652c462e31321e8c1
dockbook_commit=3d6f62235c6bc9e2505d2c2b6ad1d42e15d081af
dockbook_tree=a5a14a81a10857834301888a0cd2b3cf902bf17d
# Historical diagnostic harness stays immutable as the candidate test evolves.
dockbook_donor_commit=1618cc5958148c7d534f9e3893681e14fcc94d80
dockbook_donor_tree=253fcbdeb7c39391ddf8bad89b7877e1e18911b8
dockbook_donor="$run/dockbook-donor"
dockbook_diagnostic_mode=passive
dockbook_overlay=dockbook-overlay.json
if [[ "$mode" == dockbook-controlled-order-3d6f ]]; then
  dockbook_diagnostic_mode=controlled-order
  dockbook_overlay=dockbook-controlled-order-overlay.json
fi
stage=preflight
record_exit() {
  result=$?
  trap - EXIT
  printf 'stage=%s\nexit_code=%s\ncommit=%s\n' "$stage" "$result" "$commit" > "$out/status.txt"
  if [[ -d "$src/.git" || -f "$src/.git" ]]; then
    git -C "$src" diff --stat > "$out/generated-source-diff-stat.txt" || true
    git -C "$src" diff --binary > "$out/generated-source.patch" || true
    git -C "$src" status --porcelain=v1 > "$out/source-after.status" || true
  fi
  df -h "$run" > "$out/disk-after.txt" || true
  exit "$result"
}
trap record_exit EXIT

{
  printf 'commit=%s\ntree=%s\n' "$commit" "$(git -C "$repo" rev-parse HEAD^{tree})"
  printf 'mode=%s\n' "$mode"
  printf 'scope=focused native UI; not full GIMP/plug-in or aggregate acceptance\n'
  printf 'image_inspection=pending, even if automated checks pass\n'
  sha256sum "$repo/migration/baseline/debian13-package-lock.json"
  cat /etc/os-release
  gcc --version | head -n 1
  dpkg-query -W ca-certificates git python3 build-essential pkgconf libc6 \
    xvfb xauth dbus-x11 libxtst6 librsvg2-common fontconfig fonts-cantarell \
    glib-networking at-spi2-core libblas3 liblapack3 libgtk-3-common
  if [[ "$mode" == pre-fix-stack-978 || "$mode" == dockbook-stack-3d6f || "$mode" == dockbook-controlled-order-3d6f ]]; then dpkg-query -W gdb; fi
} > "$out/source-identity.txt"
df -h "$run" > "$out/disk-before.txt"
# Do not delete runner software or borrow a paid larger runner automatically.
available=$(df -Pk "$run" | awk 'NR == 2 {print $4}')
if (( available < 8 * 1024 * 1024 )); then
  echo 'Need at least 8 GiB free for the fresh focused build; capacity is not guaranteed.' >&2
  exit 1
fi

stage=source
source_commit=$commit
if [[ "$mode" == baseline-carrier-red ]]; then
  git -C "$repo" fetch --no-tags --depth=1 origin "$baseline"
  source_commit=$baseline
elif [[ "$mode" == pre-fix-stack-978 ]]; then
  git -C "$repo" fetch --no-tags --depth=1 origin "$stack_commit"
  source_commit=$stack_commit
elif [[ "$mode" == dockbook-stack-3d6f || "$mode" == dockbook-controlled-order-3d6f ]]; then
  git -C "$repo" fetch --no-tags --depth=1 origin "$dockbook_commit"
  source_commit=$dockbook_commit
fi
git -C "$repo" worktree add --detach "$src" "$source_commit"
if [[ "$mode" == pre-fix-stack-978 ]]; then
  test "$(git -C "$src" rev-parse HEAD)" = "$stack_commit"
  test "$(git -C "$src" rev-parse HEAD^{tree})" = "$stack_tree"
  printf '%s  %s\n' "$stack_test_sha256" "$src/app/tests/test-common-layer-popup.cpp" | sha256sum -c -
  {
    printf 'diagnostic_only=true\nacceptance_eligible=false\n'
    printf 'tested_commit=%s\ntested_tree=%s\n' "$stack_commit" "$stack_tree"
    printf 'test_sha256=%s\noverlay=none\n' "$stack_test_sha256"
    printf 'selection_basis=next registered sibling after run 40176782833 last passed staged-child-0; crash location not yet proven\n'
  } > "$out/stack-source-identity.txt"
fi
git -C "$src" -c submodule.gimp-data.url=https://github.com/GNOME/gimp-data.git \
  submodule update --init --depth=1 gimp-data
git -C "$src" submodule status > "$out/submodule-identity.txt"
test "$(git -C "$src/gimp-data" rev-parse HEAD)" = \
  "$(git -C "$src" rev-parse HEAD:gimp-data)"
test -z "$(git -C "$src" status --porcelain=v1 --untracked-files=all)"
if [[ "$mode" == baseline-carrier-red ]]; then
  python3 -B "$repo/tools/prepare-painter-popup-baseline.py" \
    --baseline "$src" --candidate "$repo" --output "$out/baseline-overlay.json"
elif [[ "$mode" == dockbook-stack-3d6f || "$mode" == dockbook-controlled-order-3d6f ]]; then
  test "$(git -C "$src" rev-parse HEAD)" = "$dockbook_commit"
  test "$(git -C "$src" rev-parse HEAD^{tree})" = "$dockbook_tree"
  # A separate current-user-owned exact public checkout supplies only the
  # reviewed historical test TU. Its production and uninitialized data are not
  # imported; the preparer verifies donor commit/tree/index/blob/raw bytes.
  git init "$dockbook_donor"
  git -C "$dockbook_donor" remote add origin https://github.com/seagetch/gimp-painter.git
  git -C "$dockbook_donor" fetch --no-tags --depth=1 origin "$dockbook_donor_commit"
  git -C "$dockbook_donor" checkout --detach FETCH_HEAD
  test "$(git -C "$dockbook_donor" rev-parse HEAD)" = "$dockbook_donor_commit"
  test "$(git -C "$dockbook_donor" rev-parse HEAD^{tree})" = "$dockbook_donor_tree"
  python3 -B "$repo/tools/prepare-painter-dockbook-diagnostic.py" \
    --source "$src" --donor "$dockbook_donor" --diagnostic-mode "$dockbook_diagnostic_mode" \
    --output "$out/$dockbook_overlay"
fi
verify_source() {
  if [[ "$mode" == baseline-carrier-red ]]; then
    python3 -B "$repo/tools/prepare-painter-popup-baseline.py" \
      --baseline "$src" --verify "$out/baseline-overlay.json"
  elif [[ "$mode" == dockbook-stack-3d6f || "$mode" == dockbook-controlled-order-3d6f ]]; then
    python3 -B "$repo/tools/prepare-painter-dockbook-diagnostic.py" \
      --source "$src" --diagnostic-mode "$dockbook_diagnostic_mode" --verify "$out/$dockbook_overlay"
  else
    git -C "$src" diff --exit-code && \
      test -z "$(git -C "$src" ls-files --others --exclude-standard)"
  fi
}
sha256sum "$src/app/tests/test-common-layer-popup.cpp" > "$out/focused-test-source.sha256"

stage=dependencies
export GIMP_DEPS_DIRECTORY="$run/deps"
export GIMP_BUILD_PREFIX="$run/prefix"
python3 -B "$src/tools/prepare-linux-build-deps.py" \
  --directory "$GIMP_DEPS_DIRECTORY" 2>&1 | tee "$out/dependencies.log"
source "$src/tools/linux-debian13-env.sh"
{
  meson --version
  ninja --version
  pkg-config --modversion gtk+-3.0 glib-2.0 gegl-0.4 babl-0.1 gexiv2 libmypaint
} > "$out/dependency-versions.txt"
export CCACHE_DISABLE=1 PYTHONDONTWRITEBYTECODE=1 GDK_BACKEND=x11
unset GSETTINGS_BACKEND GIMP3_DATADIR GIMP3_DIRECTORY GIMP3_LOCALEDIR

# Check the actual composite runtime lookup before an expensive native build.
# Normal package triggers supply the compiled GTK schemas; do not hide missing
# schemas with a settings backend change or a private replacement schema.
stage=runtime-schemas
gsettings list-schemas | sort > "$out/gsettings-schemas.txt"
for schema in org.gtk.Settings.FileChooser org.gtk.Settings.ColorChooser \
              org.gtk.Settings.EmojiChooser org.gtk.Settings.Debug; do
  grep -Fxq "$schema" "$out/gsettings-schemas.txt" || {
    echo "Required GTK runtime schema missing: $schema" >&2
    exit 1
  }
done

stage=configure
meson setup "$build" "$src" --prefix="$GIMP_BUILD_PREFIX" \
  --buildtype=debugoptimized -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=disabled -Dpainter-http=disabled \
  -Denable-console-bin=false -Dgi-docgen=disabled -Dappdata-test=disabled \
  -Djavascript=disabled -Dvala=disabled -Dprint=false -Dcheck-update=no \
  -Dlibbacktrace=false 2>&1 | tee "$out/configure.log"

stage=compile
# Reuse the real production libraries and committed icon-fixture dependencies.
# Never edit Meson backends, substitute objects, or build all optional plug-ins.
targets=(painter-layer-ui painter-public-headers-c painter-public-headers-cpp \
  menus/image-menu.ui:custom menus/dockable-menu.ui:custom)
if [[ "$mode" == candidate ]]; then
  targets+=(app/gimp-3.0:executable app/gimp-painter-filter-worker:executable \
    plug-ins/common/blinds:executable plug-ins/common/tile-small:executable \
    plug-ins/common/contrast-retinex:executable plug-ins/common/convolution-matrix:executable \
    painter-layer-tree layer-tile-entry-contracts \
    native-popover-entry painter-popup-handles painter-canvas-ui \
    painter-widget-exceptions painter-layer-dispatch \
    gimp30-gmo:alias gimp30-libgimp-gmo:alias gimp30-std-plug-ins-gmo:alias \
    gimp30-python-gmo:alias gimp30-script-fu-gmo:alias \
    org.gimp.extension.goat-exercises-gmo:alias)
fi
meson compile -C "$build" -j 2 "${targets[@]}" \
  2>&1 | tee "$out/compile.log"
verify_source

# Filter plug-ins require actual compiled catalogs, not an empty directory.
# Reuse the existing strict Meson install-plan/catalog validator; this stages
# only private test output, never a system installation or source-tree alias.
locale_state() {
  python3 -B - "$repo/tools/run-painter-frozen-aggregate.py" "$build" \
    "$run/locale" "$out/locales.json" "$1" <<'PYLOCALE'
import hashlib, json, pathlib, runpy, sys
helper, build, destination, output, operation = sys.argv[1:]
functions = runpy.run_path(helper)
if operation == 'stage':
    manifest = functions['stage_build_locales'](build, destination)
    manifest['helper_sha256'] = hashlib.sha256(pathlib.Path(helper).read_bytes()).hexdigest()
    pathlib.Path(output).write_text(json.dumps(manifest, indent=2) + '\n')
else:
    manifest = json.loads(pathlib.Path(output).read_text())
    if manifest['helper_sha256'] != hashlib.sha256(pathlib.Path(helper).read_bytes()).hexdigest():
        raise RuntimeError('Locale staging helper changed')
    functions['verify_staged_locales'](destination, manifest)
print('Compiled locale catalogs ' + operation + ': ' + str(manifest['catalog_count']))
PYLOCALE
}
if [[ "$mode" == candidate ]]; then
  stage=locales
  locale_state stage
  export GIMP3_LOCALEDIR="$run/locale"
fi

failed=0
stage=header-verification
meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs \
  --logbase header-verification painter-public-header-compilation \
  painter-additional-header-routes > "$run/header-console.log" 2>&1 || failed=1
python3 -B "$repo/tools/check_painter_ui_ci.py" \
  --log "$build/meson-logs/header-verification.json" \
  --expect painter-public-header-compilation painter-additional-header-routes \
  --output "$out/header-verification-results.json" || failed=1
python3 - "$run/header-console.log" "$out/header-console.log" <<'PY'
import pathlib, sys
lines = pathlib.Path(sys.argv[1]).read_text(errors='replace').splitlines()
safe = [line for line in lines if not line.lstrip().startswith(('>>>', 'Inherited environment:'))]
pathlib.Path(sys.argv[2]).write_text('\n'.join(safe) + '\n')
PY
if [[ -f "$build/app/painter/tests/headers/header-compile-result.json" ]]; then
  cp "$build/app/painter/tests/headers/header-compile-result.json" "$out/header-compile-result.json"
fi

regressions=(painter-layer-ui painter-filter-layer-popup painter-layer-tree \
  layer-tile-entry-contracts native-popover-entry painter-popup-handles \
  painter-canvas-ui painter-right-docks-ui painter-dock-session-ui \
  painter-dock-input-ui painter-dock-layout-ui painter-widget-exceptions \
  painter-layer-dispatch)

# Each invocation gets fresh application state; raw environment-bearing Meson
# logs and Xauthority/D-Bus state remain private under $run, never artifacts.
test_group() {
  local label=$1 scale=$2 capture=$3
  shift 3
  local profile="$run/profile-$label"
  mkdir -p "$profile"/{home,config,cache,data,state,runtime,tmp,gimp-temp,gimp-cache}
  chmod 700 "$profile/runtime" "$profile/tmp" "$profile/gimp-temp" "$profile/gimp-cache"
  export HOME="$profile/home" XDG_CONFIG_HOME="$profile/config"
  export XDG_CACHE_HOME="$profile/cache" XDG_DATA_HOME="$profile/data"
  export XDG_STATE_HOME="$profile/state" XDG_RUNTIME_DIR="$profile/runtime"
  export TMPDIR="$profile/tmp" GDK_SCALE="$scale" GDK_DPI_SCALE=1
  export GIMP3_TEMPDIR="$profile/gimp-temp" GIMP3_CACHEDIR="$profile/gimp-cache"
  unset GIMP_TEST_COMMON_POPUP_SCREENSHOTS GIMP_TEST_COMMON_POPUP_DIAGNOSTICS
  unset GIMP_TEST_COMMON_POPUP_DOCK_TRACE GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES
  # Meson's selected registration, not inherited shell state, selects a suite.
  unset GIMP_TEST_COMMON_LAYER_POPUP GIMP_TEST_FILTER_LAYER_POPUP GIMP_TEST_OVERLAY_PAINT_GATE
  if [[ -n "${dockbook_prefix:-}" ]]; then
    export GIMP_TEST_COMMON_POPUP_DOCK_TRACE="$out/dockbook-lifecycle-trace.txt"
    if [[ "${dockbook_controlled_order:-0}" == 1 ]]; then
      export GIMP_TEST_COMMON_POPUP_DOCK_TRACE="$out/dockbook-controlled-order-trace.txt"
      export GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES=1
    fi
  fi
  if [[ "$#" == 1 && "$1" == painter-common-layer-popup ]]; then
    mkdir -p "$out/diagnostics-$label"
    export GIMP_TEST_COMMON_POPUP_DIAGNOSTICS="$out/diagnostics-$label"
  fi
  if [[ "$capture" == yes ]]; then
    mkdir -p "$out/$label"
    export GIMP_TEST_COMMON_POPUP_SCREENSHOTS="$out/$label"
  fi
  # xvfb-run creates private MIT-MAGIC-COOKIE-1 credentials. Normal Unix X11
  # transport is used; TCP stays disabled. Meson's landscape wrapper is off.
  local status=0 gate=0 extra=()
  if [[ -n "${red_case:-}" ]]; then extra+=(--test-args "-p $red_case"); fi
  if [[ -n "${diagnostic_prefix:-}" ]]; then
    extra+=(--test-args "--run-prefix $diagnostic_prefix" --timeout-multiplier 0.5)
  fi
  if [[ -n "${stack_case:-}" ]]; then
    extra+=(--test-args "-p $stack_case" --timeout-multiplier 0.5
            --wrapper "$run/gdb-stack-wrapper.sh")
  fi
  if [[ -n "${dockbook_prefix:-}" ]]; then
    extra+=(--test-args "--run-prefix $dockbook_prefix" --timeout-multiplier 0.5
            --wrapper "$run/gdb-dockbook-wrapper.sh")
  fi
  xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
    dbus-run-session -- meson test -C "$build" --no-rebuild \
      --num-processes 1 --print-errorlogs --logbase "$label" "${extra[@]}" "$@" \
      > "$run/$label-console.log" 2>&1 || status=$?
  if [[ -n "${stack_case:-}" ]]; then
    python3 -B "$repo/tools/check_painter_stack_diagnostic.py" \
      --log "$build/meson-logs/$label-gdb-stack-wrapper.sh.json" --selected-case "$stack_case" \
      --output "$out/$label-results.json" || gate=$?
    # Only the allowlisted report is public. The raw console/Meson environment,
    # private profile, and any process state remain outside the artifact root.
    (( gate == 0 ))
    return
  fi
  if [[ -n "${dockbook_prefix:-}" ]]; then
    if [[ "${dockbook_controlled_order:-0}" == 1 ]]; then
      python3 -B "$repo/tools/check_painter_controlled_order_diagnostic.py" \
        --log "$build/meson-logs/$label-gdb-dockbook-wrapper.sh.json" \
        --selected-prefix "$dockbook_prefix" --trace "$out/dockbook-controlled-order-trace.txt" \
        --output "$out/$label-results.json" || gate=$?
    else
      python3 -B "$repo/tools/check_painter_dockbook_diagnostic.py" \
        --log "$build/meson-logs/$label-gdb-dockbook-wrapper.sh.json" \
        --selected-prefix "$dockbook_prefix" --output "$out/$label-results.json" || gate=$?
    fi
    (( gate == 0 ))
    return
  fi
  local checks=(--log "$build/meson-logs/$label.json" --expect "$@"
                --output "$out/$label-results.json")
  if [[ "$capture" == yes ]]; then
    checks+=(--case-manifest "$repo/tools/painter-common-layer-popup-cases.json"
              --screenshots "$out/$label" --scale "$scale")
  fi
  if [[ "$#" == 1 && "$1" == painter-overlay-paint-gate ]]; then
    checks+=(--case-manifest "$repo/tools/painter-overlay-paint-gate-cases.json")
  fi
  if [[ -n "${red_case:-}" ]]; then checks+=(--baseline-red-case "$red_case"); fi
  if [[ -n "${diagnostic_prefix:-}" ]]; then
    checks+=(--diagnostic-prefix "$diagnostic_prefix"
              --case-manifest "$repo/tools/painter-common-layer-popup-cases.json")
  fi
  python3 -B "$repo/tools/check_painter_ui_ci.py" "${checks[@]}" || gate=$?
  # Meson console output has no inherited-environment block (unlike raw .txt
  # logs), but remove its command/env line and known environment headings.
  python3 - "$run/$label-console.log" "$out/$label-console.log" <<'PY'
import pathlib, sys
lines = pathlib.Path(sys.argv[1]).read_text(errors='replace').splitlines()
safe = [line for line in lines if not line.lstrip().startswith(('>>>', 'Inherited environment:'))]
pathlib.Path(sys.argv[2]).write_text('\n'.join(safe) + '\n')
PY
  if [[ -n "${red_case:-}" ]]; then
    (( status != 0 && gate == 0 ))
  else
    (( status == 0 && gate == 0 ))
  fi
}

# Bounded investigation after an original common-suite failure, not retries of
# acceptance. Select only the first failing scale, at most four prefixes and
# 90 seconds per route (the committed common registration timeout is 180s).
# Each invocation keeps test_group's fresh profile/display. No acceptance PNG
# environment is set, and all reports/captures live in a separate directory.
route_diagnostics() {
  local scale=$1 route diagnostic_prefix
  local out="$out/route-diagnostics"
  mkdir -p "$out"
  for route in ordinary-tile clone-tile ordinary-tree clone-tree; do
    diagnostic_prefix="/common-layer-native-popup/$route"
    test_group "route-$route-scale$scale" "$scale" no painter-common-layer-popup || true
  done
}

if [[ "$mode" == pre-fix-stack-978 ]]; then
  stage=historical-stack-diagnostic
  # No initialization files, auto-loaded printers, downloaded symbols, locals,
  # environment dumps, core files, or OS/ptrace/security-setting changes.
  cat > "$run/gdb-stack-wrapper.sh" <<'GDBWRAPPER'
#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
exec /usr/bin/gdb --nx --batch --quiet --return-child-result \
  -iex 'set auto-load off' \
  -ex 'set debuginfod enabled off' \
  -ex 'set pagination off' -ex 'set confirm off' \
  -ex 'set disable-randomization off' \
  -ex 'set print frame-arguments none' -ex 'set print entry-values no' \
  -ex run -ex 'thread apply all bt -no-filters 40' --args "$@"
GDBWRAPPER
  chmod 700 "$run/gdb-stack-wrapper.sh"
  # One isolated case, 90s (180s registration x 0.5), no screenshot acceptance.
  # This is a hypothesis-selected reproducer, not evidence of the old crash site.
  stack_case=/common-layer-native-popup/ordinary-tile/staged-child-1
  test_group pre-fix-staged-child-1-scale2 2 no painter-common-layer-popup || failed=1
  stage=source-integrity
  verify_source || failed=1
  printf 'Historical diagnostic only: never candidate acceptance; see stack report.\n' \
    | tee "$out/diagnostic-scope.txt"
  exit "$failed"
fi

if [[ "$mode" == dockbook-stack-3d6f || "$mode" == dockbook-controlled-order-3d6f ]]; then
  stage=dockbook-stack-diagnostic
  # A separate X11 client, never inferior execution or a fatal/log handler.
  # GDB runs it only after `run` returns. A capture can also be after clean exit;
  # only the report's actual signal/stack evidence identifies a genuine stop.
  cc -std=c11 -Wall -Wextra -Werror "$repo/tools/painter-ci-root-capture.c" \
    -o "$run/dockbook-root-capture" $(pkg-config --cflags --libs gdk-3.0 gdk-pixbuf-2.0)
  export PAINTER_DOCK_CAPTURE_TOOL="$run/dockbook-root-capture"
  export PAINTER_DOCK_CAPTURE_PATH="$out/dockbook-after-gdb-run.png"
  if [[ "$mode" == dockbook-controlled-order-3d6f ]]; then
    stage=dockbook-controlled-order-diagnostic
    dockbook_controlled_order=1
    export PAINTER_DOCK_CAPTURE_PATH="$out/dockbook-controlled-order-after-gdb-run.png"
  fi
  cat > "$run/gdb-dockbook-wrapper.sh" <<'GDBDOCKWRAPPER'
#!/usr/bin/env bash
set -euo pipefail
ulimit -c 0
exec /usr/bin/gdb --nx --batch --quiet --return-child-result \
  -iex 'set auto-load off' \
  -ex 'set debuginfod enabled off' \
  -ex 'set pagination off' -ex 'set confirm off' \
  -ex 'set disable-randomization off' \
  -ex 'set print frame-arguments none' -ex 'set print entry-values no' \
  -ex run \
  -ex 'shell timeout --kill-after=1s 5s "$PAINTER_DOCK_CAPTURE_TOOL" "$PAINTER_DOCK_CAPTURE_PATH"' \
  -ex 'thread apply all bt -no-filters 40' \
  -ex 'echo \n=== DOCKBOOK MAIN THREAD BACKTRACE (MAX 200 FRAMES) ===\n' \
  -ex 'thread 1' -ex 'bt -no-filters 200' --args "$@"
GDBDOCKWRAPPER
  chmod 700 "$run/gdb-dockbook-wrapper.sh"
  dockbook_prefix=/common-layer-native-popup/clone-tree
  if [[ "${dockbook_controlled_order:-0}" == 1 ]]; then
    test_group dockbook-controlled-order-clone-tree-scale1 1 no painter-common-layer-popup || failed=1
  else
    test_group dockbook-clone-tree-scale1 1 no painter-common-layer-popup || failed=1
  fi
  stage=source-integrity
  verify_source || failed=1
  printf 'Pinned dockbook diagnostic mode=%s; never acceptance. Optional root PNG is after GDB run returns, not an accepted popup image. No warning/returned active dispatch is inconclusive.\n' "$dockbook_diagnostic_mode" \
    | tee "$out/diagnostic-scope.txt"
  exit "$failed"
fi

if [[ "$mode" == baseline-carrier-red ]]; then
  stage=baseline-carrier-red
  for route in ordinary-tile clone-tile ordinary-tree clone-tree; do
    red_case="/common-layer-native-popup/$route/actual-entry"
    test_group "baseline-$route" 1 no painter-common-layer-popup || failed=1
  done
  stage=source-integrity
  verify_source || failed=1
  if (( failed )); then exit 1; fi
  printf 'Four intended baseline carrier assertion failures observed; these are RED controls, not UI passes.\n' \
    | tee "$out/acceptance.txt"
  exit 0
fi
stage=overlay-paint-gate-scale1
test_group overlay-paint-gate-scale1 1 no painter-overlay-paint-gate || failed=1
stage=overlay-paint-gate-scale2
test_group overlay-paint-gate-scale2 2 no painter-overlay-paint-gate || failed=1
stage=common-layer-scale1
failed_scales=()
test_group common-layer-scale1 1 yes painter-common-layer-popup || { failed=1; failed_scales+=(1); }
stage=common-layer-scale2
test_group common-layer-scale2 2 yes painter-common-layer-popup || { failed=1; failed_scales+=(2); }
stage=adjacent-regressions
test_group adjacent-regressions 1 no "${regressions[@]}" || failed=1
if (( ${#failed_scales[@]} )); then
  stage=route-diagnostics
  route_diagnostics "${failed_scales[0]}"
fi
stage=source-integrity
verify_source || failed=1
locale_state verify || failed=1
stage=automated-gate
if (( failed )); then exit 1; fi
printf 'Automated focused checks passed. All 16 PNGs still require actual image inspection.\n' \
  | tee "$out/acceptance.txt"
