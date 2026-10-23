# Native Value Invert owner context

Status: bounded slice accepted after the normal, scoped sanitizer and affected
registered regression gates recorded in
`migration/tests/native-value-invert-context.json`. Parent task 16.003 remains open.

## Bounded execution contract

The exact literal `plug-in-vinvert` claims the nonlinear-U8 native route before
argument validation. Exactly three `G_TYPE_INT` context slots are required:
recorded run mode, image and drawable. All signed values and original raw bytes
remain retained; execution uses the current owner/input rather than stored IDs.
Value Invert has no operation-specific parameter or Max RGB mode. RGB and RGBA
lower sources are admitted through RGB owners; no-alpha input is promoted by the
owned projection. Gray and indexed owners are unsupported on this raw route.

The independently accepted `filter_value_invert_raw_raster`/point-shadow worker
processes straight native-encoded RGBA8, including RGB hidden by alpha zero.
Original byte/normalized-real point entry points remain unchanged. For RGB
owners, every encoding other than nonlinear U8 retains its existing context-free normalized-real route,
including ignored saved-context slot types; its regression matrix covers all
17 other component/TRC combinations and a float-linear spill case. This is
preservation of that extension, not broad legacy precision parity or a new PDB,
helper descriptor, editor activation, wire protocol, or public argument schema.

## Input, final context and publication

The owner first captures the committed drawable cache, then overwrites only the
current immediate-parent projection intersection from the lower input proxy.
Top-level clipping uses the image rectangle; a group child uses its parent's
actual offset and extent. Off-projection input therefore retains prior completed
or loaded bytes, including after an owner-only move. The worker receives a plain
immutable captured start ROI, dimensions, private raster storage and progress.
It never accesses an owner, image, selection, GObject or GEGL buffer.

The raw wrapper validates geometry and its legacy signed selected-area bound,
zeroes the full private shadow, and changes only the captured start ROI. Tiny
positive crops are valid. Bytes outside that ROI stay zero even if final
selection expands. An outside nonempty start selection carries an explicit
`no_merge` disposition that overrides later selection changes.

Current final selection, component activity and the target's own alpha lock are
captured and sealed by the shared owner carrier. Ancestor lock is not substituted
for the target's own lock. Native import merges into a private staging buffer;
only a fully imported current generation may replace the completed drawable
buffer on its owner thread. Cancellation, replacement, lower invalidation and
close retain the previous completed cache and its bytes. This is in-process
thread execution and buffer separation, not process/crash isolation or
whole-screen/display-vsync atomicity.

Logical admission reserves native input, mask and private native staging at
9 bytes/pixel, two RGBA vectors or spill files at 8 bytes/pixel, and 304 KiB of
bounded owner/wrapper/kernel scratch. Spill additionally charges six 32,768-pixel
RGBA transport chunks and exactly 17 bytes/pixel logical spill reservation.
The trusted application swap directory is used. These are logical admission
claims, not RSS, GEGL-cache or latency guarantees. The native test rejects
17,895,066 spill bytes and accepts 17,895,067 for 1031x1021 RGB, retaining the
completed cache across refusal and recovery.

## Strict XCF and persistence

Two fresh actual old-writer files independently confirm the exact 56-byte
capsule with literal NUL-terminated `plug-in-vinvert`, tags 2/1/10/0 and payload
sizes 4/0/0/0. The loader checks decoding status, original exact name bytes,
outer/retained length, record count/kinds/tags, payload sizes/offsets, contiguous
framing and terminal position with no tail. Omitted image/drawable slots become
typed current-context placeholders in a separate executable model. Original
capsule and signed run-mode bits survive unchanged.

Synthetic persistence tests cover twelve signed run-mode/alpha scenes and
eighteen malformed capsules. Five malformed native typed models survive
Save/reopen and duplication without being silently repaired from a syntactically
valid raw capsule. These tests are source-backed synthetic cases, not additional
old-writer observations. The two actual old-writer files cover ordinary Open
with loaded cache and zero runs, duplication, Save/close/reopen, changed and
byte-identical lower notifications, and definition Undo/Redo changing only the
retained run-mode slot. No nonexistent Value Invert mode is introduced.

Four active Save cases cover vector RUNNING entry and spill actual partial
IMPORTING, each followed by cancel or close. They compare the complete retained
old cache, reopen as pending with zero unsolicited executions, destroy the old
image, recompute in the new image epoch, and Save/reopen the completed result.
These are synthetic lifecycle evidence, separately from the actual old writer.

## Native test coverage and limits

Sixteen focused native paths include fourteen synthetic/source-backed paths plus
two genuine corpus consumers. Coverage includes strict definitions and retained
values, RGB/RGBA and alpha, tiny/cropped/outside selections, late/sealed controls,
all sixteen component masks with soft selection, vector/spill lifecycle,
logical-memory and spill-quota refusal, Adobe RGB native bytes, preserved real routes, and projection-history input.
Eight synthetic parent/offset/loaded-cache scenes each run four generations.

The lifecycle matrix interrupts WAITING, PREPARING, vector RUNNING entry and
IMPORTING for cancel/replacement/lower-change/close. Spill RUNNING is deliberately
not asserted: its processing phase may complete between owner polling quanta.
Eight additional vector/spill cases interrupt after observed positive private
import progress and verify the entire prior completed buffer. This does not
claim deterministic native mid-kernel cancellation timing. The independent raw
foundation separately covers cancellation at every bounded read/write/flush
ordinal. The registered import-completion test covers spool publication gaps
using its existing test-only observation copy; no production pause hook is added.

Five focused XCF paths cover the persistence groups above. Normal execution
passed all 21 paths with exact non-skipped TAP matches and no unexpected runtime
diagnostics. The same 21 paths passed scoped ASan/UBSan with 40 instrumented
translation units and 36 RTTI-only compatibility units. Remaining GIMP and
dependencies are uninstrumented; leak detection is disabled. Full report, exact
commands, outer fixture verification and unchanged seal maps are retained.

The bounded registered regression selection passed thirteen unfiltered targets,
including all 223 FilterLayer cases, all 31 XCF-open cases and the 49-case XCF
roundtrip suite with its registered `-m slow` argument. The latter retains 295
expected malformed-input diagnostics under the unchanged strict classifier.
The first registered attempt is retained as failed: the separate build lacked
the bundled filter worker (exit 251), and the registered Python fixture wrapper
created 19 bytecode files after the pre-seal. After building that prerequisite and
removing only those generated files at the terminal boundary, the next attempt
exposed an unbuilt nested Blinds executable and is also retained as failed. The
complete default build was then performed in the separate owner tree, followed
by a preflight of the thirteen selected test executables, the helper, its four
fixed plug-ins and shared-library resolution. These eighteen executables,
build-local shared libraries and registry/build metadata are individually
sealed. Other default-build plug-in executables and system dependency binaries
are not individually sealed; the exclusive build lock was held throughout the
run. All thirteen registered targets were rerun with bytecode generation disabled, equal pre/post source path sets, and unchanged source/build seals. No production code or diagnostic policy changed.
The 64 Python controls are separately identified as non-native evidence.

## Genuine evidence

The pinned legacy reference is afa43fae3e920210146abed514f136fd49f671b5.
The fresh finite corpus has 140 base cases (50 synchronous PDB, 90 live), 2 large
live scenes (also consumed through the native spill path), and a separate
16-scene/48-run projection-history supplement.
Modern native consumers compare 92 live scenes/126 final outputs and 16 history
scenes/48 outputs, plus prior-cache, loaded-idle and actual copy geometry checks.
The 50 old PDB invocations are not modern direct-PDB executions.

See `migration/tests/value-invert-capture-evidence.json` and the fixture README
for hashes, observed old-writer shape, known inherited startup diagnostics,
source/binary pre/post seals and rebuilt-reference provenance. Recovery build
receipts are post-build evidence; their manifest was written after capture and
is not a pre-capture static-archive seal. Original capture source/executable
before/after seals remain separate and valid. Old runtime/reference dependency
limitations are retained rather than promoted to whole-old-suite acceptance.

## Reproduction and acceptance identity

Use the configured dependency environment, authenticated 1080x1920 TCP Xvfb,
verified owner-context fixture wrapper and inherited exclusive shared build lock.
`run_value_invert_focused_normal.py` defaults to the complete 21 paths. Its
`--synthetic-only` switch is explicitly incomplete diagnostic coverage. Complete
runs independently re-materialize hash-pinned Value Invert artifacts and require
the live consumer trees to match every relative filename/hash; links, special
files, empty input, missing results, skips, unexpected diagnostics and seal drift
fail. The focused sanitizer operation is `--operation value-invert`, selecting
the same 16 native and 5 XCF exact paths.

Normal acceptance sealed commit 7b80f7990d573dcc0aa2e50f0e35f31c45173826,
tree f511830bd8503b169abaa023b8ed2462170f6ba1. Subsequent publication adds
provenance, contract, ledger and receipt files; the normal all-file seal is not
claimed to describe that later tree. Sanitizer acceptance sealed commit
a585034aeeb0040930c4e96e0af917af4392df20, tree
814f49de724f4c6f344ba75e03f6d14199be0e60. Registered regression acceptance
sealed commit 7018d00e35b4c7ed1f1954df31c1a1e3878006d8, tree
8b6a462f76830ab602dc10e7a80090fbb7564b08. The intervening two changes only
retain and correct legacy-runtime provenance. The final receipt binds these
identities and hashes the unchanged tested implementation and runners. Changed
code, fixtures or runners require affected revalidation.

Full aggregate, installed UI, other operating systems, general PDB/helper and
precision parity, enormous actual I/O, LeakSanitizer, whole-program sanitizers
and whole-port completion remain separate obligations. Parent task 16.003 stays
open.

## Post-acceptance whitespace-only correction

After commit `1bf84e12b040804e833ce67f3b73bf8d62dead5a`, publication preflight
found one redundant final blank line in the capture consumer. Exactly one
newline byte was removed from `app/tests/test-filter-value-invert-capture.inc`.
`git diff --ignore-blank-lines` against that commit is empty for the file; the
full delta from the accepted raw foundation passes `git diff --check`. All 64
Python controls and WBS checks passed again. No semantic production or test
assertion changed, and no additional native execution is claimed. The receipt
retains the original tested hash and records the final file hash separately.
