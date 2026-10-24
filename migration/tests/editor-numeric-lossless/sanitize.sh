set -euo pipefail
w=/workspace/shared/gimp-native-preset-gui
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-deps-locked GIMP_BUILD_PREFIX="$w/prefix"
source "$w/source/tools/linux-debian13-env.sh"
export PYTHONDONTWRITEBYTECODE=1 CCACHE_DISABLE=1
exec 9>"$w/build.lock"; flock 9
python3 /workspace/shared/editor-controls-audit/sanitize.py
