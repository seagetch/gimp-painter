# Original WBS 08.002: CloneLayer native GType lifecycle

The existing production skeleton already uses G_DEFINE_TYPE_WITH_CODE directly
under GIMP_TYPE_LAYER and implements GimpPickable. Its public C instance and class
embed the native parent first. The only extra C value is binding-failed; the
implementation lives in a CloneSlot owned by the shared BindingStore.

Instance init creates that slot under the exception boundary. Constructed chains
to the inherited native implementation before initializing and activating the
store and observing the owning image. A failed instance becomes inert. Dispose
closes the store before parent dispose; repeated close is harmless. Native parent
finalization releases drawable/item resources, and the store's qdata destroy
notifier releases the C++ state. No parallel legacy type-registration or private
Impl registry is reintroduced. There was no reproduced production defect for
this task; the missing evidence was an explicit native lifecycle assertion.

The new C test checks the direct parent, actual registered instance/class sizes,
Pickable interface, exact factory type, inherited name/dimensions/opacity, native
GEGL buffer and empty source state. Ordinary final unref clears the weak pointer
and removes the native item ID. A separately declared native C descendant chains
its finalize override to the real parent and counts finalization. It verifies
both ordinary last-unref and two explicit g_object_run_dispose calls followed by
last-unref. Dispose closes admission while finalize remains zero; final unref
increments finalize exactly once and removes the item ID. The fixture supplies
the image required by GimpItem's inherited construction contract. Weak notification
is not mistaken for finalization, because it may fire during explicit disposal.

All 38 current native Clone tests pass with 17 matching source seals. Existing
inert construction, C/C++ header/layout and typed handle checks, repeated binding
close, retained-image shutdown and source callbacks remain in that run. The
existing 37 tests alone had not asserted the direct parent/type sizes and actual
repeated GObject-dispose/finalize chain. The production implementation is unchanged.

Both assigned legacy source blobs are preserved and mapped in source-mapping.json.
Their whole-file source ranges are anchors for this original type-skeleton action,
not blanket acceptance of all behavior in those files. Original Undo, reference,
pixel, transform and XCF tasks retain their independent obligations. Historical
Clone sanitizer and component snapshots keep their original hashes; this new
38-case run does not relabel them as current sanitizer or platform evidence.
The preceding startup report similarly remains the original 37-case snapshot.
No extra platform, sanitizer or complete migration result is claimed.
