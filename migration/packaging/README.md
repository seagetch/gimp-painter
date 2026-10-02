# Linux runtime packaging and platform gates

This recipe creates a relocatable **Debian 13 x86_64 prototype** from an already
completed Meson build. It does not turn a debugoptimized baseline into a release
build, enable missing optional formats, or satisfy unexecuted WBS 32–38 gates.

## Build ownership and preparation

Coordinate all builds, tests, installation staging and GUI phases through
`/workspace/shared/gimp-painter-build.lock`. Use a fresh output directory.
The install command uses `--no-rebuild` and `--destdir`; it never installs into
system directories or the normal application prefix. No package maintainer
script runs. Only GIMP's Meson install scripts run under the staging destination.

```sh
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
source tools/linux-debian13-env.sh
flock /workspace/shared/gimp-painter-build.lock \
  python3 tools/package-linux-runtime.py \
    --deps "$GIMP_DEPS_DIRECTORY" --output /workspace/shared/painter-package-NEW
```

The input build defaults to `build-debian13`; pass `--build` for another completed
Debian amd64 build. Runtime dependency ownership is matched against locked Debian
archives, and every used archive checksum is verified before its copyright
notice and package/source version are recorded. ELF closure includes plug-ins,
GEGL/babl operations, GTK input modules, pixbuf loaders and Python GI. libc,
libm and resolver remain host ABI dependencies. Both packaging and the smoke
validator hard-fail on any non-baseline host SONAME, even if a manifest lists it. Host Python 3.13, standard fonts,
fontconfig and a display session are required.

The launcher redirects all GIMP resource/library paths to the bundle and uses
an isolated Painter profile. For non-relocatable builds, the package appends
only a standard MyPaint resource search override to the bundled system gimprc.
GTK/pixbuf loader caches are regenerated in the isolated runtime cache after
relocation. They are not copied from the build or user environment.

## Automated relocation gate

```sh
flock /workspace/shared/gimp-painter-build.lock \
  python3 tools/test-linux-runtime.py \
    /workspace/shared/painter-package-NEW/gimp-painter-linux-x86_64 \
    --output /workspace/shared/painter-package-smoke-NEW
```

The test copies the package to a fresh path containing spaces and Japanese
characters, validates the file seal and checks every ELF object's dependency
resolution. It runs version and console create/fill/save/reopen checks under an
explicit environment whitelist with no developer dependency variables. When
strace is installed and the environment permits ptrace, attempted accesses to source/build/dependency directories
fail the resource relocation gate. Raw trace output stays local. A prohibited ptrace probe is reported as unavailable;
normal smoke tests continue without asserting a syscall audit pass.

Actual native GUI launch, drawing, save, close and reopen require a separate
recorded test with fresh screenshots. A console fill test is not a drawing,
tablet-input, X11/Wayland-equivalence or full compatibility test.

## Source and final-candidate gate

Default prototypes omit source archives. After committed history has been
reviewed to exclude private data, `--source-archive` creates exact `git archive`
exports of the declared GIMP commit and the pinned gimp-data commit. `.git`,
untracked files, builds and user state never enter these exports. A dirty
prototype includes uncommitted feature slices not represented in those exports,
so its manifest always marks the corresponding source as incomplete.

`--candidate --source-archive --aggregate-gate FILE` additionally requires
clean committed source, no untracked source files, unchanged pinned gimp-data content, and a passing gate JSON with
`source_commit` equal to the exact HEAD, `all_passed: true`, and `build_inputs`
with SHA-256 entries for `app/gimp-3.0` and `app/gimp-console-3.0`. It also requires
identical `build_freshness.before` and `build_freshness.after` snapshots around the
full aggregate, captured with the recipe's `capture_freshness_snapshot()` helper.
Old gates without these snapshots fail closed. The gate must use full aggregate
schema version 1 (`all_registered_meson_tests_frozen_linux_normal_build`). Its
registry hash, counts and each indexed target's name, suites and registered/actual
command must match the current Meson registry. Coverage must be complete, the
Meson exit status zero, every integrity flag true, and baseline exemptions absent.
Each target must explicitly be `OK`, return zero, and be classified `passed`, with
empty skipped-subtest, runtime-crash and failure-evidence arrays. Reported result
and suite totals are cross-checked against that coverage. Missing fields,
unmatched results, baseline failures, skipped subtests and Meson-OK Script-Fu
crashes all block staging even if `all_passed` was mistakenly set to true.
`registry_sha256` retains the exact raw `BUILD/meson-info/intro-tests.json` hash
captured for the aggregate, as in the version-1 aggregate generator. It is not
the hash of an archived `registered-tests.json`: that separate evidence copy may
be pretty-printed or have its environment metadata minimized. Such sanitization
must preserve the raw hash field and target identity/command fields; the package
guard never hashes the sanitized archive to compare it with the raw registry.
This is a local
package candidate, not authorization to publish or a claim that all-platform
acceptance is complete. Immediately before sealing, the recipe rechecks the source inventory, newly
created untracked source files, and the gimp-data commit/content snapshot.
Refresh the package and every smoke check after any
source or executable change. Include the build and executable hashes from the
package manifest in the final aggregate report.

### Aggregate freshness identity

Hold `/workspace/shared/gimp-painter-build.lock` across the completed default build,
both snapshot captures, the full aggregate, and candidate packaging. Stop other
source/build writers for this interval. Capture the first snapshot **after** the
complete default build and immediately before the tests. Capture the second
immediately after the full aggregate. The tests must not silently rebuild between
captures; any changed state requires another complete build and aggregate.

The aggregate driver can load the helper without invoking packaging:

```python
import importlib.util
from pathlib import Path

repository = Path("/path/to/gimp-painter").resolve()
build = repository / "build-debian13"
spec = importlib.util.spec_from_file_location(
    "package_linux", repository / "tools/package-linux-runtime.py")
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)

before = recipe.capture_freshness_snapshot(build, repository)
# Run the full aggregate here, recording every exit status without exceptions.
after = recipe.capture_freshness_snapshot(build, repository)
recipe.require_same_freshness(before, after, "aggregate")
gate["build_freshness"] = {"before": before, "after": after}
# Keep source_commit, exact executable build_inputs, and all_passed unchanged.
# all_passed may be true only when every required aggregate check passed.
```

The snapshots seal every concrete Ninja target (including intermediate and
non-default outputs), Ninja target/expanded-command fingerprints, generated Ninja
files, Meson configuration and command payloads, installed inputs/trees, tracked
source content/modes/links, untracked source inventory, and pinned gimp-data.
File timestamps/inodes and Ninja's `.ninja_log`/`.ninja_deps` bookkeeping are not
artifact identities. Missing outputs are represented explicitly; a previously
missing output appearing during convergence also invalidates the seal.

Candidate packaging compares its current state to that seal, runs ordinary
`ninja -C BUILD`, and compares again **before staging anything**. This allows
Ninja to apply PHONY/restat pruning that a dry-run cannot observe. There is no
textual "no work" waiver or target-name exception. A failed command, or any
changed source, graph/command, binary, generated header, resource, mode, link or
configuration invalidates the candidate, even if Ninja exits successfully. The
resulting changed build must be retested; it is never relabelled as tested.
The recipe checks the same seal again at the end of staging and records the
convergence command, exit status and snapshot hash in the package evidence.

The focused safety suite uses only private temporary Ninja projects:

```sh
export GIMP_DEPS_DIRECTORY=/workspace/shared/gimp-build-deps
. tools/linux-debian13-env.sh
python3 migration/tests/test_linux_runtime_package.py -v
```

These fixture checks are packaging-guard evidence, not a GIMP aggregate pass or
a final package candidate. Historical prototype artifacts and their validation
results retain their original identities.

`--archive` writes a zstd-compressed tar with sorted entries, normalized uid/gid
and the source commit timestamp, plus SHA256SUMS. This normalizes archive metadata;
it is not a claim of independently reproduced compiler output. For attachment
limits, split an archive into numbered files smaller than 20 MB and record
individual and reassembled hashes. Keep the original complete archive.

## Distribution status and licenses

The package preserves every tracked upstream COPYING/LICENSE notice from GIMP
and gimp-data, plus pinned splash/logo attribution and asset notices, and matching
Debian runtime copyright notices, together with referenced common license text.
It records corresponding Debian source package/version indexes. Those indexes
are not sufficient evidence of source availability or of every redistribution
obligation being met: dependency source delivery must be verified before external
distribution. The GIMP/gimp-data LICENSE files explicitly qualify historical
asset licensing. The Deevad36 and Kaerhon20 Painter brushes retain explicit CC0/public-domain
readme notices. Classic35, Experimental23, Ramon28 and Tanda35 have no group-local
notice. Debian documents related brush families as CC0, but that is not treated
as completed provenance verification for every legacy revision.

No Windows or macOS package, native execution, signature or notarization result
is inferred from Linux. Real tablet/tilt/pressure, Wayland, clean-machine release
installation, cross-OS file exchange and whole-port release acceptance remain
open until their specified platforms and input hardware are actually exercised.

## Current prototype evidence

`prototype-validation.json` records the executed prototype02 at source declaration
`faebd07b41b11eab9a56e7a54251dc32d1e04552`. Its immutable GUI executable predates
subsequent precision/filter changes; the source archive is intentionally incomplete
for the uncommitted binary feature slices. The runtime archive is 111,502,741 bytes
with SHA-256 `9c805f301d57ca9a1b90bba2690e40936c2cdc050143ab41c43d472a600837d0`.
Six numbered runtime parts are each at most 19,000,000 bytes. Source archives have
separate three-part GIMP and one-part gimp-data sets. All reassembly hashes passed.
These remain local review artifacts, not published files.

The native smoke also exposed a missing default `gimp-painter-smudge-tool` entry
in `etc/toolrc`. The source resource now contains this tool beside standard Smudge,
with a regression check for exactly one entry per Painter tool. Fresh-profile
GUI warning removal will be verified with the final package refresh.

Source archive layout: each Git export contains files at its root. Extract the
GIMP archive into a new source directory, then extract the gimp-data archive into
that directory's `gimp-data/` child before following the pinned Meson recipe.
The source archive names and hashes must match the package's declared commits.
