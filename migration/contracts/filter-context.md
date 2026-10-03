# Legacy FilterLayer context and exact shadow merge

The independently verified scalar/context foundation is now registered in Meson
and connected to the live GIMP 3 Blinds FilterLayer through owner-only capture
and a private raw-shadow protocol. The accepted owner slice is recorded under
`migration/tests/filter-blinds-context/`; it covers this one procedure family. No standard
GEGL effect replaces the independent FilterLayer execution route.

## Corrected genuine old evidence

`legacy-filter-context/capture-report.json` inside the immutable archive
`migration/fixtures/legacy-filter-context.tar.gz` records a new
capture from old source `afa43fae3e920210146abed514f136fd49f671b5`. The restored
installed GIMP 2.8 executable SHA256 is
`424fa608bf6e8ca6de8c6585e16c15e1111c3b7c1b3d270ae847a79c20d13c3a`.
The observation-only harness
executable SHA256 is
`3fbc210f67e099e42caed1ee76854980087bd110274c458e4a4b7086a57fc9d1`.
The normal installed executable is not modified.

The capture script verifies the pinned transformation sources, compiles private
copies of three old-core translation units, and links the actual old application
archives. The complete observation patch, harness, build commands, executable
hashes, source hashes, diagnostics, raw bytes, and event records are retained.
The hooks observe Filter start/end and actual shadow merge buffers. A progress
hook changes the requested fixture context at the actual incoming PDB progress
request, before the existing headless progress-forwarding guard. It neither
changes that guard nor replaces a plug-in transformation/control algorithm.

There are 280 native shadow cases: Y, YA, RGB, and RGBA; all component masks;
unrestricted, hard, soft, and globally nonempty but entirely outside selections;
drawable offsets; target alpha lock; and a deliberately synthetic ancestor-lock
field probe. Old groups reject alpha-lock changes through their public setter.
That last probe sets only the old native group's field and establishes the
target-only merge rule; it does not claim an old group-lock UI workflow.

Direct fixture mask writes reproduce both invalidations performed by old
`gimp_channel_apply_region()` (`app/core/gimpchannel.c:817`): boundary invalidation
and `bounds_known = FALSE`. Native emptiness is asserted before transformations;
capture validation additionally requires hard/soft coverage and the outside
versus globally empty distinction. There are 24 more genuine shadow merges from
twelve live FilterLayer scenes, each with an initial run and one unchanged-byte
lower-source update. The exact prepared Filter input is captured from the real
old layer immediately before the old runner starts.

The 53×41 scene stays within one old projection tile and generates two Blinds
STEP=40 updates. Context mutation occurs at the recorded request
`0.97560975609756095`, after Blinds has sampled its background/ROI and written its
last pixel chunk, while its PDB invocation is still running and before shadow
merge. This is evidence of phase semantics, not a responsiveness measurement.

| Context changed | Idle new jobs / cache byte changes | Mid-run jobs before deliberate source update | First result versus same-input rerun |
|---|---:|---:|---:|
| Hard selection | 0 / 0 | 1 | 1,083 different bytes |
| Soft selection | 0 / 0 | 1 | 3,291 different bytes |
| Only red active | 0 / 0 | 1 | identical |
| Only alpha active | 0 / 0 | 1 | identical |
| Target own alpha lock | 0 / 0 | 1 | identical |
| Background | 0 / 0 | 1 | 3,075 different bytes |

Every subsequent lower-source notification starts exactly one further job.
Selection bounds determine Blinds' permutation geometry at operation start.
Selection coverage and active components/target alpha lock affect the final
shadow merge. Freezing the start mask, or filtering the whole drawable and only
then applying a mask, does not reproduce these results.

## Scalar adapter

`app/painter/filter-context.{hpp,cpp}` owns no application objects, callbacks,
buffers, or files. Its row merge allocates nothing and is noexcept. It preserves
old `REPLACE_INTEN` integer multiplication/blending/division, including signed
rounding, alpha-zero hidden-color retention, and the fact that inactive alpha
still uses the computed mixed alpha for the color ratio. Exact input or shadow
aliasing is supported; invalid arguments return before writing.

Selection geometry uses half-open image bounds and wide offset intermediates.
A globally empty selection permits the whole drawable; an outside nonempty
selection remains selected with an empty local intersection. An allocation-free
accumulator accepts contiguous mask chunks so an owner can discover unknown
global bounds without calling a synchronous whole-image bounds scan. The owner
must bound each chunk and restart the accumulator when its snapshot changes.

Normal and focused ASan/UBSan runs compare all 304 native original/shadow/mask/
output records byte for byte, plus all 304 native bounds, aliasing, varied chunk
sizes, invalid inputs and bounded mask discovery. Reports are
`filter-context-native.json` and `filter-context-native-asan-ubsan.json` under
`migration/tests/`. The compact archive is independently extracted and retested
by `filter-context-native-bundled.json` and
`filter-context-native-bundled-asan-ubsan.json`, with identical results.

An additional 117,440,512 comparisons compile the exact extracted old C function
and macros and cross all original alpha, shadow alpha and mask bytes at seven
opacity values. Normal and ASan/UBSan reports are
`filter-context-scalar.json` and `filter-context-scalar-asan-ubsan.json`. These
checks validate scalar translation, not context scheduling or UI behavior.
The initial `*-sanitizers.json` attempts retain LeakSanitizer's unsupported
ptrace/thread-scan failure. The passing runs explicitly disable leak scanning;
they make no leak or full-application sanitizer claim.

The final adapter and test runners use the project's C++14 standard. A draft
used C++17 `std::clamp`; the equivalent min/max expression removes that extra
language requirement. The four final current-source reports are
`filter-context-cxx14-native.json`, `filter-context-cxx14-native-asan-ubsan.json`,
`filter-context-cxx14-scalar.json`, and
`filter-context-cxx14-scalar-asan-ubsan.json`. All 304 native records and
117,440,512 source comparisons pass again normally and under ASan/UBSan; the
native runs materialize the immutable bundle. Earlier reports remain historical
evidence for their recorded source hashes.

## Owner-phase integration contract

The original shadow path copies active components once in
`gimp_drawable_real_apply_region()`, then calls synchronous `combine_regions()`.
Its `pixel_regions_process_parallel()` receives no progress callback. The
single-thread loop and thread-pool wait do not dispatch the main loop. Ordinary
FilterLayer drawable Undo is discarded by old `gimp_image_undo_push()` because
the target is not editable, before dirty/Undo stack publication. No observed
UI event is interleaved through the pixel merge.

Cooperative import must therefore capture one coherent final-merge context,
then retain it throughout chunked import and atomic publication. It must not
sample changing selection/components separately for each chunk or restart a
Filter job for a context-only edit. Bounded capture may retry if the context
changes during capture; definition/lower-input/lifetime generation checks remain
separate. Unknown mask bounds and mask copying belong in admitted owner-thread
quanta. No GObject, GEGL graph, BindingStore, or owner closure crosses to workers,
and GEGL evaluation never starts or waits for an executor.

The integrated child route returns shadow-equivalent pixels produced with the
start ROI. A scoped private-PDB shadow-merge override can retain the actual
native shadow before modern compositing and cleanup, preserving the bundled
plug-in transformation. Inverting an already merged drawable is not assumed to
recover the required shadow. Start background/ROI and final merge context are
distinct scalar/snapshot phases.

## Invalid attempts retained

Three earlier captures are under `migration/tests/legacy-filter-context-initial-*`.
Their `INVALID-EVIDENCE.md` files explain missing lazy projection reads,
unsettled multi-tile setup, and incorrectly retained selection bounds. The
stale-mask report's original `passed` status is explicitly invalidated; no
selection/context acceptance is counted from it. Each exact failed harness was
reconstructed and its hash verified against the original report. Original
reports and raw outputs were not rewritten. The corrected corpus above is the
only context oracle accepted by the current fixture-check runner.

For review and source control, the corrected corpus is sealed in
`migration/fixtures/legacy-filter-context.tar.gz` (1,281 members), with
`legacy-filter-context.tar.manifest.json` recording every member's size/SHA256
and the archive SHA256. All three invalid attempts are preserved separately in
`migration/tests/legacy-filter-context-invalid-attempts.tar.gz` (3,550 members)
with its corresponding manifest. Every original raw file remains locally
available; no original pixels or reports were deleted or rewritten. Commit
the archives/manifests, not the thousands of unpacked redundant files.

`run_filter_context_checks.py` requires only the committed corrected bundle. It
extracts into an isolated temporary directory, validates the archive and every
member before running the C++ fixture tests, then removes that temporary copy.
The shared extractor rejects links, special files, duplicate/unsafe paths,
unexpected/missing files and changed sizes/hashes, and bounds extraction size.
Both the accepted and invalid bundles have been fully re-extracted and verified.

In the restored migration workspace, rerun with new report destinations:

```
python3 migration/tests/run_filter_context_checks.py --output /tmp/filter-context-normal.json
python3 migration/tests/run_filter_context_checks.py --sanitize --output /tmp/filter-context-asan-ubsan.json
python3 migration/tests/test_filter_context_scalar.py --output /tmp/filter-context-source.json
python3 migration/tests/test_filter_context_scalar.py --sanitize --output /tmp/filter-context-source-asan-ubsan.json
```

The extracted-source comparison additionally needs the pinned old checkout.
The compiled scalar/native tests need no GIMP application build; fresh oracle
capture still uses the recorded restored old build environment. All native-test
phases use the shared build lock.

## Native Blinds owner slice

The common typed FilterSlot now contains FilterOwnerContext, with admitted,
contiguous original-input and selection-mask storage. No extra object store or
worker-side GObject is introduced. Unknown global bounds and final coverage are
read in bounded quanta; a change during either capture restarts it. Components
and own alpha lock are read only at final sealing. After sealing, context-only
changes neither restart import nor launch another job. New input/definition and
owner lifetime invalidation remain separate and discard unpublished buffers.

The original 240 alpha-bearing native records exercise every GrayA/RGBA active
mask, four selection conditions and target/synthetic ancestor lock probes. The
24 old live records and six expansion records test both context phases; actual
process tests compare these full results again at zero/nonzero target offsets,
including replacement of the image selection as performed by XCF loading.

The compact expansion archive is separately pinned by its manifest and capture
report. Three mid-PDB scenes begin with x=[21,42), y=[3,60) in a 63x63 target and
then expand, clear or soften selection. All 2,772 unwritten shadow pixels outside
that start ROI are zero; expansion changes alpha outside the ROI in 2,208, 2,426
and 2,038 pixels respectively. This is why filling unwritten shadow with input
would fail compatibility even though input must be retained outside final
merge coverage. No fixture was generated from the new implementation.

Timing reports retain actual end-to-end and owner/heartbeat measurements. Pixel
budgets alone do not certify responsiveness, GEGL allocation/destruction or
filesystem stalls. The broad latency, platform, UI-progress and remaining
procedure gates stay open.
