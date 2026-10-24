set -euo pipefail
w=/workspace/shared/gimp-native-preset-gui;o=/workspace/shared/editor-type-alias
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-deps-locked GIMP_BUILD_PREFIX="$w/prefix"
source "$w/source/tools/linux-debian13-env.sh"
export LD_LIBRARY_PATH=$(find "$w/build" -maxdepth 1 -type d -name 'libgimp*' | paste -sd:):${LD_LIBRARY_PATH:-}
"$o/alias-first"; echo 'C11 alias-first PASS'
"$o/native-first"; echo 'C++14 native-first alias TypeTraits PASS'
NAMED_EDITOR_BINARY="$o/ownership" bash /workspace/shared/named-editor-entry/run-private.sh
