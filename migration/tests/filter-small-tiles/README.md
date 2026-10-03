# Small Tiles isolated route acceptance

WBS **16.003/isolated-small-tiles-route** passed on 2026-10-03. The original
saved `plug-in-small-tiles` name and four typed arguments remain editable. The
hidden noninteractive adapter preserves factors 0..6, including factor 0's
actual merge and selected-rectangle top-left sample. The public 2..6 entry and
UI retain their previous behavior. The route shares the accepted owner context,
BindingStore, admission, independent worker, completed-cache publication and
installed locator. Retinex and XCF multipart are outside this task.

## Checked behavior

- 196 genuine old PDB outputs match actual current isolated-helper output across
  RGB/RGBA/Gray/Gray-alpha, seven geometries and factors 0..6
- 25 actual old native scenes yield 50 matching current FilterLayer outputs:
  48 native merges and two no-merge generations. The latter publish each newly
  prepared lower input, including hidden color, instead of retaining old cache
- Fresh current PDB calls preserve all 24 public outputs and full argument
  metadata against the retained unmodified modern baseline. All 196 hidden calls
  match old output after the explicitly documented old alpha-zero merge repair;
  both interactive and last-values hidden invocation return calling errors
- Nine ordinary Filter targets pass with no skips/timeouts: 114 native tests,
  320 old Blinds comparisons, 12 identities, 72 process cases, 37 scheduler cases,
  11 spool cases, five owner gates, one completion race and 304 old scalar
  context merges/bounds. A final 114-native rerun includes explicit post-reopen
  factor 0↔1 re-edits and compares every nonuniform output pixel
- New native tests cover lower updates, invalid factors/types retaining saved
  bytes and cache, native ROI/soft selection, late active components and effective
  own alpha lock, cancellation/replacement, dependency ordering, owner close,
  Save/reopen and undoable parameter re-edit. Analytic checks are separate from
  the actual-old fixture counts
- Focused ASan/UBSan/float-cast-overflow passes on 29 instrumented and 41 RTTI-only
  sources, with 1,736 unchanged sealed inputs, five unit targets, 196 SmallTiles
  helper comparisons, 12 Blinds identities and 31 native cases. Other GIMP and
  dependencies are ordinary; LeakSanitizer is off
- Seventeen real executable-location tests pass normally and under sanitizers,
  covering both literal plug-ins, moved install, symlink, absent/nonexecutable
  files, invalid selectors and the compile-time instrumentation overlay

## Installed Linux prototype

The ordinary default build has 1,005 configured targets; 211 ELF executables
were checked without missing or invalid outputs. Two optional nondefault test
executables remain unbuilt. The fresh runtime has 5,270 manifest entries
(5,221 regular files and 49 symlinks), including 419 ELF objects. Every entry
was reverified unchanged in the original runtime and two relocated copies.
Each relocated tree also gained 27 ordinary CPython cache files mapped to sealed
Python sources; this is not a claim that execution writes no additional files.

Both factor 3 and factor 0 smokes run with an explicit minimal environment from
fresh paths containing spaces and Japanese characters, while the build remains
accessible. Each observes two actual installed GPF3 helpers and two native
`tile-small -run` children, compares complete genuine-old live rasters and
checks four typed arguments through two Save/reopen cycles. No wrong-runtime
process, observed survivor or recorded private profile remains. The core smokes
took 7.317 s and 6.442 s. The final 4,314-file source inventory and ordinary binary hashes stayed
unchanged. This is a dirty prototype, not a release build or full aggregate pass.

The first installed attempt is superseded: its fixture requested a modern
unsupported layer mode, causing a critical diagnostic and fallback. The final
fixture uses NORMAL_LEGACY, and its checker rejects that diagnostic; both final
runs pass without it. Only these two test/provenance inputs changed after the
native and sanitizer gates; production Filter bytes are unchanged. A separate
strict no-additions scan failed solely on the 27 Python caches; its unchanged
record is retained locally, and the qualified seal records those additions.

## Evidence and reproduction

`acceptance.json` links the compact current reports, exact task-source hashes
and commands. Full local logs/source archives are retained outside the tracked
repo; compact records keep their hashes instead of duplicating repeated startup
diagnostics. `../../fixtures/small-tiles-evidence.tar.gz` is
an explicitly derived exact-byte subset of preserved historical captures. Its
README/manifest retain original provenance and original archive/member hashes;
none of those reports were rewritten as a new capture.

From the isolated source checkout, using fresh output directories:

```sh
source /workspace/scratch/5b5281e79681/gimp-build-restoration/env.sh
export GIMP_BUILD_PREFIX=/workspace/scratch/5b5281e79681/.prefix-installed-filter
source tools/linux-debian13-env.sh
export GIMP_PAINTER_BUILD_LOCK=/workspace/scratch/5b5281e79681/gimp-painter-build.lock
flock "$GIMP_PAINTER_BUILD_LOCK" meson compile -C build-installed-filter -j 4
flock "$GIMP_PAINTER_BUILD_LOCK" meson test -C build-installed-filter --num-processes 1 --print-errorlogs painter-filter-procedure painter-filter-process painter-filter-wire painter-filter-scheduler painter-filter-spool painter-filter-context painter-filter-owner-gates painter-filter-import-completion gimp-filter-layer
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/capture_small_tiles.py --build build-installed-filter --output ../small-tiles-current-pdb-new
python3 migration/tests/run_filter_process_sanitizers.py --build build-installed-filter --output ../small-tiles-sanitizers-new --run --lock "$GIMP_PAINTER_BUILD_LOCK"
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/package-linux-runtime.py --build build-installed-filter --deps "$GIMP_DEPS_DIRECTORY" --output ../small-tiles-package-new
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/check_installed_small_tiles.py --build build-installed-filter --bundle ../small-tiles-package-new/gimp-painter-linux-x86_64 --factor 3 --relocate --output ../small-tiles-installed-new-3
flock "$GIMP_PAINTER_BUILD_LOCK" python3 tools/check_installed_small_tiles.py --build build-installed-filter --bundle ../small-tiles-package-new/gimp-painter-linux-x86_64 --factor 0 --relocate --output ../small-tiles-installed-new-0
```

## Limits retained

U8 nonlinear RGB/Gray is the accepted domain. Conservative 1 GiB memory
admission can reject large extents despite swap; configured spill defaults to
8 GiB and is logical admission, not reserved disk space or total-RSS proof.
Disabled swap preserves a failure/cache rather than providing another route.
Global latency, owner UI progress, remaining procedures, Windows/macOS, whole
application/GUI acceptance and release distribution remain open. Ordinary
startup resource warnings and two stale contexts at host exit remain in the
logs; no warning-free, whole-app sanitizer or leak-free claim is made.
