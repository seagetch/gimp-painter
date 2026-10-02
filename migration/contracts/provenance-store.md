# Typed ownership of XCF provenance

WBS31.016 removes free-form Painter state attachments. Design§8.4.1 requires
one C++ implementation ownership mechanism: a typed slot in BindingStore.
It prohibits arbitrary string-key/type pairs, private placement-new Impls and
reopening a live store for late registration. These requirements are unchanged.

The import/save provenance payload now belongs to `GimpPainterProvenance`, a
private GObject with one `ProvenanceSlot`. The payload itself exists only in
BindingStore. A native GimpObject owns a single, specifically typed child
reference in its existing private C data. That reference is ordinary GObject
composition, not another C++ Impl registry: the private attachment seam accepts
only this exact child type, permits one construction-time attachment, and
cannot replace an existing child. No GimpObject instance or class ABI changes.

A separate child is necessary because an ordinary native object can receive
its first XCF provenance after its derived Clone/Filter store is already
active. Inserting another slot in that active store would violate the explicit
construction/activation contract. This design does not make activation
idempotent or permit late slot registration. Plain POD/value fields already
owned by native C types remain native fields; this change does not use that
fact to exempt the Painter provenance implementation from the common store.

Named C facades identify fixed bytes/text fields with type-specific enums.
There is no arbitrary key, arbitrary template type or raw Impl lookup API.
C++ adapters call these same C facades. Getters do not create state and return
owned copies/references. The record sequence is a new GPtrArray containing
references to immutable GBytes: changing either caller's container cannot
change retained provenance. No bytes are reinterpreted as current definitions.

Native dispose marks the attachment seam closed before closing the child's
store. Close is idempotent and retains immutable values for required reads
until finalization. Mutators reject closed owners. Native finalization marks the native owner unavailable before detaching
and unreferencing the child. A private lease guard rejects finalizer reentry
before attempting to retain a refcount-zero native parent. Child finalization destroys its common store.
Neither child nor Impl retains the parent, so there is no ownership cycle.
No job, thread, signal subscription or asynchronous cleanup is introduced.

Setters retain the native owner and child for the entire operation. Replacement
publishes the new value before releasing old resources. Copy snapshots all
source fields, publishes all selected target fields, then releases overwritten
resources while both native owners remain leased. This matters even for plain
GBytes: a caller-provided free function may synchronously reenter, close the
owner, replace a field, or drop the last caller-owned reference.

The native duplicate/copy whitelist is unchanged: property records, original
extension/header, external-reference origins, original name/type and unknown
record bytes. Whole input files, original offset, dialect, transient saved ID
and restoration/current-definition dictionary are not copied. Existing target
values are preserved when the corresponding source provenance is absent.
Current native Clone/Filter definitions and validated modern XCF metadata keep
their existing precedence over immutable import provenance.

The layer dialog is a separate remaining31.016 slice. Native upstream last-file
context keys are not independent Painter implementations and remain untouched.
This checkpoint does not claim every source-family31.016 acceptance row or the
complete XCF saved-field matrix is finished.

## Native evidence

The normal Meson and focused sanitizer runs pass all42 cases: eight typed
provenance tests,14 real XCF open tests and20 Save/reopen/native duplicate
roundtrips. Dedicated ownership cases cover no-allocation reads, invalid
owner/field rejection, immutable record containers, the exact copy whitelist,
read-after-close, refused mutations, replacement reentry, last caller reference
loss, copy-time close after atomic publication and parent-finalization reentry.
Existing integration cases retain unknown records and definitions through actual
CloneLayer/item/image duplication, conversion, current editing, Save and reopen.

The focused builder instruments25 units and recompiles36 additional production
C++ units for RTTI compatibility only. ASan/UBSan/float-cast-overflow and vptr
checks are enabled; no leak or whole-dependency instrumentation claim. Selected
source/header hashes are unchanged through compilation and execution. The
checkpoint separately labels supplemental native ABI headers sampled after the
run, and seals them with the selected inputs in an immutable source archive.
The initial seven-case normal result is retained separately from the final
eight-case run after the finalization guard was added.

## Saved-field extension

The fixed text-field enum additionally carries a SAVE_REFUSAL diagnostic for
unsupported or unreadable native GEGL effects. It is copied with immutable
provenance so image/item duplication cannot bypass a lossy-Save refusal. It is
not another attachment mechanism or an executable definition, and is not written
as a new serialization namespace. Native effect unknown property sequences use
the existing immutable-record field and the ordinary XCF effect writer.
