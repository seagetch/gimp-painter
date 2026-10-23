# Native Sobel owner context and XCF contract

Status: **bounded native owner-context and XCF acceptance passed on
2026-10-16**. The final focused ordinary run passed 16 native owner paths and
3 XCF paths; focused AddressSanitizer/UndefinedBehaviorSanitizer passed the same
19 paths. All 13 selected registered regression targets then passed with exact
command/TAP checks and unchanged source/build/output seals. This is not a full
aggregate, installed-runtime, whole-program sanitizer or whole-port acceptance.
The publication-safe receipt is `migration/tests/native-sobel-context.json`.

## 1. Executable domain

This route implements the literal `plug-in-sobel`, not `plug-in-edge` with its
Sobel option. It admits native nonlinear U8 RGB and Gray with exactly six typed
integer slots: run mode, image, drawable, horizontal, vertical and keep-sign.
All three flags preserve signed INT32 values, including negative values and
extrema; every nonzero value is true. Recorded context identifiers remain data:
execution uses the current owned input rather than dereferencing saved IDs.
Malformed definitions cannot fall through to another Sobel executor. Unsupported
precision/TRC and indexed input remain unenabled, retaining the definition and
completed cache. This adds no arbitrary-PDB helper allowlist, dialog/editor route,
wire protocol, or serialized format version.

The raw arithmetic is the already accepted foundation in
`legacy-sobel-raster.md`: weighted top/bottom and left/right derivatives, original
C truncation in negative `ROUND`, RMS rounding before division by 5.66, and
synthesized alpha 255 exactly when any output color exceeds 10. Source alpha does
not gate hidden-color arithmetic. Both-disabled signed mode gives color 127 and
alpha 255; unsigned mode gives zero. RGB/Y lower input is promoted to RGBA/YA by
the owned projection, as observed in the genuine old FilterLayer. Worker
transport is straight RGBA8; Gray is replicated and extracted exactly. Native
capture/staging preserves the drawable's encoded space. Adobe RGB and Gray Lab
TRC are tested in vector and spill routes; this is not exhaustive profile parity.

## 2. Owned input, start ROI and retained history

The owner thread captures input and start-selection bounds in bounded chunks
before worker launch. Workers receive plain immutable dimensions, region, flags
and owned storage, not drawable, image or selection GObjects.

The original parent projection copy overwrites only its intersection with the
existing FilterLayer drawable. Pixels outside that copy retain the previous
cache. The nonlinear-U8 Sobel route therefore first reads each admitted input
chunk from the completed owner buffer and overwrites only immediate-parent
coverage from the lower projection. Top-level bounds use the image; group bounds
use the actual group offset and extent, including child-union coverage outside
the canvas. This policy is shared with the previously accepted Threshold Alpha
route, not generalized to all operations. Signed 64-bit clipping avoids
intermediate coordinate overflow; focused extreme-offset tests exercise the
shared geometry prerequisite without claiming huge-image allocation.

The separate genuine history corpus has 24 scenes and 72 runs: 16 ordinary
YA/RGBA top/group positive/negative fresh/seeded histories plus 8 top-level
selection/halo histories. Run one uses flags (0,0,1), run two (1,1,0), and run
three moves only the filter with (1,1,0). Lower source bytes stay unchanged.
All 72 before-copy caches, parent/copy rectangles, prepared inputs and final
outputs are checked. Twelve seeded controls call the genuine mark-as-loaded API
and display with zero jobs. These are synthetic cache seeding observations, not
old-XCF reopen evidence.

Sobel replicates horizontal neighbors at ROI endpoints but clamps vertical
neighbors to the full drawable. Vertical halo rows outside the start ROI may
therefore read retained cache. Eight selected histories contain 184 retained
halo pixels; all 16 enabled-gradient second/third runs differ from a calculation
with that halo zeroed. Exact cache/copy checks also apply to run one, whose
both-disabled output cannot itself witness gradient dependence. The history
checker separately records 1,776 retained outside-copy run-pixels and 2,664 group
run-pixels outside the image.

No selection means the full drawable. A selected region may intersect in any
positive shape, including 1x1, 1xN, Nx1 and 2x2. A selected region wholly outside
the drawable yields an explicit `no_merge` outcome. It must preserve captured
input even if final selection later expands or clears. The private full shadow
starts at zero; only the start ROI is written. Thus expansion does not invent
filtered bytes outside the original ROI. Full dimensions must be positive INT32,
RGBA extent must fit signed 64-bit storage, and nonempty ROI width is at most
`INT32_MAX / 4 - 2`. Sobel does not inherit Sharpen's tiny-crop minima or
Threshold's ROI-area rule. The alpha-bearing worker avoids the old no-alpha
counter-overflow domain described in the foundation contract.

## 3. Final context and completed-buffer invariant

After raw work, the owner captures current selection weights, active components
and the target layer's own alpha lock. A parent's lock is not substituted.
Dirty selection capture restarts before sealing, including changes during
bounded mask copying. Once sealed, that generation's final context is immutable;
later context-only edits do not rewrite staged chunks. Tested idle context-only
changes do not schedule work; a later changed or unchanged-byte lower-input
notification captures fresh context.

Final merging uses the exact legacy native-byte REPLACE arithmetic, including
signed integer rounding, inactive components and alpha lock. Raw shadow and
merged output are separate evidence. In particular, synthesized zero alpha can
leave original hidden colors in the final cache; raw byte replacement is wrong.
The checker arithmetic was independently reconciled with 304 pre-existing
synthetic old-context merge records, separately from the new Sobel observations.

Execution is asynchronous and in-process. A scheduler worker writes private
vector or spool-backed raw output. Owner-thread import merges against captured
input into a separate private GEGL staging buffer. The visible completed
drawable buffer is never a worker destination or a partially imported buffer.
Only complete import for the current generation replaces it on the owner
thread. This is a completed-buffer invariant, not process/crash isolation,
whole-program thread safety, whole-screen compositing atomicity or display-vsync
synchronization.

Cancellation, definition replacement, lower-input invalidation and closure
preserve the prior completed cache and reject partial/stale publication. Focused
vector/spill cases interrupt actual partial import, retain a cache sentinel, and
verify recovery where applicable. Wrapper failures may leave partial private
storage, never an accepted completed cache. Raw progress can reach one before a
later cancellation prevents publication; completion is established by state and
generation, not progress alone.

## 4. Logical admission and bounded storage

The route reserves native owned input, possible final mask and private staging
at 5 bytes/pixel for Gray or 9 for RGB. Vector transport adds two RGBA rasters
(8 bytes/pixel). The shared fixed 304-KiB allowance conservatively covers bounded
owner capture/import and Sobel's 32,792-byte kernel scratch. Above the existing
4-MiB RGBA threshold, real spill storage uses two files and additionally charges
six 32,768-pixel RGBA transport chunks (768 KiB). Logical spill reservation is
13 bytes/pixel for Gray or 17 for RGB. Spool location comes from trusted
application configuration, never a saved filter argument.

At 1031x1021 the Gray quota rejects 13,684,462 bytes and accepts 13,684,463; RGB
rejects 17,895,066 and accepts 17,895,067. Admission refusal preserves completed
cache identity/generation and definition without full capture or worker launch.
These are logical accounting and bounded-transfer claims, not measured RSS,
GEGL cache/swap limits, latency bounds or actual high-offset I/O beyond 4 GiB.
The existing completed cache remains subject to host memory policy.

## 5. Actual old writer and strict restoration

All 64 saved XCFs were written by the pinned original runtime. Each measured
capsule is exactly 90 bytes: the 14-byte `plug-in-sobel` name including terminal
NUL, followed by seven records with tags `2,1,10,2,2,2,0` and payload sizes
`4,0,0,4,4,4,0`. Those records encode run mode, omitted image/drawable, horizontal,
vertical, keep-sign and END. Signed values occur at offsets 26, 54, 66 and 78.
The observed property starts at 564 and payload at 572 in these generated files;
those file positions are observations, not hardcoded decoder admission rules.

The loader requires successful decoding, exact outer/retained length, exact
literal name bytes, contiguous record framing, exact tags/kinds/payload sizes,
six argument slots and the expected END position. Repaired display names and
approximate shapes do not activate execution. Omitted context IDs acquire typed
placeholders in the executable model; original raw capsule and signed values
remain preserved.

Every genuine file passes ordinary Open with zero unsolicited runs, duplication,
native Save, destruction of the original image, and reopen preserving raw
bytes/cache. Changed-lower and unchanged-byte notifications are separately
compared with genuine second outputs. H/V/K edits each cross definition Undo/Redo
and compare with genuine alternate controls; the 12 directly captured flag-toggle
reruns are also used. Recomputed caches cross a second Save/close/reopen epoch.

Twenty manufactured malformed/unsupported variants test wrong kinds and lengths,
name encoding, outer length, extra/missing records and an unsupported literal.
They remain inert and retain raw bytes/cache through Open, Save/close/reopen,
lower invalidation and another Save/reopen, with zero executions. They are
synthetic rejection tests, not additional old-writer observations.

Four synthetic native active-save cases cover RGB/Gray while vector RUNNING or
spill IMPORTING. Serialized pending state and completed-cache sentinel survive
Save and reopen; closing the original image does not resume its worker. A new
image uses its own live scheduler generation, recomputes once, then preserves
the completed result across another Save/reopen. Serialized generation/state
fields are checked separately from the new live scheduler epoch. Every active-save/reopen stage compares the complete synthetic drawable bytes
with the sentinel or expected result. These full-image comparisons are not
genuine-old active-save observations.

## 6. Genuine capture scope and independent checking

Reference commit: `afa43fae3e920210146abed514f136fd49f671b5`. Capture harnesses use
its unchanged installed `edge-sobel` executable and unchanged original merge and
writer; private observation-only hooks record actual start, progress, merge,
copy and end events. All inputs are deterministic synthetic buffers. No user
artwork is loaded. The modern port does not generate the reference outputs.

Main evidence contains 410 base plus 2 large cases: 182 original direct-PDB
invocations and 230 live scenes with 356 observed live runs. The modern consumer
compares those 230 scenes/356 final outputs, not 412 modern PDB calls. The separate
24-scene/72-run history corpus is additional. Main live coverage includes every
boolean combination, signed extreme flags, Y/RGB promotion, YA/RGBA, tiny and
tile/strip-crossing geometry, uniform and hidden colors, gradient 10/11, negative
signed rounding/RMS, selection/feather/disjoint/start-small-to-expanded or
cleared context, components, own alpha lock, and changed/unchanged/flag reruns.

Old progress reports absolute row divided by ROI height every 20 rows. The y=0,
height=41 controls observe 20/41, 40/41 and 1. The y=20,height=3 trap can report 1
mid-work and suppress a final repeated 1. Late phases are separately classified
as actual merge-entry, strict intermediate progress, and the offset progress-one
trap. Start/end/merge observations and actual pending-state clearance establish
settlement. No progress-one-as-completion shortcut is accepted.

Checkers seal archives, manifests, capture reports, original source/binary
pre/post hashes, harness/scripts, observation patches, raw buffers and writer
files. They independently regenerate inputs/masks, check exact old merges,
validate framing and constrain archive members. Small cases also receive the
full independent Sobel equation check. For the two 1031x1021 cases, that Python
checker deliberately skips full raw Sobel arithmetic for cost, while checking
other provenance/context/merge/ROI obligations. The focused native test still
compares every final output byte against those actual large old captures through
real spill execution; this distinction remains explicit.

All three captures succeeded on the first execution attempt. Nine ordinary
startup diagnostics are retained: six `g_value_set_object` / `G_VALUE_HOLDS_OBJECT`
assertion messages and three missing tag-cache messages. No unexpected capture
diagnostic is accepted. These are not warning-free old-runtime runs. Independent
audit matched every archive member against raw capture and manifest, regenerated
the finite plans, and matched 25/25/27 old source seals to checkout and pinned
Git objects. All 64 capsules were independently parsed. Current checker source
differs from archived capture-time copies only by later observed pins and a
framing comment; original capture snapshots remain intact.

Consumer variables are `GIMP_PAINTER_SOBEL_FIXTURES` and
`GIMP_PAINTER_SOBEL_PROJECTION_FIXTURES`. The shared fixture launcher verifies and
materializes both; missing evidence fails rather than silently skipping parity.
Archive hashes and exact tested source hashes are in the adjacent receipt.

## 7. Focused acceptance and transparent retained attempts

Final focused ordinary evidence passed all 19 unique requested paths with exact
positive TAP membership, zero skips/crashes/unexpected diagnostics and unchanged
source, external-input, build-output and ELF seals. GLib informational/debug
messages remain in logs; empty output is not asserted. The default native build
also completed. Seven parser controls cover the cumulative plans emitted by
repeated GLib `-p`; 17 sanitizer-runner controls and 12 capture-helper controls
passed separately as Python tests.

The first ordinary attempt executed the 16 owner tests successfully but its
strict parser incorrectly expected one final plan rather than GLib's cumulative
plans; it was rejected and retained, not retrospectively counted as acceptance.
After the bounded parser correction, a second attempt passed owner tests and
two XCF paths but failed the active-save test's assumption that serialized and
new live generations must match (observed 3 versus saved 2). A test-only correction
now checks exact serialized fields separately and validates fresh waiting/live
state through completion. No production assertion was relaxed or production
code changed for that correction. Both complete final ordinary groups were rerun.

Focused ASan/UBSan then passed all 19 paths with 40 instrumented translation
units, including Sobel, owner/context, scheduler/raster/spool, bindings,
Undo/duplication, XCF and both C harnesses. Another 36 linked C++ units were
rebuilt only for RTTI compatibility and are not sanitizer-instrumented. The
unlinked external filter-worker executable was not part of either harness.
Remaining GIMP/dependencies are uninstrumented; LeakSanitizer is disabled. This
is not whole-program sanitizer or leak acceptance. The runner sealed 3,061
source/header/dependency inputs, 118 ordinary build inputs, 8,487 materialized
fixture files and two private executables, unchanged throughout. Canonical Ninja
production entries, transitive dependencies and actual link membership control
instrumentation; ordinary build objects are not replaced.

The first broader preflight rejected 78 private sanitizer ELF additions before
any registered tests. No original ELF changed or disappeared. Each addition was
reconciled against the sanitizer compilation/executable seals; only those exact
additions are excluded from the older cross-baseline comparison, while the new
before/after seals include all files. The fresh 13-target run passed: 332 TAP
paths, 27 Python controls, 192 C and 211 C++ header probes, plus the standalone
Sobel foundation target. The 295 expected XCF negative-case warnings remain
classified and retained, along with ordinary plug-in dummy-file notices, GLib
debug output, intentional helper/batch negative-case messages and Python test
summaries. There were no skips, crashes or unexpected diagnostics; this is not
an empty-stderr or warning-free claim. Source, external-input, build and ELF
seals, including all 78 private sanitizer outputs, were unchanged.

The prior full aggregate belongs only to exact source commit
`7463d5dc65fa74832ab7f8219aae71dc83d6b8c7`; it is not carried forward as an
aggregate result for this Sobel change. No complete aggregate,
installed runtime, cross-platform, tablet/UI, arbitrary-PDB process isolation or
whole-port completion is implied by these focused results.

## 8. Reproduction and receipt hygiene

Use the configured native/legacy environments and the shared exclusive build
lane. `SOURCE`, `BUILD`, `LOCK`, `REPORT`, `ENV`, `DISPLAY_WRAPPER` and legacy
paths below are caller-supplied placeholders. `DISPLAY_WRAPPER` provides the
existing private authenticated test display. Choose fresh report/output paths;
do not overwrite retained attempts. The receipt's `commands` arrays contain the
complete selected paths and allowlisted reproducible argument vectors.

```sh
. "$ENV"
export PYTHONDONTWRITEBYTECODE=1 CCACHE_DISABLE=1
python3 -B "$SOURCE/tools/check_sobel_evidence.py"
python3 -B "$SOURCE/tools/check_sobel_projection_evidence.py"
python3 -B "$SOURCE/migration/tests/test_sobel_capture_helpers.py"
python3 -B "$SOURCE/migration/tests/test_sharpen_context_sanitizers.py"
```

For ordinary checks, hold `LOCK`, build with `ninja -C BUILD -j2`, and run each
receipt Meson command through `DISPLAY_WRAPPER`; the registered test launches
verified fixture materialization. For focused sanitizers, wrap the receipt's
sanitizer command in `run_filter_owner_context_fixture_test.py --`, provide the
same display and an inherited exclusive lock descriptor 9, and supply
`--lock-file LOCK --lock-fd 9`. Repeat every recorded native/XCF selector.

The public JSON is constructed from explicit allowlisted fields, not a raw
report copy. It retains source/fixture/output hashes, outcome counts, bounded
scope, diagnostic counts and placeholder commands; it excludes local absolute
paths, credentials, raw environment, display authorization and complete console
logs. Full raw evidence and rejected attempts remain separately retained.
Documentation was written after tested executable sources were sealed; these
new prose/receipt files are not claimed to have existed during execution.

## 9. Ledger validation boundary

Only `16.003/native-sobel-context` is newly completed; broader parent gates stay
open. The full task validator is **not passing**: it exits 1 with 67 diagnostics
on both public base `8201a7ab8e2cc1034409551013db8b9b6ba8c0ad` and this worktree.
A read-only extraction of that commit's exact validator, ledgers, inventory and
baseline data reproduced the same ordered diagnostic list. There are zero new
or removed diagnostics. Existing malformed toolbar rows, missing prior progress
records, a Threshold prerequisite reference and legacy inventory assignments
remain unresolved; this bounded unit does not repair or waive them. Removing
only the new Sobel row from each ledger restores the exact base bytes. The
receipt preserves comparison/output fingerprints and this limitation; it does
not label the full dependency gate as passed.
