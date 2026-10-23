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

The core workflow uses Bash, Git, Meson, Ninja, and `flock`. Xvfb and
`dbus-run-session` are only needed for the optional headless display wrapper.
Their absence does not block compilation or the display-independent tests.
The shared lock follows `build-test-coordination.md`; on another host set
`GIMP_PAINTER_BUILD_LOCK` to a common writable lock path.

## Create and configure

Start in the development checkout. Resolve `HEAD` once to a full base commit
ID and snapshot the tracked working changes, including staged changes, as a
binary patch. Apply that patch to the disposable worktree before configuring.
Local verification must precede publishing the change; committing first is not
a prerequisite. Record the base ID and patch hash together.

The example deliberately stops if untracked files are present, rather than
silently omitting a new source or test. Review that list, copy only the intended
new project files into the disposable worktree, and record their paths and
SHA-256 hashes before continuing. Never copy credentials, unrelated files, or
an entire unreviewed directory. Nested submodule changes require their own
reviewed snapshot; this example uses the recorded gimp-data gitlink.

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
git -C "$repo" ls-files --others --exclude-standard > "$run/untracked-files.txt"
test ! -s "$run/untracked-files.txt"
sha256sum "$run/development-before.patch" > "$run/source-patch.sha256"
git -C "$repo" worktree add --detach "$src" "$commit"
git -C "$src" submodule update --init gimp-data
git -C "$src" submodule status > "$run/submodules.txt"
test -z "$(git -C "$src" status --porcelain=v1 --untracked-files=all)"
if test -s "$run/development-before.patch"; then
  git -C "$src" apply --index --check "$run/development-before.patch"
  git -C "$src" apply --index "$run/development-before.patch"
fi
git -C "$src" diff --binary HEAD -- > "$run/tested-source-before.patch"
cmp "$run/development-before.patch" "$run/tested-source-before.patch"
test ! -e "$build"
flock "$lock" meson setup "$build" "$src" --prefix="$run/prefix" \
  -Dauto_features=disabled -Dlibunwind=false \
  -Dheadless-tests=disabled -Dpainter-http=disabled \
  2>&1 | tee "$run/meson-setup.log"
```

`gimp-data` uses the selected commit's gitlink, not the tip of its branch.
For the HTTP configuration, repeat with a new worktree/build pair and
`-Dpainter-http=enabled`; it requires libsoup-3.0 >= 3.2. For an instrumented
configuration, use another fresh pair and Meson's
`-Db_sanitize=address,undefined`. Record the actual configuration and toolchain;
a sanitizer-enabled setup alone is not sanitizer test evidence.

Check known instrumentation constraints before a long build:

- The standard undefined-sanitizer configuration must enable consistent RTTI
  for Painter libraries and core/display consumers, as specified in
  `rtti-separation.md`. Keep UBSan vptr checks enabled. Do not restore the old
  individually substituted object-file workaround.
- In the confirmed ptrace-constrained executor, LeakSanitizer cannot run even
  for build-time helpers. Set the process environment before both compilation
  and tests: `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1` and
  `UBSAN_OPTIONS=halt_on_error=1:abort_on_error=1:print_stacktrace=1`.
  This retains address and undefined-behavior checks but provides no leak-check
  coverage. Do not disable leak checks on other hosts without evidence.
- Check runtime display capability independently from D-Bus. A wrapper's
  D-Bus use is not evidence that every wrapped test requires D-Bus.

## Compile and test

Use a fresh, writable application profile before launching any test, including
GUI tests. This was already required by `../baseline/debian13/README.md`;
carry it forward when replacing the build workflow. Otherwise GTK's recent-file
writer can target a read-only home, and the configured GEGL swap directory can
remain nonexistent. These fixture errors must not be mislabeled as product
regressions or hidden by suppressing fatal warnings.

```sh
mkdir -p "$run/profile"/{home,config,cache,data,state,runtime,tmp}
export HOME="$run/profile/home"
export XDG_CONFIG_HOME="$run/profile/config"
export XDG_CACHE_HOME="$run/profile/cache"
export XDG_DATA_HOME="$run/profile/data"
export XDG_STATE_HOME="$run/profile/state"
export XDG_RUNTIME_DIR="$run/profile/runtime"
export TMPDIR="$run/profile/tmp"
export GSETTINGS_BACKEND=memory
chmod 700 "$XDG_RUNTIME_DIR"
flock "$lock" meson compile -C "$build" -j 2 \
  2>&1 | tee "$run/meson-compile.log"
flock "$lock" meson test -C "$build" --print-errorlogs \
  2>&1 | tee "$run/meson-test.log"
```

With `headless-tests=disabled`, run all registered tests. Record each actual
PASS, FAIL, SKIP, and timeout; tests that require a missing display may return
77 and remain unverified. Do not suppress other registrations simply because
the optional headless wrapper cannot start.

If Xvfb and a session bus are available, use another source/build pair with
`-Dheadless-tests=enabled` and run `meson test -C "$build" --setup=headless
--print-errorlogs`. The registered native Fill Brush tool/options test is
`painter-fill-brush-ui`; its cases still require a usable display. A socket
creation error is an environment blocker for that coverage, not a passing test
or a blocker for unrelated core tests. Do not infer that TCP is prohibited from
an AF_UNIX failure: check permitted address families through the supported
executor. On this executor, AF_UNIX socket creation returns EPERM while AF_INET
loopback socket creation, bind and listen succeed, including in the default
sandbox.

The installed Xvfb supports the standard systemd socket-activation interface:
an authenticated, loopback-only prebound AF_INET listener can be inherited at
fd 3 using `LISTEN_PID` and `LISTEN_FDS=1`. Disable automatically created
listeners with `-nolisten tcp -nolisten unix -nolisten local`, keep `-auth` with
a private temporary authority file, and connect clients through
`DISPLAY=127.0.0.1:N`. The inherited port must be 6000 + N. No systemd daemon,
security-setting changes, socket interception, or X server modifications are
required. Ordinary `-listen tcp` binds wildcard addresses and is not an
acceptable substitute for loopback-only binding.

Before GUI tests, verify the actual listener address, authenticated client
success, and rejection of a client with an empty authority file. Bound server
lifetime and remove temporary credentials during cleanup. Keep the server and
tests in one execution session if independent executor namespaces or startup
failures prevent sharing the listener. See the official
[sd_listen_fds interface](https://manpages.debian.org/trixie/libsystemd-dev/sd_listen_fds.3.en.html).
The 2026-10-11 loopback validation demonstrated these checks using unmodified
Xvfb. All 16 previously skipped native GUI registrations passed in subsequent
fixture-adjusted runs (252 TAP cases, including all 32 Fill cases). Preserve
these qualifications: Layer used Meson's timeout multiplier 3 and finished in
83.69 seconds instead of its original 60-second timeout; Profile used
`GIMP3_DATADIR=$build/data` with real generated resources rather than an installed
prefix; Save used `GSETTINGS_BACKEND=memory` as in the existing isolated-test
harnesses. This does not validate persistent dconf integration or installed
startup. Retain initial failures and timeouts with the successful reruns.

Keep command exit statuses and `meson-logs/testlog.*` with the exact source
snapshot, options, and toolchain. On 2026-10-11, the standard no-headless run
executed 124 registrations: 108 passed and 16 genuinely display-dependent
registrations skipped. The subsequent focused sanitizer run exposed a real
brush-transform arithmetic error; ordinary test success does not negate it.

## Inspect source changes, including after a failed phase

Run these checks even if configuration, compilation, or tests failed:

```sh
git -C "$src" status --short --untracked-files=all --ignore-submodules=none \
  > "$run/source-status.txt"
git -C "$src" diff --binary HEAD -- > "$run/source-after-build.patch"
cmp "$run/tested-source-before.patch" "$run/source-after-build.patch"
git -C "$src" diff --check
git -C "$repo" status --porcelain=v1 --untracked-files=all > "$run/development-after.status"
git -C "$repo" diff --binary HEAD -- > "$run/development-after.patch"
cmp "$run/development-before.status" "$run/development-after.status"
cmp "$run/development-before.patch" "$run/development-after.patch"
```

Review changes relative to the tested pre-build snapshot, and every untracked or changed
submodule entry. Preserve needed untracked files and submodule diffs separately;
the top-level patch does not contain them. Investigate unexpected changes and
port intentional fixes through normal source review, then retest the exact final
source snapshot before committing and publishing it. Do not blindly copy generated files back or discard the worktree
before reviewing its evidence. Concurrent edits to the development checkout
also require investigation if the comparison changes.

This separation isolates ordinary source generation; it is not a sandbox for
untrusted build commands. Never move a configured build to another source path.
