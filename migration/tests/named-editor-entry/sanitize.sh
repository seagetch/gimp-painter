set -euo pipefail
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-deps-locked GIMP_BUILD_PREFIX=/workspace/shared/gimp-native-preset-gui/prefix
source /workspace/shared/gimp-native-preset-gui/source/tools/linux-debian13-env.sh
export PYTHONDONTWRITEBYTECODE=1 CCACHE_DISABLE=1
exec 9>/workspace/shared/gimp-native-preset-gui/build.lock; flock 9
python3 /workspace/shared/named-editor-entry/sanitize.py > /workspace/shared/named-editor-entry/asan-build.log 2>&1
