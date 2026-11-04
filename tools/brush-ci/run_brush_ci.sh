#!/usr/bin/env bash
# Additive Brush native CI. Existing common candidate/control driver is unchanged.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo=$(git rev-parse --show-toplevel)
out=$(realpath -m "${1:?artifact directory required}")
mode=${2:?positive, baseline-red or sanitizer required}
pins=$(realpath "${BRUSH_CI_RUNTIME_PINS:?automatic runtime seal required}")
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary Actions only' >&2; exit 2; }
case "$mode" in positive|baseline-red|sanitizer) ;; *) exit 2;; esac
mkdir -p "$out"
run=$(mktemp -d "${RUNNER_TEMP:?}/painter-brush-ci.XXXXXXXX")
src="$run/source"; build="$run/build"; stage=preflight
record_exit() {
  result=$?; trap - EXIT
  printf 'stage=%s\nexit_code=%s\nmode=%s\n' "$stage" "$result" "$mode" > "$out/status.txt"
  if [[ -f "$src/.git" || -d "$src/.git" ]]; then
    git -C "$src" diff --binary > "$out/generated-source.patch" || true
    git -C "$src" status --porcelain=v1 > "$out/source-after.status" || true
  fi
  df -h "$run" > "$out/disk-after.txt" || true
  exit "$result"
}
trap record_exit EXIT
readarray -t commits < <(python3 -B - "$here" "$pins" <<'PY'
import json,sys
sys.path.insert(0,sys.argv[1]);from prepare_brush_baseline import commits
print('\n'.join(commits(json.load(open(sys.argv[2])))))
PY
)
test "${#commits[@]}" = 2
accepted=${commits[0]}; candidate=${commits[1]}
test "$(git -C "$repo" rev-parse HEAD)" = "$candidate"
test "$candidate" = "${GITHUB_SHA:?}"
test -z "$(git -C "$repo" status --porcelain=v1 --untracked-files=all)"
{
  printf 'candidate=%s\naccepted307=%s\nmode=%s\n' "$candidate" "$accepted" "$mode"
  printf 'image_inspection=pending; no automated pixel acceptance\n'
  sha256sum "$pins" "$repo/migration/baseline/debian13-package-lock.json"
  cat /etc/os-release
  gcc --version | head -n 1
  dpkg-query -W ca-certificates git python3 build-essential pkgconf libc6 \
    xvfb xauth dbus-x11 libxtst6 librsvg2-common fontconfig fonts-cantarell \
    glib-networking at-spi2-core libblas3 liblapack3 libgtk-3-common
} > "$out/source-identity.txt"
df -h "$run" > "$out/disk-before.txt"
available=$(df -Pk "$run" | awk 'NR == 2 {print $4}')
(( available >= 8 * 1024 * 1024 )) || { echo 'Need8GiB free; no paid runner or system cleanup fallback' >&2; exit 1; }
stage=source
source_commit=$candidate
if [[ "$mode" == baseline-red ]]; then
  git -C "$repo" fetch --no-tags --depth=1 origin "$accepted"
  source_commit=$accepted
fi
git -C "$repo" worktree add --detach "$src" "$source_commit"
if [[ "$mode" == baseline-red ]]; then
  test "$(git -C "$src" rev-parse HEAD^{tree})" = 8634eeef49c81aaa5c25192cb76d749ff680683f
fi
git -C "$src" -c submodule.gimp-data.url=https://github.com/GNOME/gimp-data.git \
  submodule update --init --depth=1 gimp-data
test "$(git -C "$src/gimp-data" rev-parse HEAD)" = "$(git -C "$src" rev-parse HEAD:gimp-data)"
git -C "$src" submodule status > "$out/submodule-identity.txt"
test -z "$(git -C "$src" status --porcelain=v1 --untracked-files=all)"
if [[ "$mode" == baseline-red ]]; then
  python3 -B "$here/prepare_brush_baseline.py" --baseline "$src" --candidate "$repo" \
    --pins "$pins" --output "$out/baseline-overlay.json"
fi
verify_source() {
  if [[ "$mode" == baseline-red ]]; then
    python3 -B "$here/prepare_brush_baseline.py" --baseline "$src" --verify "$out/baseline-overlay.json"
  else
    git -C "$src" diff --exit-code && test -z "$(git -C "$src" ls-files --others --exclude-standard)"
  fi
}
stage=dependencies
export GIMP_DEPS_DIRECTORY="$run/deps" GIMP_BUILD_PREFIX="$run/prefix"
python3 -B "$src/tools/prepare-linux-build-deps.py" --directory "$GIMP_DEPS_DIRECTORY" \
  2>&1 | tee "$out/dependencies.log"
source "$src/tools/linux-debian13-env.sh"
{ meson --version; ninja --version; pkg-config --modversion gtk+-3.0 glib-2.0 gegl-0.4 babl-0.1 gexiv2 libmypaint; } > "$out/dependency-versions.txt"
export CCACHE_DISABLE=1 PYTHONDONTWRITEBYTECODE=1 GDK_BACKEND=x11
unset GSETTINGS_BACKEND GIMP3_DATADIR GIMP3_DIRECTORY GIMP3_LOCALEDIR
stage=runtime-schemas
gsettings list-schemas | sort > "$out/gsettings-schemas.txt"
for schema in org.gtk.Settings.FileChooser org.gtk.Settings.ColorChooser org.gtk.Settings.EmojiChooser org.gtk.Settings.Debug; do
  grep -Fxq "$schema" "$out/gsettings-schemas.txt" || { echo "Required GTK schema missing: $schema" >&2; exit 1; }
done
stage=configure
sanitize=()
if [[ "$mode" == sanitizer ]]; then sanitize+=(-Db_sanitize=address,undefined); fi
meson setup "$build" "$src" --prefix="$GIMP_BUILD_PREFIX" \
  --buildtype=debugoptimized -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=disabled -Dpainter-http=disabled \
  -Denable-console-bin=false -Dgi-docgen=disabled -Dappdata-test=disabled \
  -Djavascript=disabled -Dvala=disabled -Dprint=false -Dcheck-update=no \
  -Dlibbacktrace=false "${sanitize[@]}" 2>&1 | tee "$out/configure.log"
stage=compile
mapfile -t targets < <(python3 -B - "$here/build-targets.json" "$mode" <<'PY'
import json,sys
x=json.load(open(sys.argv[1]));targets=x['brush_baseline_targets'] if sys.argv[2]=='baseline-red' else x['candidate_union']
if sys.argv[2]=='positive': targets=targets+x['normal_negative_control_targets']
print('\n'.join(targets))
PY
)
expected_targets=35
if [[ "$mode" == positive ]]; then expected_targets=38; fi
if [[ "$mode" == baseline-red ]]; then expected_targets=3; fi
test "${#targets[@]}" = "$expected_targets"
meson compile -C "$build" -j 2 "${targets[@]}" 2>&1 | tee "$out/compile.log"
verify_source
if [[ "$mode" == sanitizer ]]; then
  # Actual commands/target scope, not a normal build relabeled sanitized.
  cp "$build/compile_commands.json" "$out/sanitizer-compile-commands.json"
  ninja -C "$build" -t commands > "$out/sanitizer-build-commands.txt"
  printf 'Fresh normal Meson address+undefined; external GTK/dependencies are not instrumented.\n' > "$out/sanitizer-scope.txt"
fi
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
failed=0
if [[ "$mode" == positive ]]; then
  stage=canvas-old-provider-red
  bash "$here/run_canvas_ownership_red.sh" "$src" "$build" "$run" "$out" || failed=1
fi
if [[ "$mode" == sanitizer ]]; then
  stage=wire-lsan-old-provider-red
  bash "$here/run_wire_lsan_red.sh" "$repo" "$run" "$out" || failed=1
fi
if [[ "$mode" != baseline-red ]]; then
  stage=locales
  locale_state stage
  export GIMP3_LOCALEDIR="$run/locale"
  stage=header-verification
  meson test -C "$build" --no-rebuild --num-processes 1 --print-errorlogs \
    --logbase header-verification painter-public-header-compilation painter-additional-header-routes \
    > "$run/header-console.log" 2>&1 || failed=1
  python3 -B "$repo/tools/check_painter_ui_ci.py" --log "$build/meson-logs/header-verification.json" \
    --expect painter-public-header-compilation painter-additional-header-routes \
    --output "$out/header-verification-results.json" || failed=1
  python3 -B - "$run/header-console.log" "$out/header-console.log" <<'PYSAFE'
import pathlib,re,sys
lines=pathlib.Path(sys.argv[1]).read_text(errors='replace').splitlines()
pathlib.Path(sys.argv[2]).write_text('\n'.join(x for x in lines if not re.match(r'^\s*(?:>>>|Inherited environment:|Environment:|[A-Za-z_][A-Za-z_0-9]*=)',x))+'\n')
PYSAFE
  if [[ -f "$build/app/painter/tests/headers/header-compile-result.json" ]]; then
    cp "$build/app/painter/tests/headers/header-compile-result.json" "$out/header-compile-result.json"
  fi
fi
stage=brush-runtime
bash "$here/run_brush_runtime.sh" "$src" "$build" "$out" "$run/runtime" "$mode" || failed=1
# Bind both frame providers to actual recorded execution environments.
# Run after the new normal seven-case log exists; never reset prior failures.
if [[ "$mode" == positive ]]; then
  stage=frame-style-old-provider-red
  bash "$here/run_frame_style_red.sh" "$src" "$build" "$run" "$out" \
    "$build/meson-logs/frame-style-positive.json" || failed=1
fi
# Independent mask-window proof after all existing runtime/control scopes.
# Its expected old failure never changes prior or candidate success requirements.
if [[ "$mode" != baseline-red ]]; then
  stage=canvas-mask-provider-proof
  mask_mode=normal; if [[ "$mode" == sanitizer ]]; then mask_mode=sanitizer; fi
  bash "$here/run_canvas_mask_control.sh" "$src" "$build" "$run" "$out" "$mask_mode" || failed=1
fi
stage=source-integrity
verify_source || failed=1
if [[ "$mode" != baseline-red ]]; then locale_state verify || failed=1; fi
# Nonacceptance handoff only AFTER original runtime and integrity outcomes.
# Capture failure must never alter the original acceptance exit status.
if [[ "$mode" == sanitizer ]]; then
  python3 -B "$here/preframe_identity.py" capture --source "$src" --build "$build" --run "$run" \
    --options-json "$out/sanitizer-runtime-options.json" --original-exit "$failed" \
    --output "$out/preframe-build-identity.json" > "$run/preframe-identity-private.log" 2>&1 || \
    printf '%s\n' '{"acceptance_eligible":false,"status":"identity-handoff-unavailable","original_driver_exit_unchanged":true}' > "$out/preframe-handoff-unavailable.json" || :
fi
(( failed == 0 )) || exit 1
printf 'Brush %s automated checks passed; actual four-image inspection and scoped acceptance remain separate.\n' "$mode" > "$out/acceptance.txt"
