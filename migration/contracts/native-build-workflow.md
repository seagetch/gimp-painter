# Native builds in a separate source worktree

Use an ordinary detached Git worktree and a fresh out-of-source Meson build.
GIMP's generators intentionally update some tracked source files: for example,
`tools/meson-mkenums.py` writes enum C files and `pdb/meson-pdbgen.py` directs
PDB output into the source tree. A separate build directory alone does not
protect the development checkout from those writes. Give each configuration
its own source worktree as well as its own build directory.

Do not copy or edit `build.ninja`, `meson-private/meson_exe*.dat`, or other Meson
internal state. Configure the selected source directly and let Meson own its
generated backend. The removed `prepare-guarded-ninja.py` helpers and their
historical test results are not part of this workflow. Existing recovery
archives and earlier test artifacts remain historical evidence.

## Prerequisites

Use an established Linux build environment with the development dependencies
listed in `INSTALL.in` and version requirements in `meson.build`. In particular,
babl must be at least 0.1.114 and GEGL at least 0.4.62. The Ubuntu baseline is
described in `../baseline/linux-build.md`; the Debian 13 baseline records its
versions in `../baseline/debian13/build-environment.json`. Select one compatible
dependency environment before configuring; do not reuse a recovered build tree.
If an existing dependency prefix is needed, set its normal pkg-config, library,
and typelib search paths explicitly. The new install prefix below is separate
from that dependency prefix.

The commands below use Bash, Git, Meson, Ninja, `flock`, `xvfb-run`, and
`dbus-run-session`. The shared lock follows `build-test-coordination.md`; on
another host set `GIMP_PAINTER_BUILD_LOCK` to a common writable lock path.

## Create and configure

Start in the development checkout with the intended changes committed. `HEAD`
is resolved once to a full commit ID; uncommitted changes are not included.
To test another revision, set `commit` to its full reviewed commit ID instead.

```sh
set -euo pipefail
repo=$(git rev-parse --show-toplevel)
commit=$(git -C "$repo" rev-parse --verify 'HEAD^{commit}')
run=$(mktemp -d "$(dirname "$repo")/gimp-native.XXXXXXXX")
src="$run/source"
build="$run/build"
lock="${GIMP_PAINTER_BUILD_LOCK:-/workspace/shared/gimp-painter-build.lock}"
printf '%s\n' "$commit" > "$run/source-commit.txt"
git -C "$repo" status --porcelain=v1 --untracked-files=all > "$run/development-before.status"
git -C "$repo" diff --binary HEAD -- > "$run/development-before.patch"
git -C "$repo" worktree add --detach "$src" "$commit"
git -C "$src" submodule update --init gimp-data
git -C "$src" submodule status > "$run/submodules.txt"
test -z "$(git -C "$src" status --porcelain=v1 --untracked-files=all)"
test ! -e "$build"
flock "$lock" meson setup "$build" "$src" --prefix="$run/prefix" \
  -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=enabled -Dpainter-http=disabled \
  2>&1 | tee "$run/meson-setup.log"
```

`gimp-data` uses the selected commit's gitlink, not the tip of its branch.
For the HTTP configuration, repeat with a new worktree/build pair and
`-Dpainter-http=enabled`; it requires libsoup-3.0 >= 3.2. For an instrumented
configuration, use another fresh pair and Meson's
`-Db_sanitize=address,undefined`. Record the actual configuration and toolchain;
a sanitizer-enabled setup alone is not sanitizer test evidence.

## Compile and test

```sh
flock "$lock" meson compile -C "$build" -j 2 \
  2>&1 | tee "$run/meson-compile.log"
flock "$lock" meson test -C "$build" --setup=headless --print-errorlogs \
  painter-fill-brush-ui 2>&1 | tee "$run/fill-tool-test.log"
```

`painter-fill-brush-ui` is the registered native Fill Brush tool/options test.
The repository's `headless` setup supplies its Xvfb/D-Bus wrapper. Run the
broader suite separately with `meson test -C "$build" --setup=headless
--print-errorlogs` under the same lock when that verification is required.
Keep the command exit status and `meson-logs/testlog.*` with the source ID and
configuration. A missing dependency, unavailable display/session, skipped test,
or failed build remains a recorded blocker, not a passing test.

## Inspect source changes, including after a failed phase

Run these checks even if configuration, compilation, or tests failed:

```sh
git -C "$src" status --short --untracked-files=all --ignore-submodules=none \
  > "$run/source-status.txt"
git -C "$src" diff --binary HEAD -- > "$run/generated-source.patch"
git -C "$src" diff --check
git -C "$repo" status --porcelain=v1 --untracked-files=all > "$run/development-after.status"
git -C "$repo" diff --binary HEAD -- > "$run/development-after.patch"
cmp "$run/development-before.status" "$run/development-after.status"
cmp "$run/development-before.patch" "$run/development-after.patch"
```

Review the worktree's generated-source diff and every untracked or changed
submodule entry. Preserve needed untracked files and submodule diffs separately;
the top-level patch does not contain them. Investigate unexpected changes and
port intentional fixes through normal source review, then retest their committed
revision. Do not blindly copy generated files back or discard the worktree
before reviewing its evidence. Concurrent edits to the development checkout
also require investigation if the comparison changes.

This separation isolates ordinary source generation; it is not a sandbox for
untrusted build commands. Never move a configured build to another source path.
