#!/usr/bin/env bash
# DRAFT runtime stage only: call after a fresh normal Meson configure/build.
# Never run this in the restricted dot executor. Ordinary authorized Actions
# only; no TCP display, namespace/socket workaround, WM replacement or desktop.
set -euo pipefail
draft=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
src=$(realpath "${1:?source required}")
build=$(realpath "${2:?build required}")
out=$(realpath -m "${3:?artifact directory required}")
run=$(realpath -m "${4:?private scratch required}")
mode=${5:?positive, baseline-red or sanitizer required}
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary authorized GitHub Actions only' >&2; exit 2; }
case "$mode" in positive|baseline-red|sanitizer) ;; *) exit 2;; esac
test -f "$build/build.ninja"
mkdir -p "$out" "$run"
python3 -B - "$draft" "$src" "$mode" "${BRUSH_CI_RUNTIME_PINS:?automatic runtime seal required}" <<'PY'
import hashlib,importlib.util,json,pathlib,subprocess,sys
draft, source, mode, runtime_pins = sys.argv[1:]
spec=importlib.util.spec_from_file_location('overlay',pathlib.Path(draft)/'prepare_brush_baseline.py')
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
pins=json.loads(pathlib.Path(runtime_pins).read_text())
base,candidate=m.commits(pins)
actual=subprocess.check_output(['git','-C',source,'rev-parse','HEAD']).decode().strip()
assert actual == (base if mode == 'baseline-red' else candidate), 'Wrong source HEAD'
checks = [('app/tests/test-painter-brush-thumbnail-popup.inc', 'test_include_sha256'),
          ('app/tests/test-painter-compact-options.c', 'candidate_main_sha256')]
if mode != 'baseline-red':
    checks.append(('migration/tests/brush-thumbnail-native-popup/cases.json', 'case_manifest_sha256'))
for relative, field in checks:
    path = pathlib.Path(source) / relative
    assert path.is_file() and not path.is_symlink(), 'Missing/symlink pinned source: ' + relative
    assert hashlib.sha256(path.read_bytes()).hexdigest() == pins[field], 'Source pin mismatch: ' + relative
PY
if [[ "$mode" == sanitizer ]]; then
  python3 -B - "$build" "$out/sanitizer-build-options.json" <<'PY'
import json,pathlib,subprocess,sys
build,output=sys.argv[1:]
options=json.loads(subprocess.check_output(['meson','introspect','--buildoptions',build]))
selected=[x for x in options if x.get('name') == 'b_sanitize']
assert len(selected)==1, 'Missing sanitizer build option'
value=selected[0]['value']
names=set(value if isinstance(value,list) else value.split(','))
assert {'address','undefined'} <= names, 'Not an ASan+UBSan build'
pathlib.Path(output).write_text(json.dumps(selected,indent=2)+'\n')
PY
  # Preserve strict leak failure while making child executable/caller frames
  # observable across external libraries that lack frame pointers. No suppression.
  unset LSAN_OPTIONS
  export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
  python3 -B - "$out/sanitizer-runtime-options.json" <<'PYOPTIONS'
import json,os,pathlib,sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({
    'format': 1, 'ASAN_OPTIONS': os.environ['ASAN_OPTIONS'],
    'UBSAN_OPTIONS': os.environ['UBSAN_OPTIONS'],
    'LSAN_OPTIONS_unset': 'LSAN_OPTIONS' not in os.environ,
    'raw_environment_exported': False,
    'scope': 'Explicit sanitizer settings only; no suppression or success-exit override'
}, indent=2)+'\n')
PYOPTIONS
fi
export GDK_BACKEND=x11 PYTHONDONTWRITEBYTECODE=1
unset GIMP_TEST_PROTOCOL_FIXTURE GIMP_TEST_PROTOCOL_SCENARIO GIMP_TEST_PROTOCOL_LOG_DIR GIMP_TEST_EXPECT_REDUNDANT_QUIT
unset GSETTINGS_BACKEND GIMP3_DIRECTORY GIMP3_DATADIR NO_AT_BRIDGE
unset GIMP_TEST_LAYER_UI_FILTER GIMP_TEST_COLORIFY_ONCANVAS
unset GIMP_TEST_TOOLBAR_PAINTER GIMP_TEST_TOOLBAR_SINGLE GIMP_TEST_TOOLBAR_SESSION_DIRECTORY GIMP_TEST_TOOLBAR_SESSION_PASS
unset GIMP_TEST_COMMON_POPUP_SCREENSHOTS GIMP_TEST_COMMON_POPUP_DIAGNOSTICS
unset GIMP_TEST_COMMON_POPUP_DOCK_TRACE GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES
unset GIMP_TEST_COMMON_LAYER_POPUP GIMP_TEST_FILTER_LAYER_POPUP GIMP_TEST_OVERLAY_PAINT_GATE
run_group() {
  local label=$1 scale=$2 selector=$3 manifest=$4 capture=${5:-} registration=${6:-painter-compact-options}
  local profile="$run/profile-$label" status=0 gate=0
  mkdir -p "$profile"/{home,config,cache,data,state,runtime,tmp,gimp-temp,gimp-cache}
  chmod 700 "$profile/runtime" "$profile/tmp" "$profile/gimp-temp" "$profile/gimp-cache"
  export HOME="$profile/home" XDG_CONFIG_HOME="$profile/config" XDG_CACHE_HOME="$profile/cache"
  export XDG_DATA_HOME="$profile/data" XDG_STATE_HOME="$profile/state" XDG_RUNTIME_DIR="$profile/runtime"
  export TMPDIR="$profile/tmp" GIMP3_TEMPDIR="$profile/gimp-temp" GIMP3_CACHEDIR="$profile/gimp-cache"
  export GDK_SCALE="$scale" GDK_DPI_SCALE=1
  # Preserve the prior per-suite fixture contract: native on-canvas for popup
  # suites; original registered fixtures for compact and toolbar-window. Toolbar
  # cases intentionally exercise both native/painter and single/multiwindow.
  if [[ "$selector" == /painter-compact-options || "$registration" == painter-toolbar-window || "$registration" == plug-in-protocol || "$registration" == choice-lifecycle || "$registration" == painter-brush-geometry || "$registration" == color-config-ownership || "$registration" == display-renderer-ownership ]]; then
    unset GIMP_TESTING_SESSIONRC_NAME
  else
    export GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas
  fi
  unset GIMP_BRUSH_POPUP_CAPTURE GIMP_BRUSH_POPUP_BOTTOM_CAPTURE
  if [[ -n "$capture" ]]; then
    if [[ "$selector" == /brush-thumbnail-native-popup/lower-controls-actual-input ]]; then
      export GIMP_BRUSH_POPUP_BOTTOM_CAPTURE="$capture"
    else
      export GIMP_BRUSH_POPUP_CAPTURE="$capture"
    fi
  fi
  # Existing compact registration has 180 seconds. Explicit 3x budget is
  # diagnostic time allowance, not an assertion/skip/failure exemption.
  local selection=()
  if [[ -n "$selector" ]]; then selection+=(--test-args "-p $selector"); fi
  if [[ "$registration" == plug-in-protocol || "$registration" == choice-lifecycle || "$registration" == painter-brush-geometry || "$registration" == color-config-ownership || "$registration" == display-renderer-ownership ]]; then
    # Supplementary lifecycle gates, outside the153 UI cases: protocol uses
    # real pipes; choice is library-only; geometry initializes the core model.
    # No display or D-Bus; each existing Meson registration keeps its timeout.
    # Meson owns the protocol fixture path, never inherited fault controls.
    local child_logs=""
    if [[ "$registration" == plug-in-protocol ]]; then
      child_logs="$run/protocol-$mode-child-logs"
      mkdir -m 700 "$child_logs" || return 1
    fi
    ( unset DISPLAY WAYLAND_DISPLAY
      if [[ "$registration" == plug-in-protocol ]]; then
        export GIMP_TEST_PROTOCOL_LOG_DIR="$child_logs"
      fi
      meson test -C "$build" --no-rebuild --num-processes 1 \
        --print-errorlogs --logbase "$label" "${selection[@]}" "$registration"
    ) > "$run/$label-console.log" 2>&1 || status=$?
    # Existing host saves stderr before its strict abort. Collect only bounded
    # sanitized known scenario stderr; never publish wire/stdout/profile files.
    if [[ "$registration" == plug-in-protocol ]]; then
      python3 -B "$draft/capture_protocol_stderr.py" --runner-temp "$RUNNER_TEMP" \
        --runtime-dir "$run" --artifact-dir "$out" --mode "$mode" || gate=1
    fi
  else
    xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
      dbus-run-session -- meson test -C "$build" --no-rebuild --num-processes 1 \
        --timeout-multiplier 3 --print-errorlogs --logbase "$label" \
        "${selection[@]}" "$registration" \
        > "$run/$label-console.log" 2>&1 || status=$?
  fi
  local args=(--log "$build/meson-logs/$label.json" --output "$out/$label-results.json")
  if [[ "$mode" == baseline-red ]]; then
    args+=(--baseline-red)
  else
    args+=(--common-checker "$src/tools/check_painter_ui_ci.py" --case-manifest "$manifest")
    if [[ -n "$capture" ]]; then args+=(--screenshot "$capture" --scale "$scale"); fi
  fi
  python3 -B "$draft/check_brush_ci.py" "${args[@]}" || gate=$?
  # Never publish raw Meson logs, profiles, .Xauthority, D-Bus addresses or env.
  python3 -B - "$run/$label-console.log" "$out/$label-console.log" <<'PY'
import pathlib,re,sys
lines=pathlib.Path(sys.argv[1]).read_text(errors='replace').splitlines()
safe=[x for x in lines if not re.match(r'^\s*(?:>>>|Inherited environment:|Environment:|[A-Za-z_][A-Za-z_0-9]*=)',x)]
pathlib.Path(sys.argv[2]).write_text('\n'.join(safe)+'\n')
PY
  if [[ "$mode" == baseline-red ]]; then ((status != 0 && gate == 0));
  else ((status == 0 && gate == 0)); fi
}

failed=0
if [[ "$mode" != baseline-red ]]; then
  run_group "protocol-$mode" 1 /plug-in-protocol "$draft/protocol-cases.json" '' plug-in-protocol || failed=1
  run_group "choice-$mode" 1 /choice-lifecycle "$draft/choice-cases.json" '' choice-lifecycle || failed=1
  run_group "geometry-$mode" 1 /painter/geometry "$draft/geometry-cases.json" '' painter-brush-geometry || failed=1
  run_group "color-config-$mode" 1 /ownership/config "$draft/color-config-cases.json" '' color-config-ownership || failed=1
  # GLib -p /ownership omits nested leaves; run this dedicated two-case binary whole.
  run_group "display-renderer-$mode" 1 '' "$draft/display-renderer-cases.json" '' display-renderer-ownership || failed=1
  # Canvas requires GTK: keep it on the portrait Xvfb branch, whole binary.
  run_group "canvas-$mode" 1 '' "$draft/canvas-cases.json" '' canvas-ownership || failed=1
  # Real GTK frame regression: all seven cases, no parent filter or skip waiver.
  run_group "frame-style-$mode" 1 '' "$draft/frame-style-cases.json" '' gimp-frame-style-idempotence || failed=1
fi
if [[ "$mode" == baseline-red ]]; then
  run_group brush-baseline-red 1 /brush-thumbnail-native-popup/actual-entry '' || failed=1
  python3 -B "$draft/prepare_brush_baseline.py" --baseline "$src" \
    --verify "$out/baseline-overlay.json" || failed=1
elif [[ "$mode" == sanitizer ]]; then
  # C5 measured nine cases consuming486.44s of the540s whole-registration cap.
  # Partition sanitizer workload only:21 fresh profiles/processes, one exact
  # leaf each, unchanged540s ceiling and strict parent/child leak checks. This
  # is not one full21 sanitizer process; normal full21 at both scales remains.
  # Continue independent cases after a failure; aggregate requires all21.
  plan="$run/sanitizer-plan.tsv"
  if python3 -B "$draft/sanitizer_cases.py" --bundle "$draft" --plan > "$plan"; then
    while IFS=$'\t' read -r label manifest selector; do
      run_group "$label" 1 "$selector" "$draft/$manifest" || failed=1
    done < "$plan"
  else
    failed=1
  fi
  python3 -B "$draft/sanitizer_cases.py" --bundle "$draft" \
    --logs "$build/meson-logs" --reports "$out" --common-checker "$src/tools/check_painter_ui_ci.py" \
    --output "$out/brush-sanitizer-results.json" || failed=1
  # The caller must retain the exact sanitizer compile/link command manifest
  # and instrumented scope. This script cannot turn a normal build into ASan.
else
  for scale in 1 2; do
    run_group "brush-scale$scale" "$scale" /brush-thumbnail-native-popup "$draft/brush-cases.json" || failed=1
    run_group "brush-actual-entry-scale$scale" "$scale" /brush-thumbnail-native-popup/actual-entry \
      "$draft/actual-entry-cases.json" "$out/brush-thumbnail-actual-entry-scale$scale.png" || failed=1
    run_group "brush-lower-controls-scale$scale" "$scale" /brush-thumbnail-native-popup/lower-controls-actual-input \
      "$draft/lower-control-case.json" "$out/brush-thumbnail-lower-controls-scale$scale.png" || failed=1
  done
  while IFS=$'\t' read -r label registration selector; do
    if [[ "$selector" == NONE ]]; then selector=''; fi
    run_group "previous-$label" 1 "$selector" "$draft/group-manifests/$label.json" '' "$registration" || failed=1
  done < <(python3 -B - "$draft/positive-groups.json" <<'PY'
import json,sys
for group in json.load(open(sys.argv[1]))['groups']:
    print(group['label']+'\t'+group['registration']+'\t'+(group['selector'] or 'NONE'))
PY
  )
fi
# BEGIN fresh-process causality diagnostics (not acceptance substitutes).
# Each run_group owns a different profile and process. Continue after failure;
# strict common checks retain complete test stdout/stderr and reject exit/leaks.
if [[ "$mode" != baseline-red ]]; then
  if [[ "$mode" == positive ]]; then
    run_group diagnostic-private-target-normal 1 /brush-thumbnail-native-popup/private-target-release \
      "$draft/diagnostic-private-target-normal.json" || failed=1
  fi
  run_group "diagnostic-harness-only-$mode" 1 /brush-exit-diagnostic/harness-only \
    "$draft/diagnostic-harness-only.json" || failed=1
  run_group "diagnostic-fixture-no-popup-$mode" 1 /brush-exit-diagnostic/fixture-no-popup \
    "$draft/diagnostic-fixture-no-popup.json" || failed=1
fi
# END fresh-process causality diagnostics.
if [[ "$mode" != baseline-red ]]; then
  git -C "$src" diff --exit-code || failed=1
  test -z "$(git -C "$src" ls-files --others --exclude-standard)" || failed=1
fi
if (( failed )); then exit 1; fi
printf 'Brush %s automated gate passed; image review and full acceptance remain separate.\n' "$mode"
