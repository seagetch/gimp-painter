# Checked legacy Blinds preparation entry

This is a bounded replacement for the procedure-preparation part of original
C-entry-map row 26 and task `08.018`. It does not restore the historical void
`gimp_filter_layer_set_procedure` ABI or complete that row for arbitrary
procedures. The original parent and separate core-extension hooks remain open.
Native validation is pending for this implementation checkpoint.

## Why this is a separate operation

The pinned old source `afa43fae3e920210146abed514f136fd49f671b5` constructs a
PDB runner, initializes its registered argument types to zero, transforms supplied
values, and skips image/drawable/item-ID input slots. It clears the old runner
before lookup. An unknown name therefore leaves no runner. See
`app/core/gimpfilterlayer.cpp:630–660,1198–1203`,
`app/pdb/pdb-cxx-utils.hpp:63–98,165–184,255–279`, and
`app/pdb/gimpprocedure.c:424–441` at that pinned source.

The native saved-definition setter deliberately preserves unsupported names,
absent/empty argument models and raw/opaque serialization. It does not perform
this preparation. Aliasing the old name to that setter would silently change
lookup, initial values, conversion and failure behavior. The new checked
`gimp_filter_layer_set_legacy_procedure` makes its narrower contract explicit.
Both named `FilterLayerRef` methods delegate to this sole C entry and the
existing `FilterSlot`; no second runner, argument model or store is introduced.

## Admitted request

- The only name is the existing admitted literal `plug-in-blinds`.
- The owning application's already restored trusted provider must be available
  and match `FilterEditorSchema`. This operation does not query, restore, launch
  or resolve an arbitrary plug-in, dispatch a main loop or probe an executable.
- NULL arguments create seven present, independently owned `G_TYPE_INT` values,
  all zero. Current pspec defaults (angle 30 and segments 3), historical ID
  pspec defaults and inactive execution defaults are not used.
- A supplied array is a valid `GArray<GValue>` of exactly seven elements.
  Empty, short, long and wrong-element-size arrays are rejected.
- Ordered roles are run-mode, image-ID placeholder, drawable-ID placeholder,
  angle, segments, direction and transparency. Slots 1 and 2 are ignored and
  remain zero, without copying, resolving or retaining their payloads. Slots
  0 and 3 through 6 are consumed. The existing executor supplies live context;
  it never writes runtime IDs back into this saved model.
- Consumed types are exact native int, Boolean, float and double, plus the
  exact run-mode enum in slot 0. Floating values must be finite and safely
  representable as int before transformation. Failed transforms and other
  types are rejected. This is not the old unchecked generic transformation set.

The retained inventory's type roles `0,13,16,0,0,0,0` are not current GType IDs.
Old `GimpInt32`, `GimpImageID` and `GimpDrawableID` subtypes are mapped to the
native integer representation already used by migrated legacy arguments.
Historical GType identity and source/binary compatibility are not claimed.

Preparation does not clamp executable domains. In particular, zero segments
remain a valid saved preparation result but cannot render through the existing
Blinds admission. A successful call proves installation, not successful filter
execution. Noncanonical direction/transparency integers retain the existing
execution rules. Existing saved-model APIs and callers keep their wider,
lossless contracts.

## Ownership, publication and failure

The C boundary checks a live owner and active owner-thread binding before
argument/provider work. Layer, image and application leases cover the operation.
The name and all consumed scalar sources are staged before transformations,
because even a built-in scalar type pair can have a registered transform
callback. No caller element address is retained or reread during conversion.
The caller may mutate, unset and release its array after return; returned query
arrays have independent value ownership. Concurrent mutation or invalid storage
during the synchronous call is outside the caller contract.

Binding generation, definition revision/model, original owner context and
provider identity are checked through the existing final publication point.
Transform-time replacement or close cannot be overwritten by the outer call.
The implementation uses the existing no-Undo definition installation,
notifications and scheduler. A successful explicit replacement retires old
raw/opaque definition bytes; it does not synthesize historical serialized bytes.

Unsupported names, unavailable/incompatible providers and other pre-publication
rejections return FALSE with a contained GError without changes by the outer
attempt. If a preparation callback has installed a newer definition or closed
the owner, that callback's changes are preserved rather than rolled back.
The existing post-publication notification contract remains: synchronous close
or replacement can make the call return FALSE after publication. It must never
roll back a newer definition, and FALSE is not a universal no-op guarantee.
No C++ exception crosses the C entry. Normal GLib requirements for live pointers
and an initially clear GError apply; fatal GLib OOM is not made recoverable.

## Acceptance scope

The mixed C11/C++14 `filter-entry-contracts` registration retains every existing
case and adds finite preparation, ownership, provider, conversion and reentry
controls. Positive provider cases use the real restored bundled registration;
synthetic registry negatives never execute fabricated providers. Transform
overrides run in isolated subprocesses. The named handle route is counted with
the existing link-wrap mechanism.

Validation uses fresh detached source worktrees and ordinary Meson normal and
ASan/UBSan target configurations, actual linked instrumentation/RTTI inspection,
complete selected registrations, isolated profiles and authenticated portrait
TCP Xvfb. No individually substituted objects or edited Meson backend is allowed.
The affected registration list, source seals, original failures and actual
outcomes belong in the final receipt. This bounded component acceptance is not
a fresh whole-registration aggregate, old-runtime pass, leak check or full-port
completion. The earlier exact-47202852 Clone/navigation acceptance remains
immutable and source-bound; a later production change does not pass its
current-root source-equivalence check by assertion.
