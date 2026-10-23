# Checked Small Tiles preparation entry

This is an additive bounded extension of the existing checked Blinds preparation
operation, not a restoration of the historical void setter or arbitrary PDB
execution. `gimp_filter_layer_set_legacy_procedure` and its two named
`FilterLayerRef` methods remain the sole entry and owner-state path.

The exact admitted additional name is `plug-in-small-tiles`. Its original
registered roles are `0,13,16,0` (run-mode, image ID, drawable ID, tile factor).
Those inventory tags are historical argument roles, not current GType IDs.
The original PDB runner initializes registered values to typed zero, skips the
image/drawable context slots and transforms the remaining supplied values.
The reviewed native representation is four `G_TYPE_INT` values. NULL arguments
create four typed zeros; supplied arrays must contain exactly four GValues.
Only slots0 and3 are staged and transformed, while slots1 and2 remain zero
without retaining, copying or resolving caller payloads. Native execution uses
its existing live context and never stores runtime IDs in the prepared model.

The exact scalar conversion whitelist, finite/int-range checks, complete staging
before callback-capable transformations, provider metadata/capture validation,
owner-thread admission, retained owner/context lifetimes and final generation/
definition/provider publication guard are shared with Blinds. A compile-time
restriction admits only the reviewed four- and seven-value layouts; this is not
an arbitrary procedure binder. Scalar zero is not replaced with a modern pspec
default, and preparation does not clamp rendering domains. Unsupported names,
invalid shapes/types and pre-publication provider/owner failure preserve the
outer call's prior definition. Reentrant newer definitions or closure survive;
FALSE is not a universal no-op guarantee after synchronous notification.

## Explicit limits

* The historical `void gimp_filter_layer_set_procedure` spelling/ABI remains
  absent. The checked operation returns success/error, preserves unknown saved
  definitions on rejection and rejects unsafe transforms. These are intentional
  native differences; the canonical original entry remains PARTIAL.
* Other procedures, general PDB admission, arbitrary object/string conversion,
  historical subtype GType identity and binary layout are not restored here.
* Existing raw/saved-model setters retain their broader lossless contracts.
* No new GUI caller or user interaction is added. Existing execution pixels are
  compared through the new C preparation path against the immutable genuine-old
  25-scene/50-output Small Tiles live fixtures, not against regenerated expected
  pixels or a substituted GEGL operation.
* The prior Blinds acceptance archive and its source hashes remain immutable.
