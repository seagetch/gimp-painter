# Painter XCF multipart storage integration

This implementation extends `xcf-painter-v1.md`, design §4's versioned noncolliding
metadata, and `provenance-store.md`'s owner-local immutable values. It preserves
the existing v1 dictionary schema and its semantics; it changes only transport.
Ordinary Save uses the integrated transport for regenerated v1 capsules larger
than 1 MiB. The explicit internal FALSE option retains inline-only refusal for
compatibility/failure tests; there is no process-global switch.

## Identity and framing

- Existing `gimp-painter-image`, `gimp-painter-item`, and
  `gimp-painter-origin` v1 capsules remain readable and writable unchanged.
- A multipart manifest uses the same semantic parasite name and persistent flag,
  but a distinct 12-byte magic `GPXCF\0\0\0\2\0\0\0`. Its fixed binary body is
  little endian: owner class u32, owner ID u32, namespace u32, block size u32,
  logical length u64, block count u32, reserved zero u32, SHA-256 digest 32 bytes.
  The digest covers the complete old v1 capsule, including its old magic. There
  are no pointers, runtime IDs, file paths, compression, or recursive manifests.
- Owner classes: 0 image, 1 layer (including its ordinary/custom/text/group
  subclasses), 2 channel (including selection and effect masks), 3 layer mask,
  4 path. Image ID is zero. All item IDs are the unique, nonzero IDs already
  allocated by the save transaction and written to native tattoo properties.
  Namespace is 0 image, 1 item, 2 origin; image/item combinations are checked.
  Layer-mask classification precedes channel classification (GimpLayerMask is a channel subclass). Validation happens after all owner properties, including tattoo/group tags.
  Same-image reference resolution retains its existing separate validation.
- Chunk parasite names are exactly
  `gimp-painter-chunk-v1/<namespace>/<digest-hex>/<index-8hex>`.
  Each payload starts with `GPXCC\0\0\0\1\0\0\0`, then repeats the manifest's
  owner class/ID/namespace/block size/logical length/block count/reserved/digest,
  then index u32, reserved zero u32, and at most 65,536 payload bytes. All except
  the final block have exactly 65,536 bytes. Names and payload identity agree.
- The canonical writer emits one manifest followed by its chunks in index order.
  The reader accepts a unique complete set in arbitrary physical order and
  parasite grouping: stock GIMP can reorder its hash-backed parasite list.
  Index metadata has the explicit count bound below and may spill to disk.
  Each record is framed inside ordinary `PROP_PARASITES`; the writer emits one
  such property for each manifest/chunk so no property size exceeds the
  existing 32-bit wire length.
  Every inner length is checked against both its property and immutable source.
- Maximum count is 1,048,576 chunks (64 GiB logical bytes) per transport; at most
  three semantic transports per owner; no nested transports. A retained inert
  record can additionally have one suppression carrier. The owner-local input
  work bound therefore permits six times (1,048,576 + 1) inner records, including
  carriers; this does not double the logical-capsule byte limit. Existing dictionary/typed
  argument depth and work bounds remain separately enforced. Exhaustion produces
  explicit inert retention or preflight refusal, not fabricated empty semantics.

## Retention and activation

Only a complete, exact, uniquely indexed set whose digest and owner tuple match
may supply the v1 dictionary. Duplicate indices, conflicting manifests, absent, foreign-owner,
wrong-namespace, malformed, unsupported-version and trailing records stay inert.
The interceptor retains their entire original parasite wire framing and bytes,
including duplicate names and order, before native parasite-list insertion could
collapse them. Interception is per inner parasite record, retaining the original
name length/name/flags/data length/data bytes. Ordinary parasites mixed into the
same PROP_PARASITES continue through native loading and are written once, not
copied again as part of the transport archive. Arbitrary user parasites with a reserved transport/carrier prefix are likewise
retained; they cannot be silently claimed as active transport.

An inactive record must remain inactive when duplication, conversion or tattoo
repair changes its native owner identity. Before resaving an inert record, the
writer adds a suppression carrier named exactly
`gimp-painter-inert-v1/<lowercase-sha256-of-complete-inner-record>`. Its persistent
payload is the 12 bytes `GPXCI\0\0\0\1\0\0\0` followed by that 32-byte digest.
Exact name, magic, flags, length and digest must agree. This carrier is independent
of owner IDs and can only suppress activation; it can never confer validity.
Carriers are associated by content, so physical reordering/grouping has no effect.
Invalid/future carriers remain inert exact bytes, are never recursively wrapped,
and confer no authority. Existing valid carriers prevent additional emission;
duplicated original records and original carrier framing are retained. Digests
are prepared with cancellation before destination replacement. Deliberately
removing or damaging a carrier in an externally edited file leaves independently
valid sets subject to ordinary validation; the format does not claim tamper proofing.

A fixed typed transport-record field in the existing `ProvenanceSlot` stores
immutable GBytes sequences. A fixed three-value namespace enum identifies the
validated logical capsule snapshots. These are fields of the existing common
BindingStore child, not another qdata/registry or late registration. Getters never
create state; setters publish by move-temporary/swap before releasing previous
GBytes, including custom finalizer reentry. Closing preserves read access while
rejecting mutations. Native copy/convert shares immutable records; active known
capsules are re-encoded for the target's save-time owner ID. Inert records retain
the original owner tuple and exact bytes; they cannot establish new references.

Known current Clone/Filter state still wins over archived old evidence. Invalid
semantic transport on an ordinary proxy is retained and resaved inert, while
attempting to replace it with incompatible active custom semantics fails before
destination replacement. Repeated saves do not recursively archive transport
records inside their own `original-properties` field.

## File-backed encoding and atomic save

Logical capsules are snapshotted into private temporary files in 64 KiB blocks,
with read-only immutable ownership and deterministic close/unlink cleanup.
Large byte arrays use GVariant backed by immutable GBytes instead of the copying
fixed-array constructor. Canonical aligned GVariant serialization is streamed
into the file with bounded primitive scratch blocks and bounded container
framing tables; it must not force a whole logical payload heap serialization.
Unknown v1 fields/types remain supported by the same canonical serializer.
Newly encoded top-level dictionaries sort their unique keys; this stabilizes
padding/framing across reload without changing v1 field meanings. Numeric-array
endianness conversion uses bounded input blocks, and immutable Filter scalar
snapshots expose a lifetime-bounded read-only borrow instead of a GValue copy.
Temporary stream flush and close both finish before mapping/sealing, and no
writable descriptor is retained by a sealed snapshot.

Native read interception bypasses heap-allocating `GimpParasite` construction for
transport records and reconstructs only validated capsules. Native write
interception streams prepared manifest/chunk records directly, rather than
materializing every chunk in a parasite list. Save snapshots and digests finish
before `g_file_replace`; writing and final close use the existing cancellation
and changed-model checks. Any preparation, stream, disk, cancellation, digest or
close failure preserves the original destination and does not publish saved IDs.

Image/item/topology and leaf/mask-buffer observers are connected before metadata
preparation. Group projections fill derived caches on read; only their own cache
buffer observers start after all committed buffer copies. Actual child edits and
group properties remain watched throughout. The file entry point retains all
borrowed object arguments before reentrant progress callbacks.

The old inline compatibility path (sole v1/future records up to 256 MiB) still
constructs native parasites and copies their bytes. Large active model strings,
GLib dictionary keys and upstream drawable state may also allocate their native
representations. Mutable typed argument strings/arrays retain the former 256 MiB
aggregate payload allowance (including string terminators), even when their
transport is larger. Pointer tables remain under the existing structural bounds
and use checked allocation separately; immutable GBytes/GVariant references are
outside that mutable budget. Both codec allocation and the core snapshot import's
second bulk copy use recoverable allocation. Over-budget or allocation-failed
arguments remain opaque with raw definitions and committed cache intact. Known
Clone/Filter name fields retain the former inline materialization allowance;
failed binding restoration refuses a lossy resave. Aggregate child extraction
avoids GVariant's borrowed-name unpacking path, which can otherwise force a whole
unserialized argument entry into heap storage.

The bounded-copy claim is specifically the multipart transport and streamed
immutable serialization, not every upstream model allocation.
Mapping avoids a proportional heap copy, but alone is not bounded residency.
Tests must measure peak RSS, allocated disk bytes, live temporary files and
cancellation cleanup at 256 MiB + 1 and larger representative input, disclose
mapped pages and framing overhead, and avoid claiming fixed memory merely from
low allocator totals. Platform unlink behavior remains separately qualified.

## Acceptance and remaining limits

Independent framing/byte tests cover all three namespaces, all owner classes, every
length/count boundary, reordered/regrouped complete sets, duplicate/missing/foreign/damaged chunks,
canonical serialization against GLib, and custom GBytes finalizer reentry.
Native tests cover real Save/Open beyond 256 MiB, v1 compatibility, unknown
fields/raw/property ordering, duplicate/convert, stable and unresolved references,
invalid inert exact retention, cancellation at preparation/write/close and
injected disk/write failure with a preexisting destination sentinel. Focused
sanitizer scope and platform/resource limits are reported explicitly.


The exact current result manifest is
[`../tests/xcf-multipart-storage/acceptance.json`](../tests/xcf-multipart-storage/acceptance.json).
It links compact normal, focused sanitizer, scalar-codec, and public-PDB resource
reports with input/executable hashes. Published genuine legacy fixtures are reused
by path and generated large records are deleted after each test; no redundant
fixture or source archive is bundled in this checkpoint.

The native matrix covers standard files, all five owner classes plus ordinary,
group, Clone and Filter layers; duplicate followed by explicit re-edit; physical
reordering and mixed parasite regrouping; six malformed-set families; opaque
future/unknown-kind capsules and nondefault flags; previously foreign sets after
in-place identity repair, duplication and conversion; preflight/mid-preparation/
write/close cancellation, actual temporary-file size failure, six streamed write
failure positions and final close failure. Each failure retains destination
sentinel, saved identities and the relevant dirty state, and all transport temp
snapshots are released. Read-only scalar snapshot borrows survive current-model
replacement and owner destruction. A real >256 MiB typed-array Filter record is
opened as an opaque argument model with exact definition/cache and zero executor
runs, then ordinarily saved and reopened unchanged; huge GBytes retain the
original immutable backing. Controlled source-level allocation-failure tests
exercise checked string/array/STRV copies, including partial cleanup, separately
from the native budget-rejection proof. Progress callbacks can release the caller's
last image/file/progress references safely.

Ordinary public `gimp-xcf-save`/File Save invocation, followed by native Open,
executes three exact 256 MiB + 1 and 512 MiB + 1 unknown-record roundtrips.
One compact baseline Save/Open establishes the newly created Filter's initial
import provenance before attaching the large unknown sequence. The
unknown archive remains a single exact SHA-256-matched sequence, complete-file
size is stable, Clone source identity resolves, and Filter raw bytes and typed
arguments remain unchanged. These are native application API tests without an
interactive desktop, not a GUI latency claim.

Peak anonymous/file-backed RSS, resident mappings, temporary FDs and actual
filesystem allocation delta are reported separately. Linux `/proc/map_files`
allocation lookup is denied here, so open-FD allocation is explicitly incomplete;
filesystem free-block delta observes actual allocation but includes other users
of the same filesystem. Whole-map GLib validation can fault pages proportional
to payload size; page release is advisory and does not certify fixed total RSS.
Native legacy inline capsules still have their older copying path. No Windows,
macOS, big-endian hardware, full metadata-field ledger, arbitrary malformed graph,
interactive responsiveness, or whole-dependency/LeakSanitizer gate is closed.

Whole-image duplication intentionally retains historical top-level Clone source
identity until expiry or explicit source re-edit. Whole-group duplication retains
its existing hierarchy-local remapping. This task does not silently change that
legacy behavior; the duplicate/re-edit test asserts original identity and expiry
before explicitly choosing the copied source. Source comparison uses the pinned
legacy revision `afa43fae3e920210146abed514f136fd49f671b5`, not a new old-runtime replay.

The resource report stopwatch includes compact Filter provenance priming; its
process elapsed time also includes fixture generation and application setup. It
is not presented as an isolated Save benchmark or interactive latency measure.
