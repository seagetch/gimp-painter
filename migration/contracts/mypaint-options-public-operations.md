# Original Options public-operation audit

Legacy pin: afa43fae3e920210146abed514f136fd49f671b5. Hashes and exact
operation findings: migration/tests/options-mode-entry/public-operations-audit.json.
Parent08.018 remains open; this audit is not a declaration of full API parity.

## Constant rendering-mode helper

Original signature: `GimpMypaintBrushMode gimp_mypaint_options_get_brush_mode
(GimpMypaintOptions *options)`. The pinned implementation:347–354 validates the
instance and always returns GIMP_MYPAINT_NORMAL, irrespective of stored brush-mode.
The sole native model already stores the original enum property0–3. Restoration
must not return that property, consult Resource, or change rendering behavior.
Original enum source values NORMAL0, NORMAL_AND_ERASE1, LOCK_ALPHA2, COLORIZE3
are declared in the compatibility options header. No second GType is registered;
the existing native property remains authoritative for serialized storage. The
original libgimpbase enum header route and generated enum-get-type C symbol are
not restored by this operation-level declaration.

Typed get_brush_mode delegates to the original C entry. Valid disposed instances
still return NORMAL (no slot/draft access); NULL or wrong-type instances retain
the original GLib critical diagnostic and default. Arbitrary dangling pointers
are not a valid GObject API input. No extra active-state gate changes this pure
historical helper.

## Explicitly unmatched operations

- **get_current_brush:** returns borrowed mutable `options->brush`, a duplicate
  of the selected preset, initially NULL. The native sole draft is an immutable
  Resource value with separate selected-source ownership. Returning the selected
  source, a new snapshot object, or a cached second model would misrepresent
  identity, direct mutation, dirty state and lifetime. No alias implemented.
- **copy_brush_props:** legacy implementation gets brush-size/brush-angle/
  brush-aspect-ratio into uninitialized doubles then sets these names on dest.
  Its GimpToolOptions base and context do not install those properties; options
  class registers only generated brush settings plus mode/view type/view size.
  No pinned application caller exists beyond declaration/definition. Copying
  current inherited native GimpPaintOptions properties would invent behavior,
  while reproducing undefined values is not a compatibility contract. Kept open.
- **is_ready:** old brush pointer startsNULL, first accepted valid brush makes
  TRUE; a NULL selection callback leaves it sticky; dispose does not clear it
  until finalizer. Native implicit seed, explicit draft clear and closed-binding
  policies differ. Neither active-state nor selected-source alias is equivalent.
- **mapping get/set:** existing original-signature implementations retain float
  rounding and owned getter arrays; setter's wider native notification behavior
  is already explicit PARTIAL. Absent-old-draft versus native seed is not erased.
- **want_null_motion:** is a Brush-class virtual, not Options. Original entry
  guards brush and both coordinate pointers then dispatches; defaultTRUE does
  not remove the subclass contract. Held brush-provider work remains untouched.

These obligations are retained in existing entry-map rows; no task is closed
by interpreting selected-resource identity as current mutable brush equivalence.
