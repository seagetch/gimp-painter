# Native operation-granular buffer Undo (09.011)

Original criterion: **一操作単位で復元できる**, dependency09.010. Native
GimpPaintCore already records each committed difference in the ordinary image
Undo stack. The added actual application fixture covers byte/float/double and
direct/floating paths: two distinct operations, exact full-buffer Undo/Redo,
redo-branch replacement, outside-only operations without an entry, and explicit
`push_undo=false`. It checks stack depth as well as bytes and released preview
freeze/session ownership. Existing09.010 additionally verifies tile-aligned
native saved extents against conservative damage union; this is not a claim
that each operation copies the complete drawable.

The complete original429-line DrawableFeature duty remains TODO, with exact
original source identity and every inherited obligation retained in the JSON
ledger. Pixel-history proof does not close09.011/mypaint-undo-stroke: legacy
engine/stroke-state ownership remains an independent explicit requirement.
Commit/cancel, displayed projection, sampling/nonincremental behavior and all
remaining obsolete buffer consumers remain with their traceable owners.

All21 native groups and21 focused sanitizer groups pass. Two actual native
mutants (drop requested Undo / force Undo when disabled) fail at the intended
stack-depth assertions; the restored full suite passes. Eleven units are
instrumented with ASan/UBSan/float-cast, excluding vptr and LSan; dependencies
and the rest of the application are not instrumented.
No GUI or physical pen interaction is claimed by this model-level fixture.
