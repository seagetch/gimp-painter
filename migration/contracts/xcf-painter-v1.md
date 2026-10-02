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
validate normal form, dictionary version `uint32(1)`, field types and bounds.
Unaligned wire storage is copied from GVariant's aligned serialization. The
image dictionary also has `dialect = "gimp-painter-standard-v1"`. A complete
image-property sequence with a recognized marker can choose standard parsing
before later object-work limits. Later duplicate namespace declarations take
precedence. Explicit recovery selection overrides the marker. The marker is not
a prerequisite for opening existing standard files.

Unknown dictionary fields survive v1 edits. Unknown future namespace bytes on
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
records, arbitrary malformed tile recovery, image-duplication provenance and the
complete historical metadata inventory remain separate audit gates.

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
retained. Limits are depth 32, 65,536 aggregate slots/references, and one parasite
below 256 MiB. These are explicit serialization limits, not whole-input-file
limits or evidence against a legacy interpretation.

Filter pixels are the committed cache snapshot. After all topology and definitions
are restored, the core normalizes saved generation lineage into a fresh runtime
epoch. Complete/equal caches remain clean with zero initial runs; stale/incomplete
caches schedule a new evaluation. In-flight worker output is never serialized as
a completed cache. Persisted counters are validated, not trusted as job tokens.

## Remaining external-reference boundary

A live Clone source or Filter object argument outside the saved image currently
fails preflight without changing the destination. Same-image IDs and expired
reference descriptors are supported. Persisting live cross-image references
without rebinding by a coincidental local name remains an explicit migration
gate; this refusal is not a permanent feature policy or a full save-compatibility
claim. Image-level duplication of unsaved origin metadata and edits replacing an
uninterpreted argument model remain targeted follow-on checks.

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
