set -euo pipefail
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-deps-locked GIMP_BUILD_PREFIX=/workspace/shared/gimp-native-preset-gui/prefix
source /workspace/shared/gimp-native-preset-gui/source/tools/linux-debian13-env.sh
export PYTHONDONTWRITEBYTECODE=1 CCACHE_DISABLE=1
python3 /workspace/shared/named-editor-entry/mutations.py > /workspace/shared/named-editor-entry/mutant-build.log 2>&1
