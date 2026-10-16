# Native Sharpen owner-context and XCF acceptance contract

Status: accepted for the bounded domain below on 2026-10-16. The focused native
and XCF tests, registered regression sets, and focused native/XCF ASan/UBSan
checks passed. The source-sealed receipt is
`migration/tests/native-sharpen-context.json`. This is not a full-suite,
installed-runtime or whole-port completion claim.

This contract extends the independently validated arithmetic unit described in
`legacy-sharpen-raster.md`. That document's earlier statement that the foundation
does not enable a FilterLayer route describes its original scope; the route and
persistence behavior specified here are a separate acceptance unit.

## 1. Executable domain and preserved definitions

The route recognizes the literal `plug-in-sharpen` name and exactly four typed
integer argument slots: recorded run mode, image slot, drawable slot and signed
INT32 percentage. The original values and raw definition are retained. Execution
uses the current owned lower-input snapshot, not saved image/drawable identifiers.
No isolated-PDB descriptor, allowlist entry, helper/wire-protocol revision or new
serialized version is introduced. The existing parameter editor is not enabled
for Sharpen by this work; unsupported editor state remains preserve-only.

Execution is bounded to native nonlinear U8 RGB or Gray. The FilterLayer itself
has alpha-bearing RGBA or YA output; no-alpha RGB/Y lower sources acquire the
alpha-bearing layout in the same way as the actual legacy FilterLayer. Worker
transport is straight, tightly packed RGBA8. Gray is replicated/extracted exactly;
its bytes are not passed through an incidental RGB color conversion. Input,
owner snapshots and native staging use the drawable's native color space.
Adobe RGB and Gray Lab TRC vector/spill checks establish this encoded-byte
behavior for those tested profiles, not an exhaustive profile-conversion claim.

The percentage is not restricted or normalized to the dialog's 1–99 range.
The supported signed interval is `100 - INT32_MAX` through `INT32_MAX`
(inclusive). Values at least 99 use the legacy factor of one. Defined zero and
negative percentages retain their original integer arithmetic and two rounding
stages. Values below that interval are unsupported because the old signed
subtraction overflows; no invented result is supplied.

An intersecting start ROI must be at least two pixels wide and three high.
Width-one or height-one/two intersections are unsupported because the pinned
implementation reads out of bounds or leaves rows unwritten there. The native
adapter admits positive full dimensions with width at most `INT32_MAX / 4`,
height at most `INT32_MAX`, and checked extent/reservation arithmetic. The
lower-level full-shadow wrapper has a separately broader full-width bound of
`INT32_MAX`, provided its actual intersecting crop satisfies the kernel limit
and the full RGBA extent fits signed 64-bit storage addressing. That wrapper
capability does not expand the application's admitted domain.

Wrong arity/types, undefined percentages, unsupported precision and unsupported
intersecting geometry must leave the recorded definition and last completed
cache intact. They must not silently coerce arguments, execute a different
filter, or publish an identity result as a successful Sharpen run. The focused
native tests cover linear U8, nonlinear U16 and nonlinear float rejection;
other higher-precision or indexed execution is not enabled or accepted here.

## 2. Owned input, zero shadow and start selection

The shared `FilterOwnerContext` captures complete current owned input and the
start selection bounds on the owner thread, in bounded chunks. The neutral
start gate supplies a plain `FilterSelectionRegion` without depending on an
isolated-PDB request. The existing isolated request overload retains its own
raw-shadow/background handling. A distinct plain region carrier belongs to each prepared Sharpen configuration.
The owner populates and seals it before worker launch; it stays immutable during
worker access and is reused only after prior worker completion. Workers receive
dimensions, region, options, storage and progress, never image/selection/drawable
GObjects.

The raw operation has these required invariants:

- No selection means the full drawable. A nonempty selection is intersected
  with the drawable using its image offsets; it may have no intersection.
- Validate ordered half-open ROI coordinates, selection/intersection consistency,
  dimensions, percentage, exact backing sizes and direct input/output aliasing
  before I/O. Distinct raster objects must also have independent backing; that
  indirect-alias exclusion remains a caller obligation.
- Zero the entire private full-raster output, including alpha, before processing
  the captured ROI. Zero-fill writes are at most 16 KiB and cancellable.
- Read only the captured crop. Crop transfers split at crop-row boundaries, and
  the bounded kernel's strip halos never reach outside the captured ROI. Copy
  the first/last crop rows and columns exactly. Hidden color participates in
  filtering, and the kernel preserves alpha within the crop.
- Leave every raw byte outside the start ROI zero. Copying input into that
  region would change genuine late-selection-expansion/clear behavior.
- A nonempty start selection wholly outside the drawable is `no_merge`, even
  if selection later clears or expands. Its initialized private shadow is not
  merged. The final result preserves the owned input snapshot.

Component activity, soft selection weights and alpha lock do not affect the
raw kernel. They belong to the final owner merge. The genuine corpus contains
pre-merge shadow and mask bytes supporting this distinction; the modern live
comparison gate checks final output bytes.

## 3. Final merge gate and context changes

After raw processing, the owner captures the current final selection and then
seals component activity and the target layer's own alpha lock. Ancestor alpha
lock is not substituted for the target's lock. Gray uses its corresponding
color/alpha activity bits and the established native-byte replacement merge.

Dirty selection capture restarts before sealing, including bounded mask copying.
A known empty selection can seal in the transition quantum. Once sealed, the
snapshot remains fixed for that staged import: subsequent selection, component
or lock changes do not rewrite already staged chunks or mutate that generation's
merge. A subsequent lower-input invalidation obtains a fresh context. Context-only
changes in these tests do not manufacture a new generation or duplicate run.

The final selected mask is captured in drawable-local coordinates, with checked
image offsets. An unrestricted final selection merges over the full drawable;
a nonempty final selection outside the drawable preserves the original pixels.
The earlier `no_merge` start disposition takes precedence over either condition.
The merge always combines the original owned input and raw shadow using the
sealed final context, not a mixture of old and current lower input.

## 4. Private cache, lifetime and resource admission

The existing scheduler owns generation/lifetime checks and publication. All
raw output and partially imported GEGL staging remain private. Only a completed
current-generation result may replace the completed cache. Cancellation,
storage failure, definition replacement, lower-input change or owner closure
must not expose partial output or let stale work overwrite the cache. The old
completed buffer, cache generation and stored definition remain available when
a run fails or is cancelled. A later valid invalidation may recover normally.

The storage wrapper checks cancellation around I/O, propagates storage errors,
and reports success only after flush and the final cancellation check. Raw
raster failure may leave partial private bytes. The vector wrapper publishes
transactionally and leaves its caller's output unchanged on failure; vector
input/output aliasing is supported through that private transaction. Progress
covers zero-fill and kernel work monotonically and reaches one only on success.

Admission accounts for the whole route, not just the bounded kernel:

- Owner input snapshot, possible final mask and native staging cost a logical
  5 bytes/pixel for Gray and 9 for RGB.
- The vector route adds two full RGBA rasters, 8 bytes/pixel, and a 2 MiB
  scratch/owner-merge allowance.
- Above the 4 MiB RGBA raster threshold, the spill route uses two raster files,
  six bounded transport chunks and the same scratch allowance. Its logical
  spill reservation includes both RGBA rasters and owner obligations: exactly
  13 bytes/pixel for Gray or 17 for RGB.
- Geometry and reservation arithmetic are checked before a runnable request is
  installed. Admission refusal must precede full owner capture/worker launch
  and preserve the cache and definition.

The 1031×1021 exact-quota tests reject RGB at 17,895,066 bytes and accept at
17,895,067; Gray rejects at 13,684,462 and accepts at 13,684,463. Sparse oversized
Gray/RGB refusal tests read/write only a one-pixel cache sentinel before rejection.
These are logical admission results, not process-RSS, peak-memory, GEGL cache/swap
or latency measurements. Existing GEGL caches remain subject to host policy.
Declared extents above 4 GiB are validated without actual high-offset execution;
there is no corresponding very-large-I/O claim.

## 5. Strict old-writer capsule and unknown preservation

The old-writer admission rule comes from actual files, not a proposed format.
The pinned writer produced twenty Sharpen XCF files across four lower-source
layouts and percentages 10, 0, -100, 99 and `INT32_MAX`. Each has a 68-byte
capsule, the exact `plug-in-sharpen` bytes including terminal NUL, and these five
contiguous records:

1. INT32 run mode: tag 2, payload size 4
2. Omitted unsupported image: tag 1, payload size 0
3. Omitted drawable: tag 10, payload size 0
4. INT32 percentage: tag 2, payload size 4
5. END: tag 0, payload size 0

The loader requires successful decoding, exact outer and retained length,
exact original name encoding, exactly five records, exact tags/kinds/declared
and payload sizes, contiguous encoded offsets/lengths, no length mismatch and
the expected terminal position. A repaired display name is insufficient.
Only this demonstrated schema yields executable arguments. The separately
materialized model receives context placeholders for the omitted image/drawable;
original run-mode/percentage bits and the complete retained capsule are unchanged.
A structurally valid capsule can still contain an unsupported execution value;
materialization does not bypass the domain checks above.

Unknown or malformed definitions remain inert and byte-preserved with their
completed pixels/provenance. They must not be upgraded into guessed executable
arguments, rewritten to UI defaults or silently dropped. The focused malformed
checks cover float run-mode/percentage tags, swapped context-slot kinds,
percentage/END length mismatch, missing or embedded name NUL, outer length
mismatch, and extra/missing argument records. These eleven variants are
manufactured rejection tests, not old-writer observations. They prove retained
raw bytes/cache and zero executions on Open and lower-input invalidation;
they do not separately establish malformed-file Save/reopen coverage.

For all twenty genuine old-writer files, the tested lifecycle is ordinary Open,
loaded cache with zero runs, duplicate, native Save, destruction of the original
image, ordinary reopen with unchanged definition/cache and zero runs, then
changed-lower-input recomputation against the captured second output. Definition
Undo/Redo carries the integer value and raw capsule together; redo is compared
with genuine alternate-percentage second-output bytes. This establishes this
bounded Sharpen persistence path, not general acceptance of every legacy XCF
extension or unknown payload.

## 6. Evidence hierarchy and genuine capture limits

The reference is GIMP Painter commit
`afa43fae3e920210146abed514f136fd49f671b5`, using unchanged Sharpen, shadow merge
and XCF writer implementation sources. `migration/fixtures/legacy-sharpen-README.md`
is the reconstruction/provenance guide. The sealed archives are:

- `legacy-sharpen-context.tar.xz`: 186 cases, comprising 114 synchronous PDB
  observations and 72 actual FilterLayer scenes, including 20 old-writer files.
  SHA-256: `d54b50fe358aa5915303f4ea92deb26fec6df90113d7fe4b0b06b2706e5120b5`.
- `legacy-sharpen-large.tar.xz`: two additional 1031×1021 YA/RGBA FilterLayer
  scenes, hard selection changed at the original final progress call to empty
  or soft, each followed by a changed-lower-input rerun.
  SHA-256: `afd9483df4cff23c4ffb80d594f9468cb8e76cfe20cf04fda38e43f2190d2acd`.

Together there are 188 scenes/observations and 74 actual live FilterLayer scenes.
The modern live test compares 116 exact final outputs across those 74 scenes,
including the large spill reruns. It does not execute 114 modern direct-PDB
comparisons, and the 114 old PDB observations must not be added to the modern
live parity count. Raw Y/RGB observations belong to direct PDB or lower-source
layouts; old FilterLayer output is always YA/RGBA.

Captures include native input/shadow/masks/output, changed lower input, build and
runtime logs, source/binary hashes, scripts and observation hooks. Hooks record
buffers and apply specified late context changes before the actual legacy merge;
the fixtures are not pixels calculated by the new implementation. Source and
binary hashes were checked again after capture. A repeat receipt records 2,126
raw artifacts and all twenty XCF files byte-identical to the preceding capture.

The asynchronous FilterLayer harness cannot observe a PDB return value. Its
corrected status field is `-1` (unavailable), with actual start/end/merge counts
recorded separately. The earlier initialized, unobserved live status is disclosed
in the repeat receipt. Synchronous PDB status is genuinely observed. These limits
must not be recast as successful live PDB return codes.

The old runtime emits two startup `g_value_set_object` GObject criticals and a
missing fresh-profile tag-cache message before cases run. Their text and order
match previously sealed reference corpora. Full diagnostics remain in the
captures; this is not warning-free legacy-runtime acceptance. The capture report
records no additional case-time warning, critical, crash or timeout. Undefined
percentages and width-one/height-below-three executions were deliberately not
captured. No result is inferred for those cases.

`tools/check_sharpen_evidence.py` verifies pinned archive/manifest/report hashes,
member safety, provenance, case counts and bounded native extents. It extracts
into a separate consumer directory and constructs a 188-row combined TSV without
rewriting either original receipt. The native launcher supplies that verified
directory via `GIMP_PAINTER_SHARPEN_FIXTURES`; missing evidence fails rather than
silently skipping the registered comparisons.

## 7. Inspected acceptance results

The evidence directory for this review is
`/workspace/shared/gimp-sharpen-context-evidence`. The second native source seal,
`native-second-seal.json`, has 24 recorded inputs, all matching the reviewed tree.
`native-second-results.json` records exit zero for both focused commands. Full
Meson logs are under `/workspace/shared/gimp-profile.0SY6Brlg/build/meson-logs/`
with bases `sharpen-context-second` and `sharpen-xcf-second` (`.txt`, `.json` and
`.junit.xml`). These are run receipts for this checkout, not portable proof of a
future modified tree. Relevant source/evidence changes require revalidation.

The focused `gimp-filter-layer` run passed all eleven selected paths:

- `sharpen_integer_native_route`: ordinary/noncanonical signed values and width-two
  cases; original integer slots/raw definition retained
- `sharpen_invalid_definitions_keep_cache`: four wrong scalar types, too few/many
  arguments and undefined percentage, without losing the completed cache
- `sharpen_unsupported_geometry_precision`: unsupported full/cropped geometry and
  representative unsupported precisions, without worker execution
- `sharpen_late_context_routes`: vector/spill late full/empty/soft selection,
  components, own alpha lock and ancestor-lock distinction
- `sharpen_sealed_context_routes`: frozen final context and fresh context on rerun
- `sharpen_outside_selection_no_merge`: start outside remains no-merge despite a
  late clear, followed by a fresh lower-input rerun
- `sharpen_cancel_replace_lower_close`: interruption across waiting/preparing,
  vector running and import-entry phases, retaining the prior cache
- `sharpen_actual_old_live`: 116 exact comparisons across 74 genuine live scenes
- `sharpen_native_profile_bytes`: four Adobe RGB/Gray Lab TRC vector/spill cases
- `sharpen_admission_preserves_cache`: oversized and exact Gray/RGB quota cases
- `sharpen_partial_import_lifecycle`: eight vector/spill cancellation, definition
  replacement, lower-input change and closure actions after actual staged chunks

The spool may move directly between phases in one owner quantum, so the lifecycle
test intentionally makes no timing-dependent spill `RUNNING` assertion. Separate
partial-import checks interrupt at observed progress strictly between 0.85 and
1.0; the inspected log records approximately 0.916–0.925. They verify every byte
of the retained old cache, not just the first pixel or import-entry transition.

The focused `painter-xcf-open` run passed both selected paths:
`sharpen_old_writer_roundtrip` (twenty real files) and
`sharpen_malformed_writer_capsules` (eleven manufactured variants).
Modern focused logs retain expected profile `.dummy` plug-in skip messages and
GLib environment-thread debug lines; neither run is described as silent startup.
Both returned zero with all selected TAP cases successful.

`shadow-normal.json` and `shadow-sanitize.json` separately report successful
non-UI raw-shadow wrapper runs: 648 source-C/vector/spill comparisons plus 344
read/write/flush cancellation/storage-error ordinal cases each. They also cover
ROI edges, offsets, no-selection/outside distinction, validation, progress,
source immutability and transactional aliases. Their seals report no changes
after compile/run. The sanitizer run used ASan/UBSan with leak detection disabled.
This source-C arithmetic/storage evidence is distinct from actual old-runtime
capture and current native application execution. It does not establish native
owner/XCF sanitizer coverage or any leak result.

The subsequently inspected `registered-acceptance.json` records both registered
regression sets as passed, with zero skips, crashes or unexpected diagnostics:

- `sharpen-context-independent`: seven executables covering raster, raster
  kernels, native kernels, kernel progress, Sharpen foundation, full-shadow
  wrapper and context.
- `sharpen-context-regression`: complete `gimp-filter-layer` (160 TAP cases,
  including existing isolated Blinds/Small Tiles/Retinex/Convolution routes),
  `painter-xcf-open` (16), `painter-xcf-roundtrip` (49),
  `painter-filter-scheduler` (39), `painter-filter-owner-gates` (5), and the
  non-TAP `painter-filter-context-native` and `painter-filter-import-completion`.

Their full Meson logs use those bases in the same log directory. The XCF
roundtrip run retains expected negative-case warnings about unsupported or
malformed metadata/effects. Successful diagnostic classification does not mean
empty stderr, and these selected regression sets are not the whole project
aggregate.

The focused native FilterLayer/XCF ASan/UBSan run subsequently passed all eleven
native paths and both XCF paths, repeating all 116 genuine pixel comparisons.
It rebuilt 36 explicitly instrumented translation units and 35 separately
identified RTTI-only units into private objects, archives and two executables.
Remaining GIMP and dependencies were uninstrumented; LeakSanitizer was disabled.
All 3,040 source/dependency seals, 113 ordinary-build seals, two private executable
seals and 2,260 fixture seals were unchanged. Ordinary seals cover selected
objects and archive containers, not every unmodified thin-archive member. No
unexpected runtime or sanitizer diagnostic was found.

The first sanitizer attempt is retained as a failed preflight with zero commands
and thirteen tests not run: its canonical-command selector did not recognize a
nested production library. The corrected selector uses actual harness link and
archive membership, includes the nested libraries and excludes the unrelated
standalone worker executable. Fourteen portable runner regression checks and
the native retry passed. The failed attempt is not counted as test coverage.

Full aggregate, installed runtime, Windows/macOS, UI/editor behavior, exhaustive
profiles, LeakSanitizer, total-RSS/latency measurements and actual I/O above 4 GiB
remain outside this bounded acceptance claim.

## 8. Reproducing the focused checks

The ordinary configured native build registers `painter-filter-sharpen`,
`painter-filter-sharpen-shadow`, `gimp-filter-layer` and `painter-xcf-open`.
The two app targets run through
`migration/tests/run_filter_owner_context_fixture_test.py`, which verifies and
materializes the committed genuine fixture archives. Use a valid authenticated
X display and the configured dependency environment. The focused paths listed
above can be selected with Meson's `--test-args` and repeated GLib `-p` flags;
`--no-rebuild` is appropriate only after a successful build of that source.

The standalone raw-shadow check is reproducible with:

```
flock "$BUILD_LOCK" python3 -B app/painter/tests/run_filter_sharpen_tests.py \
  "$SCRATCH_BUILD" --shadow --report "$REPORT"
# Repeat in a fresh output directory with --sanitize for focused ASan/UBSan.
python3 -B migration/tests/test_sharpen_context_sanitizers.py
```

The standalone runner's `--shadow` mode selects the full-raster wrapper corpus;
its sanitizer mode is separate from native owner-context acceptance. For the
native checks, `migration/tests/run_sharpen_context_sanitizers.py --help` documents
required build directory, report path, exclusive inherited lock FD and exact
native/XCF test paths. Run it inside the authenticated display and verified
fixture wrappers, opening the lock FD only after those wrappers, since they do
not forward inherited descriptors. Each selected test runs in a separate
process, and a missing, skipped, duplicate or mismatched TAP result fails the
runner. Private objects, archives and executables leave the ordinary build
artifacts untouched.
