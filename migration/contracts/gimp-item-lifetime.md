# Retained GimpItem lifetime repair

Date: 2026-10-02. This is the narrow common-core prerequisite for owning C++
CloneLayer/FilterLayer handles; it does not change their single BindingStore
ownership scheme or add a strong item-to-image/Gimp cycle.

## Failure and repair

Previously `GimpItemPrivate::image` was borrowed indefinitely. A layer retained
past image destruction still returned that freed address, and its finalizer
read `image->gimp->item_table`. The item now registers a weak image pointer and
an independent weak Gimp identity for its ID-table entry. Image destruction
clears the image getter. Finalization unregisters surviving weak hooks and
removes the ID only if the owning Gimp/table remains available and the entry
still identifies this item. No weak-reference promotion occurs during Gimp's
refcount-zero teardown. These ordinary core links remain main-thread confined.

`set_image` unregisters the old image hook and registers the new one; same-image
calls are inert and same-Gimp moves preserve IDs. The defensive different-Gimp
branch removes the old mapping and allocates in the destination table. The
application's singleton unit registry disallows simultaneous Gimp contexts, so
that branch is reviewed rather than claimed as an executed supported workflow.

`replace_item` transfers identity only after selecting the destination image,
and unregisters both weak hooks on the retired item. It zeroes that retired
item's ID. This is necessary because the old direct `image = NULL` would leave
a weak callback targeting freed instance storage. Finalization also checks the
mapping identity so it cannot erase an entry transferred to another item.

## Executed regression coverage

The full-GIMP Clone test executable now has 37 cases, including:

- A genuine retained ordinary source, serialized source snapshot, typed C++
  CloneLayer handle and owning source handle survive destruction of their image.
  Image getters become NULL, Clone binding is closed, source ID remains until
  the last source reference is released, then disappears from the live table
- Same-image assignment and relocation between two live images under the same
  Gimp preserve ID; destroying the old image cannot clear the new image link
- Replacement in the same/different images transfers ID, image, and geometry;
  destroying the retired item cannot erase the replacement ID or leave a weak
  callback into freed storage
- A subprocess retains source and Clone past image destruction, runs ordinary
  Gimp shutdown/finalization, observes Gimp actually finalize, and only then
  releases both items. No owner cycle, dead-table access or resurrection occurs

The subprocess explicitly expects one existing upstream headless-test warning:
`gimp_font_factory_finalize()` unconditionally calls `g_object_unref()` on its
NULL `pango_context` when `gimp_init_for_testing()` selects no fonts. A diagnostic
backtrace identified `app/text/gimpfontfactory.c:220` through
`gimp_data_factories_exit`; this unrelated code was not modified. The test does
not suppress other warnings or sanitizer failures. The existing writable-data
path warning may also appear during test shutdown.

The Filter worker separately executes retained-handle queued and running-job
cases with `gimpitem.c` instrumented, verifying image/source destruction,
closed binding, no late publication, readable cached pixels and clean release.
See its separate Filter evidence for the measured result.

## Reproduction and scope

Use the build environment and shared build lock described in
`clone-layer-port.md`. `run_clone_layer_sanitizers.py` now explicitly instruments
`app/core/gimpitem.c` in addition to CloneLayer, group duplication, adapters and
BindingStore. Other upstream/dependency objects remain uninstrumented and leak
detection is disabled. Normal and focused sanitizer evidence is saved as
`migration/tests/clone-layer-testlog.{txt,json}` and
`migration/tests/clone-layer-sanitizers.json` with an environment allowlist.

The adjacent immutable XCF provenance-copy block in `gimp_item_real_duplicate`
is a separately owned XCF change. It is not part of the weak lifetime mechanism.
