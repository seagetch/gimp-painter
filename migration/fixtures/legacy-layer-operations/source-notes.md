# Source-grounded acceptance and prior evidence

Repository guidance: top-level README and tasks.md; migration/progress.tsv;
migration/contracts/clone-layer-port.md and filter-layer-port.md. No AGENTS.md
was present in the workspace/repository ancestry. The isolated source HEAD is
`afa43fae3e920210146abed514f136fd49f671b5`.

Existing legacy-runtime/clone-capture.log already observed a same-size source
move, one source-delete Undo and a post-Undo pixel. Existing legacy-clone-undo
covered size-changing resize plus repeated Undo/Redo and mask/history defects.
Existing legacy-composite covered original presets, source recolor and opacity
Undo/Redo. These are preserved, not replaced or inflated into complete layer
operation coverage.

This package adds full operation/reference and pixel histories for movement,
stack reorder, direct versus group duplicate, delete/Undo/Redo of source/clone/
group and FilterLayer lower-topology effects. It measures source identity and
notifications at each step instead of treating equal names as equal objects.

Relevant pinned source behavior:
- app/core/gimpclonelayer.cpp: set_source/get_source retain borrowed source
  identity and connect update/frozen callbacks; duplicate calls set_source on
  the original source. Inherited translate changes clone position.
- app/core/gimpgrouplayer.c: gimp_group_layer_duplicate_aux constructs the
  original-to-copy table; gimp_group_layer_remap_clone_layers maps sources found
  in that table and retains external ones. The remap occurs before image add.
- app/core/gimpitem.c and gimplayer.c: translate/duplicate route to actual type
  slots; ordinary position/offset state and Undo handling are exercised.
- app/core/gimpimage.c and gimplayerundo.c: image remove with push_undo keeps
  deleted objects in layer Undo ownership; pop swaps insertion/removal.
- app/core/gimpfilterlayer.cpp: duplicate copies procedure/arguments; translate
  invokes inherited position handling then filter_reset/invalidate_layer.
  The original runner receives image/drawable IDs for the current filter and
  consumes its current lower stack. Completion instrumentation only increments
  observation counters after the original end handling.

No inferred pointer lifetime after Undo history eviction is claimed. Observation
pointers are non-owning, and we deliberately do not clear histories while testing
removed-object survival. The layer source API is read only for already resolved
clones in this scene; unresolved-name resolution is not under test.
