#!/usr/bin/env bash
# Source after prepare-linux-build-deps.py. Keep the Ubuntu baseline separate.
linux_debian13_repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
linux_debian13_workspace="$(dirname "$linux_debian13_repo")"
linux_debian13_deps="${GIMP_DEPS_DIRECTORY:-$linux_debian13_workspace/.deps-debian13}"
linux_debian13_prefix="${GIMP_BUILD_PREFIX:-$linux_debian13_workspace/.build-prefix-debian13}"

if [[ ! -f "$linux_debian13_deps/env.sh" ]]; then
  echo "Missing local dependency environment: $linux_debian13_deps/env.sh" >&2
  return 1
fi
source "$linux_debian13_deps/env.sh"

export PATH="$linux_debian13_prefix/bin:$PATH"
export PKG_CONFIG_PATH="$linux_debian13_prefix/lib/x86_64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH"
export LD_LIBRARY_PATH="$linux_debian13_prefix/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
export GI_TYPELIB_PATH="$linux_debian13_prefix/lib/x86_64-linux-gnu/girepository-1.0:$GI_TYPELIB_PATH"
export XDG_DATA_DIRS="$linux_debian13_prefix/share:$XDG_DATA_DIRS"
export BABL_PATH="$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu/babl-0.1"
export GEGL_PATH="$GIMP_DEPS_ROOT/usr/lib/x86_64-linux-gnu/gegl-0.4"

unset linux_debian13_repo linux_debian13_workspace linux_debian13_deps linux_debian13_prefix
