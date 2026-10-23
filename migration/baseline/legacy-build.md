# Legacy build: reference, observation and recovery

Reference: `afa43fae3e920210146abed514f136fd49f671b5` from
<https://github.com/seagetch/gimp-painter.git>, branch `gimp-2-8`.
The separate clean worktree used on 2026-10-01 is `../gimp-painter-legacy`.
Never switch the migration checkout back to the legacy branch to build it.

## Actual observation on 2026-10-01

`legacy/probe-2026-10-01.json` records the Debian 13 x86_64 environment, source
checksums, detected tool versions, pkg-config failures and actual bootstrap log.
The command `NOCONFIGURE=1 sh ./autogen.sh` exited 1 because libtool, gtk-doc,
autoconf, automake, intltool and xsltproc were missing. GTK2 / GEGL0.3 development
modules were not found. No configure/build/run/painting success is claimed.
A later dependency restoration requires a new probe; this dated observation
must not be rewritten to suggest it succeeded.

Reproduce the historical check in a disposable clean checkout (installs nothing;
bootstrap can generate or modify files inside that checkout):

```sh
git worktree add --detach ../gimp-painter-legacy afa43fae3e920210146abed514f136fd49f671b5
python3 tools/probe_legacy_build.py --source ../gimp-painter-legacy \
  --output /tmp/legacy-probe.json --bootstrap
```

## Available source pins are not a complete environment lock

`configure.ac` declares C++14, babl >=0.1.12, GEGL0.3 >=0.3.0,
GLib >=2.30.2, GTK2 >=2.24.10 and json-glib >=1.0. Additional dependencies
are inventoried in `../inventory/legacy-dependencies.tsv`. These are minima,
not exact reproducible dependency revisions.

The pinned source's Flatpak application manifest records:

| Dependency | Exact source revision | Declared tag |
| --- | --- | --- |
| babl | `e92ced2c54250af1dddb58f76ff1678d74a30dcf` | BABL_0_1_46 |
| GEGL | `29254dcd875d271043aba57d0e6ebc48ec63c013` | GEGL_0_3_34 |

Both URLs and pins are retained in `../inventory/legacy-flatpak-pins.tsv`.
However, GNOME Platform/SDK `3.28` has no immutable runtime commit in that
manifest, the gimp source is a moving branch, and base/dependency archives have
not all been retrieved and verified. The original manifest therefore cannot
close 02.001. A modern GEGL0.4 development package cannot satisfy `gegl-0.3`.

## Recovery procedure (independent child of 02.015)

1. **Restore tools and old ABI dependencies in an isolated prefix/container.**
   Use official distribution snapshots or official upstream releases; preserve
   checksums, signature verification, complete resolved package versions and
   base-image/runtime digest. Keep GEGL0.3 separate from modern GEGL0.4 and
   record the exact compiler, SDK, GTK2, GLib and babl actually used. Coordinate
   with modern-build work to avoid concurrent package-manager writes. A local
   dependency prefix is preferable to mutating the host system.
2. **Re-run the probe before building.** Bootstrap can use explicit
   `AUTOMAKE=automake ACLOCAL=aclocal` overrides if only newer supported tools
   are available; record the resolved versions. Do not claim the original
   automake-1.11 command ran in that case. Run `./autogen.sh --disable-gtk-doc`
   only with a recorded option change, and save configure, build and link logs.
   Keep optional-feature decisions visible, especially Script-Fu, the PDB
   procedures used by FilterLayer, and PNG export for baseline capture.
3. **Preserve compatibility patches.** Build failures from modern C++/GLib or
   other toolchains require a separately hashed patch series. Keep the pristine
   source revision and patch diff, and distinguish build-only adaptations from
   any change to rendering, file parsing or timing. If the latter are necessary,
   record an unresolved reference-fidelity risk rather than silently accepting
   changed behavior as the old baseline.
4. **Use an existing legacy executable when available.** Verify its source
   revision, build provenance, binary hash, dependency versions and feature set
   before trusting measurements. Do not substitute stock GIMP2.8 or a different
   gimp-painter revision solely because the version number looks similar.
   Run with an isolated empty user profile; never modify the user's old works
   in place. Record whether missing provenance limits a result to exploratory.
5. **Confirm startup, drawing and editable save/reopen.** Save startup logs,
   profile location, capture actions, one-stroke PNG/XCF results and hashes.
   Then capture the actual normal/CloneLayer/FilterLayer/composite fixtures,
   operation expectations and asynchronous traces in 02.003–02.013. An XCF
   manually encoded by a script tests a parser but is not a legacy writer run.
6. **Continue independent source extraction while runtime is unavailable.**
   `tools/freeze_legacy_assets.py` verifies source assets and
   `fixtures/strokes/FORMAT.md` defines the input/provenance contract. Source
   reader/writer formulas may inform implementation, with static evidence
   labels. Runtime compatibility tests remain incomplete and their dependent
   completion gates remain closed.

## Historical gates before recovery (2026-10-01 initial probe)

- 02.001: still needs a verified complete isolated build/dependency lock
- 02.002: still needs a legacy executable and successful reproducible build log
- 02.003–02.007: no legacy startup/drawing/XCF capture results yet
- 02.008: independently checkable source-asset lock
- 02.009: independently checkable recording format; recording/replay adapter
  and measured output remain 02.010
- 02.010–02.013: no measured stroke/layer/rotation/scheduler traces yet
- 02.014: source rights/provenance register exists; runtime fixture permissions
  and provenance remain coupled to captures
- 02.015: this recovery procedure is available; the aggregate section gate
  stays open until the required runtime and provenance work is completed

## First recovery update (2026-10-01)

The later 2026-10-01 recovery successfully built and installed the old source,
then ran headless drawing and normal/CloneLayer save/reopen captures. See
`legacy/README.md` and `legacy/build-report.json` for the pinned dependency lock,
explicit adaptations, real outputs and remaining failures. The initial dated
probe above is historical; it is no longer the current dependency availability
status. FilterLayer execution/save succeeded, but the reference reader crashes
on its output, so successful FilterLayer roundtrip and later runtime gates
remain unverified.


## Current recovery acceptance review (2026-10-15)

The historical limits above and in `legacy/README.md` describe earlier attempts.
The current `legacy/replay-20261015/report.json` and its sealed build logs supersede
“no second pristine replay”: the unchanged recipe completed end-to-end with new
pinned GIMP/babl/GEGL checkouts and a new dependency extraction. All 560 cached
archives matched the signed snapshot package lock. babl passed 23 tests. GEGL's
simple suite passed 28, skipped two, and failed `test-gegl-tile` (exit 2). The
recipe intentionally retained that failure and continued; overall recipe exit 0
is build success, not a green dependency suite. Installed GIMP 2.8.23 has SHA-256
`85bc4733504509eb7b5a215cd0d19f6b6af5c4a23cd6112987de92acf3d230a1`.

The separate positive builtin FilterLayer capture exercised original writer and
reader and compared every cache byte with the original PDB control. The installed
uninstrumented app independently reopened it (exit 0 and explicit pass marker).
The direct plug-in-edge XCF still crashes the same original reader (exit 139).
The builtin capture retains pending=1 and zero completion callbacks. Neither
result is a universal scheduler or XCF compatibility pass.

Later rotation and timeline packages contain genuine GUI-initialized observations
on authenticated loopback Xvfb; the initial failed UNIX-socket attempt remains
historical evidence. Rotation's two GUI captures both end in teardown segfaults
(command_exit=-11) after the measured rows are written. A captured GUI callback
is not physical device testing or a clean GUI lifecycle. Consult each package's
README, reconstruction instructions and sealed raw logs for its exact scope.

### Dependency repair: executable procedure

Run from the migration repository root on the recorded Debian 13 amd64 toolchain.
Use a new absolute, shell-safe directory without whitespace, outside the migration
checkout and all active modern prefixes. Never run the recipe against the active
migration source, an observer-patched tree, or the only copy of an old work.

```sh
set -e
ROOT="$PWD"
WORK=/absolute/path/to/new-legacy-recovery
test ! -e "$WORK" || exit 1
mkdir -p "$WORK"
python3 "$ROOT/tools/prepare-linux-build-deps.py" --directory "$WORK/deps" \
  --lock "$ROOT/migration/baseline/legacy/debian13-package-lock.json"
git clone https://github.com/seagetch/gimp-painter.git "$WORK/gimp"
git -C "$WORK/gimp" checkout --detach afa43fae3e920210146abed514f136fd49f671b5
git clone https://github.com/GNOME/babl.git "$WORK/babl"
git -C "$WORK/babl" checkout --detach e92ced2c54250af1dddb58f76ff1678d74a30dcf
git clone https://github.com/GNOME/gegl.git "$WORK/gegl"
git -C "$WORK/gegl" checkout --detach 29254dcd875d271043aba57d0e6ebc48ec63c013
python3 "$ROOT/tools/probe_legacy_build.py" --source "$WORK/gimp" \
  --output "$WORK/pre-build-probe.json"
JOBS=4 bash "$ROOT/tools/build_legacy_reference.sh" "$WORK/deps" "$WORK/build" \
  "$WORK/gimp" "$WORK/babl" "$WORK/gegl" > "$WORK/rebuild.log" 2>&1
```

Check every exit status; do not continue after a failed acquisition, checkout or
build. The probe here deliberately omits --bootstrap so it leaves the build input
clean. For a bootstrap probe, use another disposable checkout. The wrapper checks
pinned HEAD and tracked cleanliness before applying its recorded patches. Use
fresh clones, since that check does not reject unrelated untracked files. After a
failed build, preserve the directory and logs; start another fresh directory for
a clean replay rather than reapplying patches over the changed tree. The script
has no per-stage resume contract and uses /usr/bin/gcc and /usr/bin/g++; record
and verify those host compiler versions/bytes against the existing build report.
The package lock does not make an arbitrary host or historical Flatpak runtime
bit-identical. Network acquisition may require an available authorized upstream
or verified cache; a blocked download is not a successful dependency repair.

The exact measured invocation and all stage command lines are in the replay
report and build-log archive. New paths produce a new run: keep its own source
pins, package verification, relocated-tool manifest, build logs and binary hash.
Do not replace failed historical logs with the new successful result.

### Existing environment: verify before reuse

An available environment can avoid another rebuild. Source its generated
build/env.sh with Bash (temporarily disable nounset if enabled), then verify the
installed binary hash, --version, package lock, dependency resolution, recorded
source patches and required plugins. Missing provenance makes it exploratory.
Keep installed original binaries separate from any later capture instrumentation.
Use fresh GIMP2_DIRECTORY profiles and copies of controlled inputs; never load
private original work in a destructive capture. A version string alone is not
source identity, and a matching executable hash alone does not verify its shared
libraries, plugins or user profile.

For an independent original-reader smoke check, adapt only the file path in
`../fixtures/legacy-filter-positive/independent-reopen.scm` into a new local copy.
Use the checked fixture from that package, not a user file. Record the adapted
script hash. With BUILD pointing at the verified build and SCRIPT at this copy:

```sh
set +u
source "$BUILD/env.sh"
PROFILE=$(mktemp -d)
GIMP2_DIRECTORY="$PROFILE" timeout 60 "$BUILD/prefix/bin/gimp-2.8" \
  --no-interface --no-shm --batch-interpreter=plug-in-script-fu-eval \
  -b "(load \"$SCRIPT\")" > "$PROFILE/reopen.log" 2>&1
status=$?
printf '%s\n' "$status" > "$PROFILE/exit-code"
```

Run that diagnostic block without errexit, so failure status is recorded, and
restore the caller's shell options afterward. Require exit 0 AND the explicit
INDEPENDENT_ORIGINAL_READER_REOPEN_PASS marker, with no batch execution error.
Retain the complete log and profile path. Version/startup alone does not test
painting or editable save/reopen. Reproduce the separate normal/CloneLayer,
positive FilterLayer, stroke, layer, rotation and timeline captures only by their
own recorded build/run contracts. Local `*-record.sh` scripts are execution
records with absolute paths, not universally relocatable entry points.

### Static fallback and unresolved gates

If dependencies, display, provenance or runtime behavior block a measurement,
retain the failure and continue source extraction from the pinned commit only.
`tools/freeze_legacy_assets.py` and `../fixtures/strokes/FORMAT.md` provide the
source-asset and input contracts. Label extracted formulas and Cairo/source-only
observations static; they never replace original writer/reader, GUI event or
asynchronous execution observations.

Available bounded evidence now covers 02.001–02.013; the task ledger and each
package define acceptance, not this recovery procedure. Rights and source notices
are reviewed under 02.014. 02.015 may be accepted for its written dependency-repair,
existing-environment and static-fallback procedure only after both 02.014 and
02.015/recovery-procedure dependencies are accepted. This review does not set task
status or grant redistribution rights. It does not require repairing every old
bug as a precondition for documenting recovery.

Unclosed runtime/port gates remain explicit: no pristine historical binary identity;
no green original GIMP test suite (the historical test-xcf startup critical abort
remains, and the second replay did not rerun it); no passing direct edge-reader
roundtrip; no clean rotation GUI shutdown; no physical mouse/tablet equivalence;
no universal cancellation, responsiveness/p95/p99 or all-feature scheduler pass;
no modern-port pixel/behavior parity from old-reference evidence alone. Startup
GValue criticals, obsolete GEGL cache warnings, plugin registration, font cache
and Script-Fu shutdown diagnostics remain retained in the corresponding logs.

The offline `test_legacy_recovery_procedure.py` checks the recorded recipe/lock,
archived commands and outcomes, and the positive/negative separation. It does not
execute GIMP or close any acceptance dependency.
