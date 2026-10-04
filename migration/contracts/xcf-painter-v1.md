# Painter XCF v1 preservation contract

2026-10-02. This application checkpoint adds ordinary File Save → File Open
roundtrips to the [reader contract](xcf-open-integration.md). It is not a claim
that migration sections 10–12, all historical metadata, every mode, or all
corruption/GUI/platform gates are complete.

## Standard container and explicit provenance

The writer uses standard XCF header/property/offset/tile structures. It never
writes colliding old tags 32/33 or casts old mode numbers into modern enums.
Independent Clone/Filter semantics live in persistent `gimp-painter-item`
parasites; `gimp-painter-image` declares the dialect. Custom layers/modes require
at least XCF v11. Images with paths use v18 standalone path records, so path
metadata and identities do not disappear through the deprecated image-level path
encodings. Otherwise upstream version selection remains applicable.

Each namespace contains 12 magic/version bytes `47 50 58 43 46 00 00 00 01 00 00
00`, followed by canonical, little-endian GVariant `a{sv}` serialization. Parsers
validate normal form, unique dictionary keys, dictionary version `uint32(1)`,
field types and bounds. Normal form alone does not make duplicate keys
unambiguous. Any duplicate key, including an unknown key, retains the original
capsule but prevents rewriting; preflight fails before replacing a destination.
Unaligned wire storage is copied from GVariant's aligned serialization. The
image dictionary also has `dialect = "gimp-painter-standard-v1"`. A complete
image-property sequence with a recognized marker can choose standard parsing
before later object-work limits. Later duplicate namespace declarations take
precedence. Explicit recovery selection overrides the marker. The marker is not
a prerequisite for opening existing standard files.

Unknown dictionary fields survive v1 edits. Unknown future namespace bytes or unknown/missing/wrong-typed item kinds on
an ordinary proxy remain byte-for-byte intact and warn on loading. They cannot
be overwritten with current Clone/Filter/custom-mode semantics: preflight fails
before destination replacement. `gimp-painter-origin` is a separate v1 archive
when an unknown semantic namespace must remain untouched. An unknown origin
archive version likewise cannot be overwritten.

## Current model versus original evidence

The authoritative `kind` is `ordinary`, `clone` or `filter`, derived from the
current object, not the imported object's old class. An optional `legacy-mode`
uses the explicit Painter mapping API. The standard fallback mode is Normal
legacy; it is only an ordinary-reader fallback. Reopening with this build
restores the exact mapped mode, not flattened pixels. Other GIMP versions do not
implement these independent layers or operators and are not promised semantic
roundtrip compatibility.

`original-header`, `original-properties`, `original-owner` and
`original-extension` archive imported records independently of current state.
They retain ordering, duplicates, unsupported records and raw bytes. The first
archive is retained instead of recursively embedding each subsequent XCF.
`opaque-record-sequences` is an ordered array of byte arrays containing unknown
property record sequences encountered after import; identical sequences are
not appended repeatedly. Image/layer/channel/mask/path property contexts are
covered. Immutable GBytes/string provenance and unknown-record arrays survive
item duplication and conversion through a lower-level core helper with no
core→XCF dependency. Original owner/type is archival only; converting a Clone
to an ordinary layer does not turn it back into a Clone on reopening.

The complete imported source remains owned by the in-memory reader snapshot.
The writer archives property/header/extension records, not another full original
XCF and pixel hierarchy inside each new XCF. Modern GEGL effect-specific unknown
records, arbitrary malformed tile recovery and the complete historical metadata
inventory remain separate audit gates. Image Duplicate now copies the same
immutable image-level provenance, independently of native parasites.

## Stable references and cache state

Every saved item receives a unique nonzero per-file ID, preferring a prior
successful save's identity, then native tattoos, and repairing collisions/zero
values without mutating the live model. Selection IDs are reserved even for
empty selections. The same IDs are written to native tattoos and reference
metadata. A successful close retains the chosen IDs for subsequent saves.

Clone metadata stores source ID, state, expired flag, lazy-name policy, nullable
pending/current names and original imported lookup name. A nonmutating snapshot
holds the source during synchronous save. Reloading resolves an exact unique ID
after the full hierarchy exists. A missing ID stays expired/unresolved with name
lookup disabled, so an unrelated same-name layer cannot be substituted. Legacy
name resolution retains its original policy only when that policy was actually
saved. Restoration does not resize, project or overwrite the committed cache.

Filter metadata stores current procedure, raw definition presence/bytes,
converted argument-model presence, typed argument tree, current/cache generation,
cache completeness and diagnostic execution state. The immutable original
`PROP_FILTER_SPEC` payload and the edited typed execution arguments are separate:
editing supported options does not regenerate the old raw bytes. A null converted
model never asserts that unsupported original arguments were empty. Explicitly
empty models/arrays and null models/arrays remain distinct.

Typed argument serialization uses `a(sbv)`: type name, null flag and typed payload.
Scalars include exact float/double bits, numeric widths, nullable byte-preserved
strings, STRV, numeric/raw arrays, GBytes and GVariant. Nested arrays and object
references use the bounded core importer, not legacy pointer-copy GValue code.
Object descriptors retain declared type, original runtime ID, was-set/expired
state and optional file ID; self-image and same-image item targets are resolved,
expired descriptors remain descriptors. Arbitrary object internals are not
fabricated. Unsupported values fail preflight; uninterpreted loaded metadata is
retained. Limits are depth 32 and 65,536 aggregate slots/references. Ordinary Save uses the
[multipart transport](xcf-multipart-transport.md) for regenerated v1 capsules
larger than 1 MiB, streaming the same logical capsule up to an explicit 64 GiB
transport work bound without changing field semantics. The internal inline-only
opt-out retains its prior 256 MiB preflight bound. Neither bound is a whole-input-file limit or evidence against a legacy
interpretation.

Filter pixels are the committed cache snapshot. After all topology and definitions
are restored, the core normalizes saved generation lineage into a fresh runtime
epoch. Complete/equal caches remain clean with zero initial runs; stale/incomplete
caches schedule a new evaluation. In-flight worker output is never serialized as
a completed cache. Persisted counters are validated, not trusted as job tokens.

## External and unavailable references

Live Clone sources and Filter image/item arguments outside the saved image are
saved as explicitly unresolved descriptors, with committed caches retained. Save
does not mutate their live session bindings. No external image is embedded or
looked up by file, network, matching local name, tattoo or runtime ID on reload.
The active file-local source ID is zero; object argument descriptors are expired.
An explicit future UI relink may choose a source, but the loader never guesses.

`external-reference-origins` stores diagnostic role, declared/actual types,
original object/image runtime IDs and optional already-visible object/image names.
Runtime IDs are provenance, not persistent lookup authority. Image names are
omitted whenever a source, import or export file handle is nonlocal. Local
display names are restricted to safe basenames; no derived file paths/URIs,
credential userinfo or remote root-query text are stored. Unknown object internals are still not fabricated: objects
without a supported image/item identity require opaque preservation or explicit
preflight failure.

The immutable origin array is deduplicated by serialized fingerprint plus exact
value comparison, bounded by the argument/parasite limits. A successful save
retains it as immutable item provenance so closing an external source and then
resaving the original live image does not lose its recorded identity. Cancellation
never publishes new provenance. Item duplication shares only these diagnostic
bytes, never an active external binding or guessed target.

## Save transaction and failure

Metadata/reference encoding and committed-buffer duplication happen before
`g_file_replace`. No custom unsupported value can truncate a preexisting file.
The synchronous transaction retains objects and immutable definitions/buffers,
observes dirty/notify/topology/pixel changes, and compares nonmutating Clone and
Filter checkpoints. Reentrant edits abort rather than mix old and new semantics.
A full model clone is not used, avoiding accidental source/ID remapping.

Save progress is cancellable. Writes and seeks receive the cancellable; both
normal and final-closing progress checkpoints check cancellation/model changes.
Failure cancels replacement close and preserves the primary write error rather
than replacing it with close success. A caller-provided arbitrary stream may
already contain partial bytes; atomic destination preservation is specifically
the GFile replacement contract. Successful close is required before committing
saved IDs. The existing image metadata temporary-parasite cleanup and all upstream
failure branches have not received a blanket audit.

## Reproduction and evidence

Build `app/tests/{xcf,painter-xcf-open,painter-xcf-roundtrip}` under the documented
Debian 13 environment, holding `/tmp/gimp-painter-build.lock` for shared builds.
Run `meson test -C build-debian13 xcf painter-xcf-open painter-xcf-roundtrip
--no-rebuild --logbase painter-xcf-writer-checkpoint`.

The roundtrip suite covers both genuine Clone fixtures and source edits after
reload, Filter raw/current argument separation and pending/fresh caches, exact
ordinary projection/mask/channel/mode, duplicate→edit→three saves with unknown
records and bounded size, conversion to ordinary, newly introduced unknown wire
records, future-version payload retention, sixteen typed/nested/expired argument
slots, seven cancellation/reentrant opacity checkpoints, two low-level definition
changes, and five write-failure positions preserving a preexisting sentinel.

The focused ASan+UBSan runner is
`migration/tests/run_painter_xcf_roundtrip_sanitizers.py`. It instruments the XCF
reader/writer/probe/argument/source helpers, GimpItem, Clone/Filter adapters,
BindingStore and the real application test harness; remaining upstream
GIMP/dependencies are uninstrumented. Leak detection is explicitly disabled.
Test summaries and sanitized logs are `migration/tests/painter-xcf-writer-*`;
the focused report is `migration/tests/painter-xcf-roundtrip-sanitizers.json`.

Confirmed checkpoint: 27 normal application cases pass (upstream XCF 4, Open 13,
roundtrip 10); all 10 focused sanitizer roundtrip cases pass with exit 0 and no
ASan/UBSan diagnostics. Sixteen save snapshot prepare/free cycles verify observer
removal before transaction destruction, followed by live-object edits. The writer
uses common ObjectRef and Connection ownership, staging connections before vector
publication so allocation failure disconnects safely. Expected unknown-version recovery warning is exercised.


## Preservation follow-on checkpoint

Normal application coverage is 32 cases (XCF 4, Open 14, roundtrip 14); all 14
focused ASan+UBSan roundtrip cases also pass. The focus now includes
`gimpimage-duplicate.c`. Evidence is `painter-xcf-preservation-{meson.txt,
testlog.txt,testlog.json,sanitizers.json}` in `migration/tests/`.

- A valid explicit dialect marker resolves a genuinely dual-valid byte stream;
  explicit legacy recovery still wins, and an unknown marker does not resolve it
- Missing Clone IDs are retained as `unresolved-source-id` diagnostics with an
  inactive source ID. Neither matching names nor subsequently reused IDs can
  activate them; unresolved pending-name state is retained separately
- Image Duplicate → Save → Open retains unknown image-level records
- Typed model checks cover all sixteen scalar/array slots, nested values, expired
  descriptors, and a live path identity across repeated saves
- Unreadable typed models are current immutable opaque argument payloads owned
  by the Filter definition itself, separately from raw `PROP_FILTER_SPEC` and
  converted arguments. A nullable canonical variant wrapper retains a missing
  field distinctly from any field value. Duplicate without editing retains the
  current unknown model. Explicit null replacement clears it; definition Undo
  restores it and Redo clears it. This does not depend on an XCF qdata/revision
  heuristic. Save/reopen checks cover each state and meaningful cached pixels.
  A real supported Edge replacement completes before Undo; Undo restores the
  earlier opaque buffer and conservatively marks it stale, rather than claiming
  the newer procedure's cache belongs to the old definition. Missing fields,
  wrong-typed fields and nonnull zero-byte opaque wrappers remain distinct;
  invalid caller-owned wrappers fail before destination replacement
- The first replaced imported model/procedure is separately retained as
  `original-argument-model` / `original-argument-procedure`; later explicit typed
  values win over this archival state

Opaque argument models are never executed or fabricated as empty converted
arguments. This checkpoint does not turn unsupported definitions into executable
procedures. The core definition-revision getter remains a runtime-only API and is
not persisted or used as cache/job lineage.


## External-reference checkpoint

Normal application coverage is 33 cases (XCF 4, Open 14, roundtrip 15); all 15
focused ASan+UBSan roundtrip cases pass. Evidence is
`migration/tests/painter-xcf-external-{meson.txt,testlog.txt,testlog.json,sanitizers.json}`.
The new real file test includes a live cross-image Clone, external image/item
Filter arguments, live local arguments, duplicate local names and colliding
local tattoos. It verifies live bindings remain unchanged during save, then
unresolved descriptors and exact caches survive repeated reload/resave and
closure of the original external image before resaving the live original. A
synthetic credential-bearing source URI is not copied into any saved bytes.


## Schema and source-name privacy checkpoint

Normal application coverage is 34 cases (XCF 4, Open 14, roundtrip 16); all 16
focused ASan+UBSan roundtrip cases pass. Evidence is
`migration/tests/painter-xcf-schema-{meson.txt,testlog.txt,testlog.json,sanitizers.json}`.
The schema test exercises duplicate version, duplicate unknown field, duplicate
kind, unknown string kind and wrong-typed kind. Duplicate dictionaries cannot be
rewritten through a potentially different lookup/canonicalization interpretation.
Unknown item kinds remain ordinary proxies whose capsule and separate origin
archive survive edits and another save/open. Assigning custom semantics to such
a proxy refuses before replacement. External-reference scenes test both a
credential-bearing path URI and a root URI with a credential-like query.

The focused runner now instruments 21 translation units, including the shared
Filter scheduler and exact Edge/Gauss kernels. Focused C++ objects enable RTTI
and precede private native archives with their original members omitted, avoiding
mixed instrumented/uninstrumented COMDAT ownership. UBSan vptr checks remain
enabled; no diagnostic suppression is used. WorkAdmission is header-only coverage.
Source and interface-header hashes are captured at compilation and checked after
testing; the checkpoint has no changed sources. Remaining upstream libraries and
dependencies are still uninstrumented, and leak detection is disabled.


## Registered recovery-save route

Normal application coverage is 36 cases (XCF 4, Open 14, roundtrip 18); all 18
focused ASan+UBSan roundtrip cases pass. Evidence is
`migration/tests/painter-xcf-recovery-{meson.txt,testlog.txt,testlog.json,sanitizers.json}`.

`xcf_save_recovery_image()` invokes the registered `gimp-xcf-save` procedure with
the current three arguments: run mode, image and destination file. It inspects
the PDB return status and releases the returned value array. It bypasses the
ordinary `file_save()` wrapper so the live image keeps its original file
association and dirty state. `app/errors.c` uses the same helper and renames the
temporary backup only after a successful save. An obsolete drawable-array
argument can no longer shift the file argument or cause a failed save to publish
a stale temporary file.

The real application test reopens backups of both genuine Clone fixtures and the
Filter fixture. It checks source identity, metadata, exact committed layer caches
and projection, Filter raw bytes/typed arguments and zero initial executions.
Original source bytes, file association, dirty counter and dirty time remain
unchanged. Actual destination-open failure, metadata preflight refusal and the
crash caller's null-error path preserve a sentinel and return failure. The
production crash call site compiles in the native application archive. This
remains best-effort fatal recovery, not async-signal-safe recovery or a newly
implemented autosave schedule.

See [xcf-remaining-gates.md](xcf-remaining-gates.md) for the audited residual
implementation and application gates, including native owner/context corruption and full application lifecycle checks.


## Transactional semantic-envelope checkpoint

Normal application coverage is 38 cases (XCF 4, Open 14, roundtrip 20); all 20
focused ASan+UBSan roundtrip cases pass with stable source/header hashes.
Evidence is `migration/tests/painter-xcf-semantic-schema-{meson.txt,testlog.txt,
testlog.json,sanitizers.json,before.json}`. The isolated pre-fix preservation
translation unit from commit `c15722e45b` fails both new tests at their first
invalid scene: a custom layer was constructed. This controlled before/after
comparison uses the current application harness, not a complete old-commit build.
No shared source or native archive was replaced for the comparison.

Filter cache/definition envelopes and Clone reference envelopes are validated
before changing the ordinary layer into a custom type. Required fields cannot
be silently defaulted; Filter counters require `cache-generation <= generation
<= G_MAXINT64`, and diagnostic state must be a known enum. Clone state is checked
against pending-name presence, declared source ID and expiration. Missing targets
remain an explicit unresolved-reference case rather than a malformed envelope.
Optional source names remain optional; an empty pending name remains distinct
from no pending name. The core diagnostic source-name snapshot normalizes an
absent recorded name to an empty string, without inventing a target.

Common legacy-mode and original-name fields are also checked before replacement.
An invalid envelope stays an inert ordinary proxy retaining the exact capsule and
committed pixels. Its ordinary edits and duplicates can be saved/reopened while
keeping that capsule intact; applying new custom semantics refuses before
replacing the destination. Modern Filters are never marked loaded by the legacy
finish pass or an exception catch. Only successful validated snapshot restoration
certifies their cache freshness. Unreadable typed argument models continue to use
the separate core opaque-definition path.

Coverage includes 24 invalid Filter/common-field scenes, 18 invalid Clone scenes,
three valid maximum-counter states and four valid missing/empty Clone-name states.
The invalid scenes include missing/wrong types and semantically inconsistent
state/counters, with exact bytes/pixels through duplication and repeated saves.
Valid complete, stale and incomplete caches preserve their relationships after
runtime-epoch normalization.
