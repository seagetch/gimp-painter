#!/usr/bin/env bash
# Separate, after-acceptance debugger observation. Never an acceptance verdict.
set -euo pipefail
started=$(/usr/bin/python3 -B -c 'import time; print(time.monotonic())')
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
out=$(realpath "${1:?existing artifact directory required}")
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary hosted Actions only' >&2; exit 2; }
[[ -z ${LD_PRELOAD:-} && -z ${LD_AUDIT:-} ]] || exit 2
evidence="$out/readiness-owner-diagnostic"
mkdir "$evidence"
receipt="$out/preframe-build-identity.json"
stage=identity-unavailable
record_state() {
  /usr/bin/python3 -B - "$evidence/status.json" "$stage" "$1" "$2" <<'PY'
import json,pathlib,sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({'format':1,'acceptance_eligible':False,
 'diagnostic_only':True,'stage':sys.argv[2],'diagnostic_step_exit':int(sys.argv[3]),'state':sys.argv[4],
 'original_driver_exit_unchanged':True,'dependency_closure':'GDB installed after original acceptance; diagnostic system dependencies may differ.',
 'qualification':'Separate instrumented build only; debugger changes timing; no acceptance or leak verdict. Primary gate exit unchanged.'},indent=2)+'\n')
PY
}
finish() {
  code=$?; trap - EXIT
  record_state "$code" finished || :
  if [[ -f "$receipt" ]]; then
    /usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/primary-final.json" || :
  fi
  # Original native_driver failure is never reset by this diagnostic.
  exit 0
}
trap finish EXIT
record_state 0 running
test -f "$receipt"
/usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/identity-verified.json"
# Read only these fixed fields after strict ownership and hash verification.
mapfile -t paths < <(/usr/bin/python3 -B - "$receipt" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
assert j['original_exit'] == 1
for name in ('run','source','build'): print(j[name])
PY
)
test "${#paths[@]}" = 3
run=${paths[0]}; primary_src=${paths[1]}; primary_build=${paths[2]}
private="$run/readiness-owner-private"
mkdir -m 700 "$private"
/usr/bin/python3 -B - "$private/budget.json" "$started" <<'PYBUDGET'
import json,pathlib,sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({'started_monotonic':float(sys.argv[2]),
 'step_seconds':1200,'controller_seconds':180,'cleanup_seconds':9,'outer_margin_seconds':30})+'\n')
PYBUDGET
src="$private/source"; build="$private/build"
stage=separate-source-overlay; record_state 0 running
# Fresh worktree, not mutation of protected primary registration or build.
git -C "$primary_src" worktree add --detach "$src" "${GITHUB_SHA:?}" > "$private/source.log" 2>&1
git -C "$src" -c submodule.gimp-data.url=https://github.com/GNOME/gimp-data.git \
  submodule update --init --depth=1 gimp-data >> "$private/source.log" 2>&1
test "$(git -C "$src/gimp-data" rev-parse HEAD)" = "$(git -C "$src" rev-parse HEAD:gimp-data)"
cat "$src/tools/brush-ci/readiness-owner/meson.append" >> "$src/app/tests/meson.build"
stage=official-debian-debugger-install; record_state 0 running
# Existing signed Debian13 repositories only. No new sources/security options;
# no privileged tracing, capability, seccomp, sysctl or namespace changes.
timeout --kill-after=10s 60s apt-get update > "$private/apt-update.log" 2>&1
timeout --kill-after=10s 180s apt-get install --no-install-recommends -y gdb > "$private/apt-install.log" 2>&1
/usr/bin/python3 -B - "$evidence/debugger-tool.json" <<'PY'
import hashlib,json,pathlib,re,subprocess,sys
p=pathlib.Path('/usr/bin/gdb')
version=subprocess.check_output(['/usr/bin/gdb','--nx','--nh','--version'],text=True,timeout=10).splitlines()[0]
package=subprocess.check_output(['dpkg-query','-W','-f=${Version}','gdb'],text=True,timeout=10)
assert len(version)<180 and len(package)<100 and re.fullmatch(r'[A-Za-z0-9.+:~_-]+',package)
pathlib.Path(sys.argv[1]).write_text(json.dumps({'acceptance_eligible':False,'gdb_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),
 'version':version,'official_debian_package_version':package,'installed_only_after_original_acceptance':True,
 'dependency_closure_may_differ_from_acceptance':True},indent=2)+'\n')
PY
stage=post-install-identity; record_state 0 running
/usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/post-install-identity.json"
# Reconstruct only the same known build environment, never a generic snapshot.
export GIMP_DEPS_DIRECTORY="$run/deps" GIMP_BUILD_PREFIX="$private/prefix"
source "$src/tools/linux-debian13-env.sh"
export GIMP3_LOCALEDIR="$private/locale" GDK_BACKEND=x11 PYTHONDONTWRITEBYTECODE=1
export ASAN_OPTIONS=halt_on_error=1:detect_leaks=1:fast_unwind_on_malloc=0:malloc_context_size=40:log_exe_name=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
unset LSAN_OPTIONS
unset GIMP_TEST_PROTOCOL_FIXTURE GIMP_TEST_PROTOCOL_SCENARIO GIMP_TEST_PROTOCOL_LOG_DIR GIMP_TEST_EXPECT_REDUNDANT_QUIT
unset GSETTINGS_BACKEND GIMP3_DIRECTORY GIMP3_DATADIR NO_AT_BRIDGE
unset GIMP_TEST_LAYER_UI_FILTER GIMP_TEST_COLORIFY_ONCANVAS
unset GIMP_TEST_TOOLBAR_PAINTER GIMP_TEST_TOOLBAR_SINGLE GIMP_TEST_TOOLBAR_SESSION_DIRECTORY GIMP_TEST_TOOLBAR_SESSION_PASS
unset GIMP_TEST_COMMON_POPUP_SCREENSHOTS GIMP_TEST_COMMON_POPUP_DIAGNOSTICS
unset GIMP_TEST_COMMON_POPUP_DOCK_TRACE GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES
unset GIMP_TEST_COMMON_LAYER_POPUP GIMP_TEST_FILTER_LAYER_POPUP GIMP_TEST_OVERLAY_PAINT_GATE
unset GIMP_BRUSH_POPUP_CAPTURE GIMP_BRUSH_POPUP_BOTTOM_CAPTURE DISPLAY WAYLAND_DISPLAY
profile="$private/profile"
mkdir -p "$profile"/{home,config,cache,data,state,runtime,tmp,gimp-temp,gimp-cache}
chmod 700 "$profile/runtime" "$profile/tmp" "$profile/gimp-temp" "$profile/gimp-cache"
export HOME="$profile/home" XDG_CONFIG_HOME="$profile/config" XDG_CACHE_HOME="$profile/cache"
export XDG_DATA_HOME="$profile/data" XDG_STATE_HOME="$profile/state" XDG_RUNTIME_DIR="$profile/runtime"
export TMPDIR="$profile/tmp" GIMP3_TEMPDIR="$profile/gimp-temp" GIMP3_CACHEDIR="$profile/gimp-cache"
export GDK_SCALE=1 GDK_DPI_SCALE=1 GIMP_TESTING_SESSIONRC_NAME=sessionrc-native-oncanvas
build_remaining() {
  # Absolute15min preparation cutoff from entry; no per-command budget reset.
  /usr/bin/python3 -B - "$started" <<'PYREMAIN'
import math,sys,time
print(max(0,math.floor(float(sys.argv[1])+900-time.monotonic())))
PYREMAIN
}
stage=separate-configure; record_state 0 running
# Ordinary Meson/Ninja dependency closure, no object/link command replay.
remaining=$(build_remaining); (( remaining > 0 ))
timeout --kill-after=5s "${remaining}s" meson setup "$build" "$src" --prefix="$GIMP_BUILD_PREFIX" \
  --buildtype=debugoptimized -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=disabled -Dpainter-http=disabled -Denable-console-bin=false \
  -Dgi-docgen=disabled -Dappdata-test=disabled -Djavascript=disabled -Dvala=disabled \
  -Dprint=false -Dcheck-update=no -Dlibbacktrace=false -Db_sanitize=address,undefined \
  > "$private/configure.log" 2>&1
stage=separate-build; record_state 0 running
mapfile -t targets < <(python3 -B - "$src/tools/brush-ci/matched-preclick/policy.json" <<'PYTARGETS'
import json,sys
j=json.load(open(sys.argv[1])); targets=j['common_targets']
assert targets[0]=='app/tests/matched-preclick:executable' and len(targets)==13
print('readiness-owner');print('\n'.join(targets[1:]))
PYTARGETS
)
test "${#targets[@]}" = 13
remaining=$(build_remaining); (( remaining > 0 ))
timeout --kill-after=5s "${remaining}s" meson compile -C "$build" -j 2 "${targets[@]}" > "$private/build.log" 2>&1
stage=separate-locales; record_state 0 running
python3 -B - "$src/tools/run-painter-frozen-aggregate.py" "$build" "$private/locale" <<'PYLOCALE' > "$private/locales.log" 2>&1
import hashlib,json,pathlib,runpy,sys
helper,build,dest=sys.argv[1:];p=pathlib.Path(helper)
assert hashlib.sha256(p.read_bytes()).hexdigest()=='4cdc8a36be383487c4cca06fc173f8d3c2b949370318b66fd15f22548eecfada'
f=runpy.run_path(helper);j=f['stage_build_locales'](build,dest)
f['verify_staged_locales'](dest,j)
pathlib.Path(dest+'.json').write_text(json.dumps(j,indent=2)+'\n')
PYLOCALE
stage=separate-identity; record_state 0 running
/usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/primary-after-build.json"
/usr/bin/python3 -B "$here/readiness-owner/identity.py" capture --receipt "$receipt" --output "$private/owner-build-identity.json"
/usr/bin/python3 -B "$here/readiness-owner/identity.py" summary --receipt "$private/owner-build-identity.json" --output "$evidence/build-identity.json"
stage=isolated-debugger-observation; record_state 0 running
ulimit -c 0
# Only structured controller/identity reports enter artifacts. Meson/MI/apt
# stdout remains private; no raw environment, arguments, locals or core dumps.
xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
  dbus-run-session -- meson test -C "$build" --no-rebuild --num-processes 1 \
    --timeout-multiplier 3 --logbase readiness-owner-nonacceptance \
    --wrapper "python3 $src/tools/brush-ci/readiness-owner/identity.py wrapper --receipt $private/owner-build-identity.json --output $evidence/sample.json --" \
    --test-args '-p /brush-thumbnail-native-popup/private-target-release' readiness-owner \
    > "$private/meson-console.log" 2>&1
stage=post-observation-identity; record_state 0 running
/usr/bin/python3 -B "$here/readiness-owner/identity.py" verify --receipt "$private/owner-build-identity.json" --output "$private/after-identity.json"
/usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/primary-after-observation.json"
stage=diagnostic-finished-not-acceptance; record_state 0 running
