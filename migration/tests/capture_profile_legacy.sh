#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run on the native cloud GTK display; leaves all existing app instances alone.
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
legacy="${1:-/workspace/shared/gimp-legacy-build}"
output="${2:-/workspace/shared/painter-profile-legacy-seeded}"
# env.sh predates nounset-safe generated environments.
set +u
source "$legacy/env.sh"
set -u
export GIMP2_DIRECTORY="$output/profile"
export XDG_CACHE_HOME="$output/cache"
export GEGL_PATH="$legacy/prefix/lib/gegl-0.3"
export LC_ALL=C
mkdir -p "$output"
if [ ! -e "$output/profile" ]; then
  cp -a "$root/migration/fixtures/profile-seeds" "$output/profile"
fi
exec 9>/workspace/shared/gimp-painter-build.lock
flock 9
{
  sha256sum "$legacy/prefix/bin/gimp-2.8"
  timeout 90 "$legacy/prefix/bin/gimp-2.8" --new-instance --no-splash --no-fonts \
    --batch '(gimp-context-set-opacity 37)' \
    --batch '(gimp-quit 0)'
  printf 'LEGACY_PROFILE_CAPTURE_EXIT=0\n'
} 2>&1 | tee "$output/capture.log"
