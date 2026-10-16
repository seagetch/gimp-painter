# Native Threshold Alpha owner context and XCF contract

Status: the bounded native owner-context and XCF acceptance checks below
passed on 2026-10-16. The final ordinary run passed 15 focused native paths,
3 focused XCF paths and 12 registered regression targets. Focused native/XCF
ASan/UBSan subsequently passed all 18 selected paths. Standalone raw-shadow
normal and ASan/UBSan checks passed separately. This is not a full-project
aggregate, installed-runtime, whole-port or whole-program sanitizer acceptance
claim.

The source-sealed acceptance receipt is
`migration/tests/native-threshold-alpha-context.json`. It records the bounded
ordinary and sanitizer evidence for the tested checkout, not portable proof for
a future modified tree. Relevant source or evidence changes require revalidation.

## 1. Executable domain and definition preservation

The new owner-context route recognizes the literal `plug-in-threshold-alpha`
name only for native nonlinear U8 RGB or Gray. It requires exactly four typed
integer slots: recorded run mode, image slot, drawable slot and signed INT32
threshold. The original values and raw definition remain retained. Execution
uses the current owned input, never saved image or drawable identifiers.

Every signed INT32 threshold is accepted without clamping to a dialog range.
For each processed pixel, alpha becomes 255 exactly when `threshold < alpha`,
and otherwise becomes zero. Equality therefore clears alpha. Negative values,
including INT32_MIN, set every processed alpha byte to 255; thresholds at least
255, including INT32_MAX, clear every processed alpha byte. RGB or Y color bytes
remain unchanged inside the captured start ROI. This reuses the existing native
point arithmetic rather than introducing an alternative threshold formula.

The name is claimed before argument validation on this byte route. Malformed
nonlinear-U8 Threshold definitions must fail with the definition and completed
cache retained; they cannot fall through to the older unrestricted point route.
The native tests cover wrong types in all four slots and too few or too many
arguments. Indexed execution is not enabled.

The alpha-bearing output is RGBA or YA. No-alpha RGB/Y lower sources are promoted
through the owned projection as in the genuine legacy FilterLayer. Worker
transport is tightly packed, straight RGBA8; Gray is replicated and extracted
exactly, rather than converted through an incidental RGB color space. Owner
input, cache prefill and native staging use the drawable's encoded native space.
Vector and spill checks cover Adobe RGB and Gray Lab TRC, without claiming
exhaustive profile-conversion coverage.

The existing `real_samples()` point route remains separate. That predicate is
true for every precision other than nonlinear U8, including linear and perceptual
U8 as well as higher precisions. Those routes retain their 32-byte RGBA-double
transport, existing typed compatibility and context-free behavior. They do not
acquire this byte route's selection, component, alpha-lock or projection-history
semantics. Regression checks exercise 17 component/TRC combinations in RGB and
Gray plus two real-sample spill cases, 36 cases total, including fractional alpha
and ignored context-slot compatibility. This preserves the existing extension;
it does not establish legacy U8 parity for those other representations.

No general PDB descriptor, isolated helper allowlist, wire protocol, serialized
version or parameter-editor activation is added. Broader native-point-precision
work remains a separate task.

## 2. Owned input and parent projection history

The shared `FilterOwnerContext` captures the complete owned input and start
selection bounds on the owner thread in bounded chunks. The prepared plain
`FilterSelectionRegion` belongs to that configuration, is populated before worker
launch and is immutable during worker use. Workers receive plain dimensions,
region, threshold, storage and progress data, not image, selection or drawable
GObjects.

The original application's `project_region()` copied only the intersection of
the immediate parent projection into the existing FilterLayer drawable. Neither
that copy nor `filter_reset()` cleared pixels outside the copied rectangle.
Consequently, prior completed pixels, including loaded cache bytes and prior
threshold results, remain part of the next input outside that intersection.
Always clearing such pixels to zero would be incorrect.

For this nonlinear-U8 Threshold route, each already-admitted native input chunk
is first read from the committed owner buffer in drawable-local coordinates.
Only the intersection with the current immediate parent projection is then
blitted from the lower input proxy in image coordinates. A top-level filter uses
the image rectangle. A group child uses its parent's actual offset and extent;
a normal group's child-union bounds can extend outside the image and expand
when only the filter moves. This uses the same existing chunk and introduces no
additional pixel allocation or worker-side GObject access.

This correction is deliberately route-scoped. Other byte operations and the
preserved real-sample route retain their previous input policy; this unit does
not claim their general projection-history parity.

The separate genuine 16-scene history supplement proves all 48 observed starts
equal the previous drawable cache with precisely the observed parent projection
intersection overwritten. It includes YA/RGBA, top-level/group, positive/negative
overflow and fresh/seeded cache. Images are 13×11; source and owner are 17×13.
Thresholds are -1, 127 and 127. Before the third run only the owner moves to the
opposite offset; the lower source is stationary and byte-identical. Eight seeded
controls use the genuine mark-as-loaded API and retain their cache with zero
startup jobs. These seeded controls are not actual XCF reopening observations.

Measured history evidence includes 888 top-level run-two outside-projection
pixels retaining alpha 255. After movement, run three retains 380 alpha-zero and
508 alpha-255 pixels outside its new intersection. Ordinary group projections
contain 2,664 run-pixels outside the top-level image canvas. The modern consumer
checks 16 distinct scenes, 48 final outputs, 48 before-copy cache observations,
8 loaded-idle controls and all 48 parent/copy rectangles separately from the
original 174-scene corpus.

## 3. Start ROI and full raw shadow

No selection means the full drawable. A nonempty image selection is intersected
with the drawable using its image offsets and may have no intersection. Ordered
half-open coordinates, selection/intersection consistency, dimensions, exact
backing extents and direct raster aliasing are validated before I/O. Distinct
raster objects must also have independent backing; indirect alias exclusion is
a caller obligation.

Full dimensions must be positive and at most INT32_MAX, and the full RGBA byte
extent must fit signed 64-bit addressing. Application reservation arithmetic is
also checked before installing a runnable request. The actual intersecting ROI
must have at most INT32_MAX pixels, reflecting the original iterator's signed
`total_area`. This area restriction applies to the captured crop, not an
unrelated full raster with a small or outside selection.

Every positive intersection is valid, including 1×1, 1×N, N×1 and 2×2. Validation
is route-specific: Threshold's tiny support does not weaken Sharpen's existing
minimum intersecting width-two and height-three requirement. Native tests accept
the tiny Threshold crops and reject the same unsupported Sharpen crops.

The raw wrapper zeroes the entire private full-raster output, including alpha,
before filtering only the captured crop. Full-raster transfers are at most
16 KiB; crop views also split transfers at row boundaries. There is no scratch
raster. Every raw byte outside the captured start ROI stays zero, even if final
selection later expands or clears. Copying the original input there would change
the genuine late-selection result.

A nonempty start selection wholly outside the drawable yields `no_merge`, not
an ordinary zero-shadow merge. Its initialized private shadow is not merged,
even if final selection clears or expands. The final result preserves the owned
input snapshot. The actual wrapper outcome is carried to final import rather
than guessed from a later selection state.

## 4. Final context and asynchronous publication

After raw processing, the owner captures the current final selection, component
activity and the target layer's own alpha lock. Ancestor lock is not substituted
for the target's lock. These controls affect the existing native-byte final
replacement merge, not the raw point kernel. A nonempty final selection outside
the drawable preserves original pixels; an empty final selection is unrestricted.
The captured start `no_merge` disposition takes precedence.

Dirty selection capture restarts before sealing, including during bounded mask
copying. A known empty selection may seal in the transition quantum. After the
context is sealed, later selection/component/lock changes cannot rewrite staged
chunks or alter that generation's merge. A later lower-input invalidation obtains
fresh context. Context-only edits in the tested idle and sealed cases preserve
generation, run count and completed cache until such a notification occurs.
Unchanged-byte lower notifications are tested separately from changed input;
the tests do not disguise an unchanged notification as a source rewrite.

Execution is asynchronous and in-process. Raw computation runs on a scheduler
worker thread using its private input/output vectors or spool-backed rasters.
Owner-thread import merges against the captured owned input into a separate
private GEGL staging buffer. Neither raw worker output nor a partially imported
buffer is the visible completed drawable buffer. Only a completely imported
result whose generation is still current may replace the completed buffer on
the owner thread. This buffer separation prevents progressive pixel mixing in
the visible cache; it is not operating-system process or crash isolation.
Replacing one complete drawable buffer also does not promise whole-screen
compositing or display-vsync atomicity.

Cancellation, storage failure, definition replacement, lower-input change and
owner closure must not publish partial or stale results. The prior completed
cache remains available through waiting, preparation, worker execution and
partial import. Rejected definitions retain their recorded values/raw bytes;
failed or cancelled runs do not replace the completed cache generation. A later
valid invalidation can recover.

The wrapper checks cancellation around I/O, propagates storage errors and reports
success only after flush and a final cancellation check. A failed raw raster may
contain partial private output; the vector wrapper publishes transactionally and
leaves its caller's output unchanged on failure. Vector input/output aliasing is
supported through that private transaction. Raw progress is monotone across
zero-fill and crop work; the raster wrapper sets one only after flush and its
final cancellation check. Vector publication and native cache publication remain
separately cancellation/generation gated, so a later cancellation can still
prevent publication after raw progress reaches one. Native progress timing is
not claimed identical to the old plug-in's progress requests.

## 5. Logical admission and bounded working storage

Admission covers the complete route rather than only the point kernel:

- Native owned input, possible final mask and private native staging reserve
  5 bytes/pixel for Gray or 9 bytes/pixel for RGB.
- Vector transport adds two full RGBA rasters, 8 bytes/pixel.
- Bounded owner capture/import staging reserves 8 × 32,768 bytes. This is in
  addition to the wrapper's 16 KiB zero array and the existing point kernel's
  32 KiB staging array, for a combined fixed allowance of 304 KiB.
- Above the existing 4 MiB RGBA raster threshold, the spill route substitutes
  two raster files and additionally charges six RGBA transport chunks of
  32,768 pixels, 768 KiB. Its logical spill reservation is exactly
  13 bytes/pixel for Gray or 17 bytes/pixel for RGB, including owner obligations.
- Spool storage uses the trusted configured application swap directory, not a
  path embedded in a saved filter definition. Geometry/reservation refusal must
  occur before full owner capture or worker launch.

At 1031×1021, Gray rejects a 13,684,462-byte spill quota and accepts 13,684,463;
RGB rejects 17,895,066 and accepts 17,895,067. Sparse oversized refusal checks
use only a one-pixel cache sentinel before rejection. Completed cache identity,
cache generation, raw definition and typed arguments are retained.

These are logical admission and bounded-transfer claims, not measurements or
limits for whole-process RSS, peak allocation, GEGL cache/swap usage or latency.
Existing completed GEGL caches follow host policy. Validation of very large
declared extents does not establish actual high-offset I/O above 4 GiB.

## 6. Strict old writer capsule and persistence

Twenty-eight actual old-writer XCF files independently established the accepted
76-byte capsule, covering Y/YA/RGB/RGBA lower layouts and thresholds INT32_MIN,
-1, 0, 127, 255, 256 and INT32_MAX. The exact original name bytes are
`plug-in-threshold-alpha` including the terminal NUL. Five contiguous records
follow: INT32 run mode `(tag 2, size 4)`, omitted image `(1,0)`, omitted drawable
`(10,0)`, INT32 threshold `(2,4)`, and END `(0,0)`.

The loader requires successful decoding, exact outer/retained size, exact name
encoding and length, exactly these five argument records, exact tags and kinds,
declared/payload sizes, contiguous encoded offsets/lengths, no length mismatch
and the expected terminal position. Repaired display names or approximate
shapes are insufficient. Only that measured schema materializes executable
arguments; current context placeholders replace omitted IDs in the separate
model. Original signed run-mode/threshold bits and retained capsule stay intact.
The ordinary route admission checks still apply afterward.

Unknown or malformed near matches remain inert with their original definition
and completed cache. Eleven manufactured variants cover float run-mode/threshold,
wrong image/drawable tags, threshold/END length mismatch, missing/embedded name
NUL, outer length mismatch and extra/missing records. They establish preserve-only
Open and lower-input invalidation with zero executions. They are not genuine
old-writer observations and do not separately prove malformed Save/reopen.

All 28 genuine files pass ordinary Open with loaded cache and zero runs,
duplication, native Save, destruction of the original image, reopen with unchanged
raw definition/cache and zero runs, followed by changed-lower recomputation
against genuine second-output bytes. The newly executed cache also crosses a
Save/close/reopen image epoch with zero unsolicited runs. Definition Undo/Redo
carries the signed value and raw capsule together; redo is compared against a
genuine alternate-threshold output.

Four additional native lifetime cases cover RGB/Gray Save while vector RUNNING
or spill IMPORTING, reopen as pending rather than resuming a serialized worker,
retain the completed-cache sentinel, close the original image, recompute in the
new image epoch, then Save/reopen the completed result. Those cases use synthetic
native data and representative sentinel pixels; they are not additional genuine
old-writer evidence. They establish the tested stale-cache/pending persistence
behavior, not general legacy XCF or whole-image thread safety.

## 7. Genuine evidence and provenance

The reference implementation is GIMP Painter commit
`afa43fae3e920210146abed514f136fd49f671b5`. Capture harnesses invoke its unchanged
Threshold Alpha executable, shadow merge and writer. Observation-only hooks
record actual copy/start/progress/merge/end events and actual buffers. Fixtures
are not outputs computed by the port.

The original two sealed corpora contain 258 cases: 256 base cases comprising
84 synchronous PDB invocations and 172 live FilterLayer scenes, plus 2 large
1031×1021 live scenes. The combined modern live consumer requires exactly
174 scenes and 256 final-output comparisons. It includes 28 changed-input reruns,
54 unchanged-byte notification reruns, 24 pre-merge and 26 mid-work late-context
scenes. Twelve base cases also make idle context-only changes with no new run.
The 84 original PDB invocations are not modern direct-PDB executions and must
not be added to the modern live comparison count.

One-tile old progress 1.0 pre-merge observations are distinguished from genuine
mid-work progress strictly between zero and one; the base mid-work scenes record
0.25. The old iterator's sparse progress schedule has no unconditional final
1.0 update. Async live status is recorded as -1, unavailable; actual start/end/
merge counts establish execution instead. Synchronous PDB status is observed.

The separate history supplement adds 16 live scenes and 48 outputs without
rewriting the original corpora or relaxing the 174/256 gate. Its measured copy
and history behavior is described above. The preliminary base adapter gate
compared 334 invocations through both vector and raster adapters: 328 raw-shadow/
final-merge cases and 6 outside no-merge cases; two observed no-alpha PDB
rejections remained separate. This is adapter evidence, not general PDB support.

Archive SHA-256 pins are:

- `legacy-threshold-alpha-context.tar.xz`:
  `2900b8082f0175123c49f02e940a23a32cbd7c4d7e803e88a4e1e06b6b64c0bf`
- `legacy-threshold-alpha-large.tar.xz`:
  `218e75c0bb6f333afc49e4ab26a5a6bda614770a67a675e46ff7395b955c2e73`
- `legacy-threshold-alpha-projection.tar.xz`:
  `0f860fb6aa0fc80a60113cd564a0bc8b00b4689743e857a6397afe9ed31e763f`

Their adjacent manifests seal all members. Base is 303,256 compressed bytes,
3,356 regular files and 21,909,230 payload bytes with an 8 MiB LZMA2 dictionary.
Large is 311,072 compressed bytes, 58 files and 90,803,851 payload bytes with a
32 MiB dictionary. History is 44,948 compressed bytes, 572 files and 693,847
payload bytes with an 8 MiB dictionary. Resource sizes are evidence packaging
properties, not application runtime-memory measurements.

Archives retain the exact harness source used for the capture build, scripts,
capture-time validators, generated headers, observation hook patches/helpers,
commands, compressed full logs, source/binary pre/post seals, raw buffers/tables
and actual old-writer documents. They retain executable hashes, not compiled
harness executables.
Verifiers pin archive/manifest/report hashes, require bounded regular members,
validate provenance and observations, and create separate consumer directories.
Missing evidence fails the registered native launcher rather than silently
skipping parity tests. Consumer environment variables are
`GIMP_PAINTER_THRESHOLD_ALPHA_FIXTURES` and
`GIMP_PAINTER_THRESHOLD_PROJECTION_FIXTURES`.

The provenance audits found only fresh synthetic inputs; no artwork-loading
path exists in the harnesses. Independent replay matched generated source/mask
and seed bytes. All 40 base/large text members and 20 history text members,
including decompressed logs, were inspected. No credentials, private user
content or artwork were found. Native intermediate/output bytes are attributed
to those synthetic inputs; this is not independent regeneration of every old
algorithm output.

The old runtime retains the same two startup `g_value_set_object` GObject
criticals and missing fresh-profile tag-cache message as earlier sealed corpora.
Legacy compiler warnings remain recorded. No additional case-time warning,
critical, crash or timeout was observed in the successful captures. This is
explicit inherited-runtime evidence, not warning-free legacy acceptance.

## 8. Inspected ordinary and standalone results

The final focused ordinary run passed exactly 15 distinct native paths in
42.33 seconds and 3 XCF paths in 16.33 seconds, with no skips, crashes or
unexpected diagnostics. The native paths cover integer extremes; strict malformed
admission; tiny selection routes; late/sealed context; outside no-merge;
cancel/replace/lower-change/close; the original genuine corpus; native profiles;
exact quota/cache retention; actual partial import; unchanged real routes;
idle unchanged-byte notification; 16 analytic boundary scenarios with four runs
each; and the separate genuine projection-history corpus.

Eight vector/spill partial-import cases interrupt cancellation, definition
replacement, lower-input change or owner closure after actual staged chunks,
with observed progress approximately 0.916–0.930. They verify the entire retained
old cache. Spill processing may cross RUNNING within one owner quantum; tests
use observable IMPORTING rather than timing-dependent spill RUNNING claims.
The three focused XCF paths are `threshold_old_writer_roundtrip`,
`threshold_malformed_writer_capsules` and `threshold_active_save_close_reopen`.

The 12 registered targets also passed:

- `gimp-filter-layer`: 175 TAP cases, including existing operations and helper routes
- `painter-xcf-open`: 19 TAP cases
- `painter-xcf-roundtrip`: 49 TAP cases
- `painter-filter-scheduler`: 39 TAP cases
- `painter-filter-owner-gates`: 5 TAP cases
- `painter-filter-native-kernels`: 6 TAP cases
- `painter-filter-context-native`, `painter-filter-point-shadow`,
  `painter-filter-sharpen`, `painter-filter-sharpen-shadow`,
  `painter-filter-context`, `painter-filter-import-completion`: successful non-TAP targets

The roundtrip target retains 295 expected negative-case diagnostics, classified
using its exact current registry identity and unchanged diagnostic patterns.
Other final targets have zero classified expected diagnostics. Passing diagnostic
classification does not mean empty stdout/stderr or warning-free build/startup.

Standalone wrapper normal and ASan/UBSan runs each passed 1,222 generated
scalar/all-alpha/tiny/crop/row-boundary cases, 20 archived original PDB outputs
in vector/spill paths, and 200 read/write/flush cancellation/storage-error ordinal
cases. They also cover malformed extents/overflow/direct alias, transactional
vector aliasing, offsets/outside distinction, monotone progress and final-context
color invariance. Leak detection was disabled. These focused wrapper results
cannot substitute for native owner/XCF sanitizers.

Sixteen pure-Python sanitizer-runner checks passed. They exercise runner syntax,
selection/helper/report behavior with mocked subprocesses; they do not execute
GIMP, a compiler or a linker. The focused native/XCF ASan/UBSan run passed all 15 native and 3 XCF paths
in 217.495 seconds, each with exit zero and its exact single positive TAP result.
It rebuilt 37 explicitly instrumented route, owner-context, scheduler/raster/
spool, bindings, Undo/duplication, XCF and harness units plus 35 separately
identified RTTI-only units into private artifacts. Remaining GIMP/dependencies
and the RTTI-only units were not sanitizer-instrumented; leak detection was
disabled. All 3,046 source/dependency seals, 114 ordinary-build seals, 2 private
executable seals and 6,285 fixture seals remained unchanged, with no seal read
errors. Ordinary-build seals cover canonical replacement inputs, direct link
objects, original test executables, build metadata and archive containers; other
unmodified thin-archive members and shared libraries are not individually sealed.
The unrelated standalone worker executable is outside these selected harness
link closures. No whole-program sanitizer or leak result is inferred.

## 9. Retained failed attempts and limits

Earlier attempts remain evidence of the debugging sequence, not successful
coverage:

- The first standalone wrapper setup lacked the restored GLib pkg-config
  environment; the corrected-environment retry passed.
- The first direct adapter build failed strict indentation/visibility warnings;
  its corrected source compiled and passed the 334-invocation gate.
- The first native build exposed a missing `gimp_layer_new` declaration in the
  XCF harness; the proper header was added before the successful build.
- The first genuine negative-offset native case exposed the unrestricted lower
  proxy read. Source review rejected a zero-only clip because it would discard
  prior off-projection pixels. Cache-prefill plus parent clipping fixed the
  production route without changing expected fixture bytes.
- A separate idle test used another test's hard-coded cached color; it was
  corrected to assert this test's actual complete cache with the same identity
  and generation checks.
- The corrected intermediate native run selected one boundary path twice:
  15 TAP passes represented 14 unique paths. The final run deduplicated selection
  and passed 15 distinct paths including the new genuine-history consumer.
- The first history harness completed 48 runs, but its validator overrequired
  outside alpha 255 after movement. The corrected validator retains exact prior
  cache/copy checks for every run and limits that specific 255 check to run two.
  A fresh capture was sealed; the failed report was not rewritten.
- The initial final-regression classifier supplied formatted Meson names where
  short registry identities were required, misclassifying the 295 known
  roundtrip negatives despite all 12 processes exiting zero. Retained-log
  reclassification used unchanged rules and source/executable seals. The original
  receipt remains retained; no unexpected diagnostic was waived.
- A portable runner invocation used `--report` instead of its supported
  `--receipt`; the valid retry passed all 16 checks.

Full aggregate/installed runtime, Windows/macOS, UI/editor behavior, general
PDB/helper support, exhaustive profile behavior, other operations' projection
history, LeakSanitizer, total-RSS/latency guarantees, actual very-large-file I/O,
process crash isolation and whole-program thread safety remain outside this
bounded contract.

## 10. Reproduction and receipt locations

`migration/fixtures/legacy-threshold-alpha-README.md` and
`migration/fixtures/legacy-threshold-alpha-projection-README.md` document exact
capture schemas and reproduction. Verify immutable evidence with
`tools/check_threshold_alpha_evidence.py --extract EMPTY_DIR` and
`tools/check_threshold_projection_evidence.py --extract EMPTY_DIR`.

The ordinary app targets use
`migration/tests/run_filter_owner_context_fixture_test.py` to verify/materialize
all required corpora. Use the configured dependency environment, an authenticated
X display, the shared build lock and a successful build of the exact source.
The acceptance receipt records the selected registry identities and exact test
paths. Use Meson's `--test-args` with explicit GLib `-p` selections for the focused
native and XCF paths, or run the listed registered targets without path filters
for their complete regression sets. `--no-rebuild` is valid only after building
that source in the configured build directory.

Standalone wrapper checks use
`app/painter/tests/run_filter_point_shadow_tests.py` with its report path and
optional `--sanitize`, each in a fresh scratch output directory. Native focused
sanitizers use `migration/tests/run_sharpen_context_sanitizers.py --operation
threshold-alpha`, exact native/XCF test paths, a report path and an inherited
exclusive lock descriptor. Open the lock only after authenticated-display and
fixture wrappers because those wrappers do not forward inherited descriptors.
Each selected path runs in a separate process; missing, skipped, duplicate or
mismatched TAP results fail the runner. Private compilation/link artifacts must
leave the ordinary build unchanged. The completed receipt records the exact seal
and instrumentation scope; those limits apply to its pass claim.
