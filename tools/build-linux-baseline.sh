#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
workspace_dir="$(dirname "$repo_dir")"
prefix_dir="$workspace_dir/.build-prefix"
source "$repo_dir/tools/linux-build-env.sh"

clone_tag() {
  local destination="$1" repository="$2" tag="$3" expected="$4"
  if [[ ! -e "$destination/.git" ]]; then
    git clone --depth=1 --branch "$tag" "$repository" "$destination"
  fi
  local actual
  actual="$(git -C "$destination" rev-parse HEAD)"
  if [[ "$actual" != "$expected" ]]; then
    echo "Unexpected commit at $destination: $actual" >&2
    exit 1
  fi
}

configure() {
  local build_dir="$1" source_dir="$2"
  shift 2
  if [[ -f "$build_dir/build.ninja" ]]; then
    meson setup "$build_dir" "$source_dir" "$@" --reconfigure
  else
    meson setup "$build_dir" "$source_dir" "$@"
  fi
}

clone_tag "$workspace_dir/babl-0.1.114" https://github.com/GNOME/babl.git \
  BABL_0_1_114 ccc4c720bec6894f622cdad31d911bee4960cf8c
clone_tag "$workspace_dir/gegl-0.4.62" https://github.com/GNOME/gegl.git \
  GEGL_0_4_62 7b3aff1e7cab39272d7242683e9a599e9b1dc5f9

configure "$workspace_dir/babl-0.1.114/build" "$workspace_dir/babl-0.1.114" \
  --prefix="$prefix_dir" -Dwith-docs=false -Denable-vapi=false
meson compile -C "$workspace_dir/babl-0.1.114/build" -j 2
meson install -C "$workspace_dir/babl-0.1.114/build"

configure "$workspace_dir/gegl-0.4.62/build" "$workspace_dir/gegl-0.4.62" \
  --prefix="$prefix_dir" -Ddocs=false -Dintrospection=true
meson compile -C "$workspace_dir/gegl-0.4.62/build" -j 2
meson install -C "$workspace_dir/gegl-0.4.62/build"

if [[ ! -e "$repo_dir/gimp-data/meson.build" ]]; then
  git -C "$repo_dir" config submodule.gimp-data.url https://github.com/GNOME/gimp-data.git
  git -C "$repo_dir" submodule update --init --depth=1 gimp-data
fi

configure "$repo_dir/build" "$repo_dir" --prefix="$prefix_dir" \
  -Dauto_features=disabled -Dlibunwind=false
meson compile -C "$repo_dir/build" -j 2
meson install --no-rebuild -C "$repo_dir/build"
