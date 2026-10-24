set -euo pipefail
w=/workspace/shared/gimp-native-preset-gui;o=/workspace/shared/named-editor-entry
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-deps-locked GIMP_BUILD_PREFIX="$w/prefix"
source "$w/source/tools/linux-debian13-env.sh"
export PYTHONDONTWRITEBYTECODE=1 CCACHE_DISABLE=1
export HOME="$o/profile/home" XDG_CONFIG_HOME="$o/profile/config" XDG_CACHE_HOME="$o/profile/cache" XDG_DATA_HOME="$o/profile/data" XDG_STATE_HOME="$o/profile/state" XDG_RUNTIME_DIR="$o/profile/runtime" TMPDIR="$o/profile/tmp"
mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR" "$TMPDIR" "$o/runtime-build/app/tests/gimpdir-output"
chmod 700 "$XDG_RUNTIME_DIR" "$TMPDIR"
if ! test -d "$o/source/app/tests/gimpdir"; then cp -a "$w/source/app/tests/gimpdir" "$o/source/app/tests/"; fi
export GIMP_TESTING_ABS_TOP_SRCDIR="$o/source" GIMP_TESTING_ABS_TOP_BUILDDIR="$o/runtime-build" GIMP_TESTING_PLUGINDIRS="$w/build/plug-ins/common" UI_TEST=yes
export G_TEST_BUILDDIR="$w/build/app/tests" G_TEST_SRCDIR="$o/source/app/tests"
export LD_LIBRARY_PATH=$(find "$w/build" -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
unset GIMP_TEST_EDITOR_NAMED_SCREENSHOT
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
unset GSETTINGS_BACKEND GIMP3_DATADIR GIMP3_DIRECTORY
exec 9>"$w/build.lock"; flock 9
python3 "$w/source/tools/run-painter-xvfb.py" "$o/named-editor-asan" "$@"
