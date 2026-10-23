# Reproduce the Debian 13 build and TCP portrait test environment

This workflow installs no host packages. It uses the checked-in Debian package
lock, a detached source worktree, ordinary Meson/Ninja and a private install
prefix. See `native-build-workflow.md` for source snapshot and generated-file
review requirements. Do not modify Meson-generated backends.

## Fresh environment

From the repository root on Debian 13 amd64 (Python, C/C++ compiler, Git,
APT, dpkg-deb and Debian archive keyring must already exist):

```sh
set -euo pipefail
repo=$PWD
workspace=$(dirname "$repo")
python3 tools/prepare-linux-build-deps.py --directory "$workspace/.deps-debian13"
run=$(mktemp -d "$workspace/gimp-native.XXXXXXXX")
commit=$(git rev-parse HEAD)
git worktree add --detach "$run/source" "$commit"
git -C "$run/source" -c submodule.gimp-data.url=https://github.com/GNOME/gimp-data.git \
  submodule update --init --depth=1 gimp-data
export GIMP_DEPS_DIRECTORY="$workspace/.deps-debian13"
export GIMP_BUILD_PREFIX="$run/prefix"
source "$run/source/tools/linux-debian13-env.sh"
export HOME="$run/profile/home" XDG_CONFIG_HOME="$run/profile/config"
export XDG_CACHE_HOME="$run/profile/cache" XDG_DATA_HOME="$run/profile/data"
export XDG_STATE_HOME="$run/profile/state" XDG_RUNTIME_DIR="$run/profile/runtime"
export TMPDIR="$run/profile/tmp"
mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME" "$XDG_DATA_HOME" \
  "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR" "$TMPDIR"
chmod 700 "$XDG_RUNTIME_DIR" "$TMPDIR"
export CCACHE_DISABLE=1
export GIMP_PAINTER_BUILD_LOCK="$workspace/gimp-painter-build.lock"
unset GSETTINGS_BACKEND GIMP3_DATADIR
flock "$GIMP_PAINTER_BUILD_LOCK" meson setup "$run/build" "$run/source" \
  --prefix="$run/prefix" -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=disabled -Dpainter-http=disabled
flock "$GIMP_PAINTER_BUILD_LOCK" meson compile -C "$run/build" -j 2
python3 "$run/source/tools/verify_build_executables.py" "$run/build"
meson install --no-rebuild -C "$run/build"
```

For an uncommitted source patch, follow the explicit snapshot/copy/hash procedure
in the native workflow before configuring. Do not silently omit untracked inputs.

## Authenticated IPv4 display

`tools/run-painter-xvfb.py COMMAND ARGS` reserves only `127.0.0.1`, passes the
listening socket to unmodified Xvfb through its supported systemd activation
interface, and sets `DISPLAY=127.0.0.1:<number>`. Screen size is 1080x1920x24.
All automatic TCP and Unix-domain listeners are disabled; the inherited listener
remains enabled. Private MIT-MAGIC-COOKIE-1 credentials are created per run.
Before launching the command, an authenticated X client must succeed and an
empty-authority client must fail. There is no `-ac` or wildcard binding.

The runner skips displays with existing X lock files. It bounds commands to
3500 seconds, creates an isolated command process group, and cleans up that
group, the server and temporary credentials on completion or failure. It does
not remove another server's lock or kill unrelated processes. A longer aggregate
must use a deliberately reviewed bound rather than an unbounded server.

```sh
python3 "$repo/tools/run-painter-xvfb.py" /bin/true
python3 "$repo/tools/run-painter-xvfb.py" meson test -C "$run/build" \
  --num-processes 1 --print-errorlogs --logbase baseline-full
```

Keep original failed/timed-out test results. A separately measured per-run budget
is not a successful original aggregate. Do not export a global GSettings or data
path override over fixture-specific registered conditions.

## Inspect the Painter canvas, not only startup

The current branch defines `painter-canvas-ui` as false by default. A clean
profile therefore opens the standard layout. The existing View → Painter Canvas
Controls action enables the custom controls; an image must also be open. For an
isolated display test only, seed a dedicated `gimprc` with
`(painter-canvas-ui yes)` and open `migration/fixtures/gimp3-baseline.xcf` through
the installed binary. Do not overwrite an existing user profile. A welcome-only
screenshot cannot establish Painter UI behavior.

On 2026-10-15, source `6e6f2568e7593089c4f3c93e4613354c1f915001` plus the reviewed
three-header route repair configured, built all 3024 steps, installed, and passed
241 executable checks. Both header registrations passed, including 17 route
controls and 393 C11/C++14 probes. Exact executable provenance and screenshot
hashes are in `migration/tests/tcp-portrait-environment/provenance.json`.

The installed and build executables have the same ELF Build ID and byte-identical
`.text`. Their whole-file hashes differ because installation removes build-tree
RUNPATH. The fixture capture visibly includes the left tool column, right layer
tiles, color controls and bottom active-tool options. This is bounded display
verification, not full old-UI equivalence. The capture process was deliberately
terminated; it is not clean-shutdown acceptance. The independent full aggregate
and known compact-options registration budget issue remain separately tracked.
