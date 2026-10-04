# Registered metadata and lossless FilterLayer editors

This is Phase C, `30.001/parameter-schema-editor`, of the
[parameter schema contract](filter-parameter-schema.md). The four existing
isolated routes now connect scalar, choice, flag, matrix and channel controls to
canonical schema keys. The saved slot mapping and legacy scalar domains remain
an explicit, separate overlay. Core `GimpProcedure` metadata is used directly;
libgimp `ProcedureConfig` and its non-public auxiliary values are not imported.

## Registered metadata and ownership

A dialog reads only metadata already registered by normal plug-in restore.
Opening, switching, validating and closing the editor never query a plug-in,
wait for a child, or dispatch an acquisition main loop. Missing or incompatible
metadata produces an explanation and blocks new or changed settings; old values,
no-op OK, Cancel and completed pixels remain available. No describe protocol,
session cache, new scheduler state, or saved schema is introduced.

Normal restore frees temporary `GimpPlugInDef` objects before publishing the
procedures to the PDB. The plug-in manager therefore records bounded provenance
for exactly four trusted providers immediately before disposing those defs.
This uses a typed slot in the common `BindingStore`, registered during manager
initialization, activated after chained construction and closed on disposal.
The record keeps at most six original procedure references, fixed provider paths,
registration cardinalities and registration timestamps, not the temporary defs.
`needs_query` remains true after a successful first restore in this source;
only the completed restore hook can attest that historical flag. A dialog cannot
capture an in-progress definition. Providers with initialization, extra or
missing registrations, file/batch procedures or wrong trusted paths are rejected.

The immutable descriptor then checks unique PDB and manager registrations,
retained manager/PDB/procedure identity, public/hidden pair identity and the
existing core binder's exact names, types, context positions, defaults, ranges
and choices. Input slots after context 0/1/2 may reorder. Metadata text, choices
and exact supported pspec classes are bounded before cached default access.
Raw choice defaults are copied and compared with cached values, so changing an
underlying default in place cannot evade validation. Auxiliary parameters not
published to the core PDB remain unknown and are not invented.

Each dialog retains its owners and immutable descriptors. Existing typed weak
`DialogHook` connections mark the relevant route stale on registration changes.
Changed commits reconstruct and compare the current descriptor, including
same-pointer pspec changes, and check the route and definition revision again
inside the transaction after signal-emitting Undo creation. Close detaches state
before releasing owners; no acquisition callbacks or child jobs survive it.
Execution continues to re-query and validate the trusted provider in the existing
private helper. This editor descriptor is not execution authorization for an
arbitrary saved procedure.

## Changed fields and preserved definitions

Integer controls parse and compare integers directly. Binary64 entries retain
round-trip ASCII text, finite subnormal values and exact negative-zero behavior.
Invalid new syntax, nonfinite values, overflow and underflow to zero are rejected.
The editor validates only changed fields on an existing definition. Unchanged
saved special values and unknown enums therefore survive unrelated edits.
Noncanonical signed flags retain their original integer until their meaning
changes; changing back before OK is a no-op. Matrix controls keep x-major saved
indices, and both 11/12-slot Convolution forms retain their arbitrary final slot.
Blinds continues to accept the legacy typed context provenance that its executor
already accepts; those context fields are not editable.

Opening and no-op OK borrow immutable values and do not materialize an argument
array. An actual same-route edit stages only changed exact-type fields and
publishes a bounded immutable snapshot patch through the existing definition and
Undo machinery. Untouched slots share their original scalar bytes, nested model,
null state and weak reference descriptors. Recorded reference type/ID/was-set
information is not reconstructed from a currently live object. Expired links
remain expired descriptors. Repeated edits flatten slot ownership rather than
retaining an expanding chain of patch models. Aliased untouched slots still keep
the original backing model, including its original replaced values, alive until
its last alias/Undo is released. The budget does not claim immediate release of
all superseded data. Original raw and opaque metadata
are retained separately from the typed snapshot. Successful edits flush the
image and create one definition Undo.

A failed guard before Undo leaves the model and history unchanged. A guard may
also fail because an Undo signal synchronously changes the provider. In that
case no staged definition or cache is published, and only this transaction's
identifiable, still-top Undo is removed when the definition remains current.
The existing `gimp_image_undo_push()` may already have emitted dirty notifications
or expired redo/old history. This narrow editor guard does not reconstruct that
history or undo unrelated reentrant work. Broader image-history atomicity remains
an open gate; ordinary invalid/no-op/stale-before-push behavior is unchanged.

## Bounded work

The descriptor allows at most 512 specs/choice items, 4 KiB per metadata string
and 64 KiB of accounted copied metadata per route. A dialog additionally retains
at most 64 KiB across all four descriptors. A temporary descriptor used for
admission/revalidation is separately bounded to 64 KiB. Retained upstream GObjects
and allocator overhead are not a total-RSS claim. Registry scans stop at 4096
nodes and recognize only the fixed provider identities.

The complete displayed saved-definition preview is at most 65,536 valid UTF-8
bytes, including its omission notice. It bounds the procedure-name copy, quotes
each string within 4096 display bytes, displays at most 256 array/STRV elements,
visits at most 512 argument/reference entries at depth at most 32, and shows at
most 4096 raw bytes. Unknown values are formatted through immutable borrows and
explicit supported-type formatters, never through an unbounded
`g_strdup_value_contents()` or a deep materialization followed by truncation.
UTF-8 repair is charged while appending, so invalid input cannot expand past the
output budget. Numeric entries also limit input text to 128 characters.

The core snapshot patch bounds 512 top-level slots/patches and 1 MiB of
additional changed-value storage. Untouched shared tails are not charged as
hypothetical deep copies: an ordinary one-field edit of Convolution with a huge
ignored tail remains available and retains the identical immutable tail. Only
older non-isolated editor paths that actually materialize full values use the
conservative 1 MiB whole-copy assessment. These are editing admission bounds;
the existing importer/XCF capacities and full stored values are unchanged.

## Acceptance boundary

The acceptance report and deterministic evidence archive accompany this task.
Native GTK tests exercise all four editors, exact no-op/Cancel and one-field
Undo, stale and replaced providers, in-place metadata drift, reordered keys,
bounded previews, large ignored tails, saved special values and imported live/
expired reference provenance. Existing Save/Open and snapshot/Undo regression
cases are rerun because the immutable model patch touches their shared accessors.
Phase D's comprehensive persistence matrix is recorded separately in
[the ordinary XCF integration contract](filter-parameter-persistence.md).

Algorithms, pixel precision, process/wire formats, scheduler, progress,
checkpoint/cancel behavior and XCF format are unchanged. The existing pixel
corpus is not regenerated from this implementation. The GLib minimum remains
2.70 and no GIMP 3.2 API is introduced. Native Linux results do not establish
Windows/macOS, tablet or full-port completion. The independent GTK AT-SPI
`34.003/gtk-atk-menu-guards` dependency defect remains open.

Phase E now records [bounded integrated Linux acceptance](filter-parameter-acceptance.md),
including scoped descriptor/definition allocation failures. This reconciles the
earlier prospective whole-application OOM wording: whole-app/dependency OOM
recovery and global latency/RSS remain outside the accepted component scope.
