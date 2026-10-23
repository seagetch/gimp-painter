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


## Recovered normal aggregate and calibrated completion (2026-10-11)

An executor replacement removed the previous source, build and dependency directories.
The checkpoint retained evidence, not runnable build artifacts. The standard rebuild
used public source `fca9fa6ed21e76a027d0edcf2c6684f9b38ad4b9`, pinned `gimp-data`
`a31e5dea4f572fb5f33b06f5c4b86db5f30643dc`, and the unchanged Debian lock/preparer.
All 560 package checksums and both Git integrity checks passed. Ordinary Meson
setup, 2,943 compilation steps and 363 subsequent test prerequisite steps passed.
No generated backend, object, production source or assertion was patched.

The single serial unfiltered normal command was `meson test -C build-native
--num-processes 1 --print-errorlogs` (with a distinct logbase). It executed all
124 registrations with **120 OK, 4 TIMEOUT, 0 FAIL, 0 registration SKIP**. The
unchanged registered budgets expired for FilterLayer (600 s), Layer UI (180 s),
XCF roundtrip (60 s) and Filter procedure (900 s). TAP recorded 1,075 passes,
one intentional >256 MiB slow-case skip and 108 planned cases not reached.
This original aggregate remains failed; its complete outputs are preserved.
Fill Brush core 40/40 and tool/options GUI 32/32 passed in that aggregate.

The saved sample of 27 matched completed registrations showed a 3.09x median
increase against prior normal timings. Compiler, build type and known environment
requirements matched; no single cause was established. A fixed-allocator SmallTiles live-update comparison found
that one-CPU affinity did not improve the case (35.47 s versus 38.37 s); no thread
limit was adopted. That exact case took 30.89 s in the aggregate, distinct from
the 108.16 s SmallTiles actual-old-live case. Full process-tree CPU measurements
and available host-resource observations are retained without inferring cause.

Measured execution rates then justified bounded **affected-only** completion using
ordinary Meson per-run `--timeout-multiplier`, keeping full registrations and all
assertions: FilterLayer 3 (1,800 s), Layer UI 3 (540 s), XCF 4 (240 s), Filter
procedure 3 (2,700 s). All four passed: 719.12 s, 277.52 s, 133.93 s and
1,864.68 s respectively. Three TAP registrations produced 258 passes and the
same intentional skip. The procedure reported 320 Blinds, 196 SmallTiles and
174 Retinex byte-exact legacy oracles, plus its analytic ROI/rejection/identity
checks. No production timeout, registered budget or assertion was changed.
These multipliers were the only timeout-policy change. Separate invocations,
fresh per-target profiles, verbose output and process sampling are also recorded.
This accommodates observed execution time in this environment; it does not
establish a runtime fix or a single host cause.

All 108 previously unreached TAP cases completed. Unique TAP coverage across the
original and affected-only runs is 1,183 passes and one skip. This union is not a
new all-124 aggregate. The two affected-only Xvfb sessions each remained below
one hour, required authentication, rejected unauthenticated access, bound only
loopback, and cleaned up their children and private credentials. HOME/XDG/TMP
were private; parent GSETTINGS_BACKEND/GIMP3_DATADIR overrides were removed so
registered fixture conditions remained effective. Tracked source matched all
10,511 pre-run hashes when evidence was frozen before any subsequent task edits.

WBS 08.012 remains DOING with no original-task completion credit. The known
brush-transform ASan/UBSan overflow is unfixed and was not rerun. Installed-prefix,
full sanitizer/LSan, HTTP and platform acceptance remain separate requirements.

See `../tests/native-full-aggregate-20261011.json`, its text TAP extracts, and
`../tests/native-affected-budget-20261011.json` with the affected output extract.
Their checksum-pinned Library checkpoint retains full sanitized stdout/stderr,
resource samples, source/dependency hashes and recovery runners; no public Git
ZIP is required. For another executor replacement, first check actual artifacts,
then recreate the pinned worktree, run the unchanged dependency preparer and use
ordinary Meson setup/compile. Replay the archived secure runner only with its
matching source/configuration and a private writable profile; never reconstruct
or modify generated Meson internals. The original unfiltered run has no multiplier;
the separate measured-budget invocations must remain explicitly labeled.
