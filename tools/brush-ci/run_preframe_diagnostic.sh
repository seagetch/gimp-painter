#!/usr/bin/env bash
# Separate, after-acceptance debugger observation. Never an acceptance verdict.
set -euo pipefail
here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
out=$(realpath "${1:?existing artifact directory required}")
[[ ${GITHUB_ACTIONS:-false} == true ]] || { echo 'BLOCKED: ordinary hosted Actions only' >&2; exit 2; }
evidence="$out/preframe-diagnostic"
mkdir "$evidence"
receipt="$out/preframe-build-identity.json"
stage=identity-unavailable
finish() {
  code=$?; trap - EXIT
  /usr/bin/python3 -B - "$evidence/status.json" "$stage" "$code" <<'PY'
import json,pathlib,sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({'format':1,'acceptance_eligible':False,
 'diagnostic_only':True,'stage':sys.argv[2],'diagnostic_step_exit':int(sys.argv[3]),
 'original_driver_exit_unchanged':True,'dependency_closure':'GDB installed after original acceptance; diagnostic system dependencies may differ.',
 'qualification':'Debugger stops perturb timing; no leak verdict under ptrace. First matching wait thread is not necessarily failing phase.'},indent=2)+'\n')
PY
  # This explicitly nonacceptance step does not override native_driver outcome.
  exit 0
}
trap finish EXIT
test -f "$receipt"
/usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/identity-verified.json"
# Read only these fixed fields after strict ownership and hash verification.
mapfile -t paths < <(/usr/bin/python3 -B - "$receipt" <<'PY'
import json,sys
j=json.load(open(sys.argv[1]))
for name in ('run','source','build'): print(j[name])
PY
)
test "${#paths[@]}" = 3
run=${paths[0]}; src=${paths[1]}; build=${paths[2]}
private="$run/preframe-diagnostic-private"
mkdir -m 700 "$private"
stage=official-debian-debugger-install
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
stage=post-install-identity
/usr/bin/python3 -B "$here/preframe_identity.py" verify --receipt "$receipt" --output "$evidence/post-install-identity.json"
# Reconstruct only the same known build environment, never a generic snapshot.
export GIMP_DEPS_DIRECTORY="$run/deps" GIMP_BUILD_PREFIX="$run/prefix"
source "$src/tools/linux-debian13-env.sh"
export GIMP3_LOCALEDIR="$run/locale" GDK_BACKEND=x11 PYTHONDONTWRITEBYTECODE=1
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
stage=isolated-debugger-observation
ulimit -c 0
# Only structured controller/identity reports enter artifacts. Meson/MI/apt
# stdout remains private; no raw environment, arguments, locals or core dumps.
xvfb-run --auto-servernum --server-args='-screen 0 1080x1920x24 -nolisten tcp' \
  dbus-run-session -- meson test -C "$build" --no-rebuild --num-processes 1 \
    --timeout-multiplier 3 --logbase preframe-nonacceptance \
    --wrapper "python3 $src/tools/brush-ci/preframe_identity.py wrapper --receipt $receipt --output $evidence/sample.json --controller $src/tools/brush-ci/sample_case14.py --" \
    --test-args '-p /brush-thumbnail-native-popup/private-target-release' painter-compact-options \
    > "$private/meson-console.log" 2>&1
stage=diagnostic-finished-not-acceptance
