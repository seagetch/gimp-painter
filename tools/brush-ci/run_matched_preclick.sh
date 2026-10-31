#!/usr/bin/env bash
# Separate nonacceptance matched four-build comparison; no acceptance mutations.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
phase=${1:?prepare, measure or one}; shift
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary hosted diagnostic only' >&2; exit 2; }
: "${BRUSH_CI_RUNTIME_PINS:?automatic source seal required}"
: "${PAINTER_CI_SOURCE:?owned controlling checkout required}"
[[ -z ${LD_PRELOAD:-} && -z ${LD_AUDIT:-} ]] || { echo 'BLOCKED: injected loader policy' >&2; exit 2; }
case "$phase" in
  prepare) out=$(realpath "${1:?output required}"); run=$(mktemp -d "${RUNNER_TEMP:?}/painter-matched-preclick.XXXXXXXX");;
  measure|one) run=$(realpath "${1:?private run required}"); out=$(realpath "${2:?output required}");;
  *) exit 2;;
esac
stage=preflight; order=0; status_order=0
if [[ "$phase" == one ]]; then cell=${3:?}; rep=${4:?}; order=${5:?}; kind=${6:?}; status_order=$order; fi
finish() {
  result=$?; trap - EXIT
  python3 -B "$here/matched_driver_state.py" status --out "$out" --phase "$phase" --stage "$stage" --code "$result" --order "$status_order" || { if [[ "$result" == 0 ]]; then result=1; fi; }
  exit "$result"
}
trap finish EXIT
python3 -B "$here/matched_driver_state.py" guard --run "$run" --source "$PAINTER_CI_SOURCE" --out "$out"

cell_env() {
  export GIMP_DEPS_DIRECTORY="$run/deps" GIMP_BUILD_PREFIX="$run/cells/$cell/prefix"
  source "$src/tools/linux-debian13-env.sh"
  export CCACHE_DISABLE=1 PYTHONDONTWRITEBYTECODE=1 GDK_BACKEND=x11 LANG=C.UTF-8 LC_ALL=C.UTF-8
  unset GSETTINGS_BACKEND GIMP3_DIRECTORY GIMP3_DATADIR GIMP3_LOCALEDIR NO_AT_BRIDGE
  unset GIMP_TEST_PROTOCOL_FIXTURE GIMP_TEST_PROTOCOL_SCENARIO GIMP_TEST_PROTOCOL_LOG_DIR GIMP_TEST_EXPECT_REDUNDANT_QUIT
  unset GIMP_TEST_LAYER_UI_FILTER GIMP_TEST_COLORIFY_ONCANVAS
  unset GIMP_TEST_TOOLBAR_PAINTER GIMP_TEST_TOOLBAR_SINGLE GIMP_TEST_TOOLBAR_SESSION_DIRECTORY GIMP_TEST_TOOLBAR_SESSION_PASS
  unset GIMP_TEST_COMMON_POPUP_SCREENSHOTS GIMP_TEST_COMMON_POPUP_DIAGNOSTICS
  unset GIMP_TEST_COMMON_POPUP_DOCK_TRACE GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES
  unset GIMP_TEST_COMMON_LAYER_POPUP GIMP_TEST_FILTER_LAYER_POPUP GIMP_TEST_OVERLAY_PAINT_GATE
  unset GIMP_BRUSH_POPUP_CAPTURE GIMP_BRUSH_POPUP_BOTTOM_CAPTURE
  unset GIMP_TESTING_PLUGINDIRS_BASENAME_IGNORES GIMP_TESTING_DOCKRC_NAME
  unset ASAN_OPTIONS UBSAN_OPTIONS LSAN_OPTIONS
}

if [[ "$phase" == prepare ]]; then
  available=$(df -Pk "$run" | awk 'NR == 2 {print $4}')
  (( available >= 40 * 1024 * 1024 )) || { echo 'Matched diagnostic requires40GiB free; no reduced/mixed-build fallback' >&2; exit 1; }
  mkdir -p "$run"/{cells,profiles,retained-output,logs} "$out"/{identities,reports,runtime-inputs}
  stage=source
  for commit in e25a94e21efca6e6604ddac6f15570c049f421e7 b671825309326bdb55571652e43ab78cc83fd92d; do
    timeout --kill-after=10s 300s git -C "$PAINTER_CI_SOURCE" fetch --no-tags --depth=1 origin "$commit" > "$run/logs/fetch-$commit.log" 2>&1
  done
  for cell in baseline-normal current-normal baseline-sanitizer current-sanitizer; do
    mkdir "$run/cells/$cell"; src="$run/cells/$cell/source"
    commit=e25a94e21efca6e6604ddac6f15570c049f421e7
    if [[ "$cell" == current-* ]]; then commit=b671825309326bdb55571652e43ab78cc83fd92d; fi
    git -C "$PAINTER_CI_SOURCE" worktree add --detach "$src" "$commit" > "$run/logs/$cell-source.log" 2>&1
    timeout --kill-after=10s 600s git -C "$src" -c submodule.gimp-data.url=https://github.com/GNOME/gimp-data.git submodule update --init --depth=1 gimp-data >> "$run/logs/$cell-source.log" 2>&1
    test "$(git -C "$src/gimp-data" rev-parse HEAD)" = a31e5dea4f572fb5f33b06f5c4b86db5f30643dc
    cp "$here/matched-preclick/test-matched-preclick.c" "$here/matched-preclick/matched-preclick-phases.inc" "$src/app/tests/"
    cat "$here/matched-preclick/meson.append" >> "$src/app/tests/meson.build"
    python3 -B "$here/apply_matched_compatibility.py" --source "$src" --cell "$cell" --inputs "$here/matched-preclick"
    python3 -B "$here/matched_provenance.py" overlay --source "$src" --inputs "$here/matched-preclick" --output "$out/identities/$cell-overlay.json"
  done
  stage=dependencies
  export GIMP_DEPS_DIRECTORY="$run/deps"
  python3 -B "$run/cells/baseline-normal/source/tools/prepare-linux-build-deps.py" --directory "$run/deps" > "$run/logs/dependencies.log" 2>&1
  stage=four-builds
  for cell in baseline-normal current-normal baseline-sanitizer current-sanitizer; do
    (
      # Per-cell checkpoints survive an outer timeout; EXIT preserves the exact
      # build-cell shell status (not necessarily the underlying tool status).
      # Only bounded classified diagnostics are exported.
      cell_stage=environment
      build_checkpoint() {
        python3 -B "$here/collect_matched_build_diagnostic.py" --run "$run" --out "$out" \
          --cell "$cell" --stage "$cell_stage" --state running
      }
      build_finish() {
        code=$?; trap - EXIT
        python3 -B "$here/collect_matched_build_diagnostic.py" --run "$run" --out "$out" \
          --cell "$cell" --stage "$cell_stage" --state finished --exit-code "$code" || { if [[ "$code" == 0 ]]; then code=1; fi; }
        exit "$code"
      }
      trap build_finish EXIT
      build_checkpoint
      src="$run/cells/$cell/source"; build="$run/cells/$cell/build"
      cell_env > "$run/logs/$cell-environment.log" 2>&1
      cell_stage=configure; build_checkpoint
      sanitizer=(); if [[ "$cell" == *-sanitizer ]]; then sanitizer+=(-Db_sanitize=address,undefined); fi
      meson setup "$build" "$src" --prefix="$GIMP_BUILD_PREFIX" \
        --buildtype=debugoptimized -Dauto_features=disabled -Dlibunwind=false \
        -Dheadless-tests=disabled -Dpainter-http=disabled -Denable-console-bin=false \
        -Dgi-docgen=disabled -Dappdata-test=disabled -Djavascript=disabled -Dvala=disabled \
        -Dprint=false -Dcheck-update=no -Dlibbacktrace=false "${sanitizer[@]}" > "$run/logs/$cell-configure.log" 2>&1
      cell_stage=target-selection; build_checkpoint
      mapfile -t targets < <(python3 -B - "$here/matched-preclick/policy.json" "$cell" 2> "$run/logs/$cell-target-selection.log" <<'PY'
import json,sys
p=json.load(open(sys.argv[1]));t=p['common_targets'][:]
if sys.argv[2].startswith('current-'):t.append(p['current_fidelity_target'])
print('\n'.join(t))
PY
)
      expected=13; if [[ "$cell" == current-* ]]; then expected=14; fi
      test "${#targets[@]}" = "$expected"
      cell_stage=compile; build_checkpoint
      meson compile -C "$build" -j 2 "${targets[@]}" > "$run/logs/$cell-compile.log" 2>&1
      cell_stage=catalog-stage; build_checkpoint
      python3 -B - "$src/tools/run-painter-frozen-aggregate.py" "$build" "$run/cells/$cell/locale" > "$run/logs/$cell-catalog-stage.log" 2>&1 <<'PY'
import hashlib,json,pathlib,runpy,sys
helper,build,dest=sys.argv[1:]
expected='4cdc8a36be383487c4cca06fc173f8d3c2b949370318b66fd15f22548eecfada'
p=pathlib.Path(helper)
if p.is_symlink() or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=expected:
    raise SystemExit('Catalog staging helper identity rejected')
f=runpy.run_path(helper);j=f['stage_build_locales'](build,dest)
if p.is_symlink() or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=expected or not isinstance(j,dict) or 'helper_sha256' in j:
    raise SystemExit('Catalog staging helper or result changed')
j['helper_sha256']=expected
pathlib.Path(dest+'.json').write_text(json.dumps(j,indent=2)+'\n')
PY
      cell_stage=build-identity; build_checkpoint
      python3 -B "$here/matched_provenance.py" build-identity --source "$src" --build "$build" --cell "$cell" --deps "$run/deps" --output "$out/identities/$cell.json" > "$run/logs/$cell-build-identity.log" 2>&1
      cell_stage=overlay-check; build_checkpoint
      python3 -B "$here/matched_provenance.py" overlay --source "$src" --inputs "$here/matched-preclick" --output "$out/identities/$cell-overlay-after-build.json" > "$run/logs/$cell-overlay-check.log" 2>&1
      cell_stage=complete; build_checkpoint
    )
  done
  stage=ready
  python3 -B "$here/matched_driver_state.py" ready --run "$run" --source "$PAINTER_CI_SOURCE" --out "$out"
  printf 'MATCHED_PRECLICK_RUN=%s\n' "$run" >> "${GITHUB_ENV:?}"
  exit 0
fi

python3 -B "$here/matched_driver_state.py" verify-ready --run "$run" --source "$PAINTER_CI_SOURCE" --out "$out"
if [[ "$phase" == one ]]; then
  src="$run/cells/$cell/source"; build="$run/cells/$cell/build"
  stage=fresh-profile
  python3 -B "$here/matched_driver_state.py" profile --run "$run" --cell "$cell" --repetition "$rep" --order "$order" --kind "$kind"
  profile=$(printf '%s/profiles/%02d-%s' "$run" "$order" "$cell")
  cell_env
  export HOME="$profile/home" XDG_CONFIG_HOME="$profile/config" XDG_CACHE_HOME="$profile/cache"
  export XDG_DATA_HOME="$profile/data" XDG_STATE_HOME="$profile/state" XDG_RUNTIME_DIR="$profile/runtime"
  export TMPDIR="$profile/tmp" GIMP3_TEMPDIR="$profile/gimp-temp" GIMP3_CACHEDIR="$profile/gimp-cache"
  export GDK_SCALE=1 GDK_DPI_SCALE=1 GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas
  export GIMP3_LOCALEDIR="$run/cells/$cell/locale"
  if [[ "$cell" == *-sanitizer ]]; then
    export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1
    export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
  fi
  python3 -B "$here/matched_provenance.py" runtime-preflight --build "$build" --cell "$cell" --order "$order" --output "$profile/preflight.json"
  label=$(printf 'matched-%02d' "$order"); registration=matched-preclick; leaf=/matched-preclick/primary
  if [[ "$kind" == fidelity ]]; then registration=painter-compact-options; leaf=/brush-thumbnail-native-popup/private-target-release; fi
  stage=native-measurement; native=0
  (unset DISPLAY WAYLAND_DISPLAY
   xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
     dbus-run-session -- meson test -C "$build" --no-rebuild --num-processes 1 \
       --print-errorlogs --logbase "$label" --wrapper "python3 $here/matched_preclick_exec.py" \
       --test-args "-p $leaf" "$registration"
  ) > "$run/logs/$label-console.log" 2>&1 || native=$?
  # Meson 1.7.0 appends the exact first wrapper executable basename to logbase.
  # The wrapper above is literally python3; never glob or accept another log.
  stage=bounded-collection; collection=0; archived=0; postflight=0; log_metadata=0
  python3 -B "$here/matched_driver_state.py" log-evidence --run "$run" --cell "$cell" --order "$order" --out "$out" || log_metadata=$?
  python3 -B "$here/matched_provenance.py" runtime-postflight --build "$build" --cell "$cell" --order "$order" \
    --log "$build/meson-logs/$label-python3.json" --output "$profile/postflight.json" || postflight=$?
  python3 -B "$here/collect_matched_preclick.py" collect --log "$build/meson-logs/$label-python3.json" \
    --cell "$cell" --repetition "$rep" --order "$order" --kind "$kind" --output "$out/reports/$label.json" || collection=$?
  python3 -B "$here/matched_driver_state.py" archive --run "$run" --cell "$cell" --order "$order" || archived=$?
  # Collector success can qualify late-emitted leaks as timing data only.
  # It NEVER resets the actual failed native Meson exit.
  (( native == 0 && collection == 0 && archived == 0 && postflight == 0 && log_metadata == 0 ))
  exit
fi

stage=fourteen-native-runs; failed=0
while IFS=$'\t' read -r cell rep order kind; do
  bash "$here/run_matched_preclick.sh" one "$run" "$out" "$cell" "$rep" "$order" "$kind" || failed=1
done < <(python3 -B "$here/matched_driver_state.py" order)
# Resolution/inventory tools run only AFTER all timing invocations; no metadata
# fontconfig inspection between samples can warm another sample's inputs.
stage=runtime-comparability
while IFS=$'\t' read -r cell rep order kind; do
  (
    src="$run/cells/$cell/source"; build="$run/cells/$cell/build"; cell_env
    profile=$(printf '%s/profiles/%02d-%s' "$run" "$order" "$cell"); label=$(printf 'matched-%02d' "$order")
    python3 -B "$here/matched_provenance.py" runtime-inputs --source "$src" --build "$build" \
      --cell "$cell" --deps "$run/deps" --profile "$profile" --catalogs "$run/cells/$cell/locale" \
      --log "$build/meson-logs/$label-python3.json" --order "$order" --preflight "$profile/preflight.json" \
      --identity "$out/identities/$cell.json" --wrapper "$here/matched_preclick_exec.py" \
      --cache-directory "$profile/native-output" --postflight "$profile/postflight.json" --output "$out/runtime-inputs/$label.json"
  ) || failed=1
done < <(python3 -B "$here/matched_driver_state.py" order)
stage=aggregate
python3 -B "$here/matched_provenance.py" compare \
  --identities "$out/identities/baseline-normal.json" "$out/identities/current-normal.json" "$out/identities/baseline-sanitizer.json" "$out/identities/current-sanitizer.json" \
  --runtime-inputs "$out"/runtime-inputs/matched-*.json --output "$out/comparability.json" || failed=1
python3 -B "$here/collect_matched_preclick.py" aggregate --reports "$out/reports" --identities "$out/comparability.json" --output "$out/summary.json" || failed=1
for cell in baseline-normal current-normal baseline-sanitizer current-sanitizer; do
  python3 -B "$here/matched_provenance.py" overlay --source "$run/cells/$cell/source" --inputs "$here/matched-preclick" --output "$out/identities/$cell-overlay-after-runtime.json" || failed=1
done
stage=complete
exit "$failed"
