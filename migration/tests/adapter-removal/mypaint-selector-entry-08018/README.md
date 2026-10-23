# MyPaint quick selector: original entry 35

This bounded port restores the selected-brush thumbnail button in the ordinary MyPaint tool options and compact Canvas toolbar. Its Preset tab selects factory resources; its Custom tab restores application draft history. The existing full editor and canonical options model remain the same instances.

## Compatibility

The C declaration is provided by the native widget header `app/widgets/gimpmypaintbrushoptions-gui.h`; callers migrating from the historical tools-directory header must update their include route. The original C spelling and signature, `GtkWidget *gimp_mypaint_brush_button_with_popup (GObject *config)`, returns a floating native button. The owning `ContextRef::create_mypaint_brush_button` method delegates to that provider and sinks the result. The operation is GUI-thread-only and rejects invalid or unavailable model contexts rather than constructing an unrelated options model. Retained native storage is not a promise that disposed objects are operational.

Row 35 remains **PARTIAL**. Historical Custom selection selected a distinct retained history brush object and loaded a clean duplicate. Native history retains source identity plus an immutable value snapshot, draft name and settings-dirty state; Custom restores that snapshot into the canonical editable draft. The closed thumbnail represents the selected resource. Custom row icons represent the saved snapshots. They intentionally need not be identical. A deliberate repeated Custom restore follows the existing native history behavior and may record another dirty snapshot. Opening or refreshing a popup does not restore a history entry.

Selections apply immediately. Closing the popup is not rollback. History names are read-only: the legacy editable-looking cell had an empty rename callback. Existing editor naming and saving controls remain available. There is no synthetic historical brush object, borrowed live-draft pointer, second resource store, or binary/layout ABI promise.

## Ownership

The button owns its supplied context and canonical options references; callbacks and popup lifetime are bounded by its terminal close. Preset resources use native context/container ownership. History indices identify append-only entries independently of duplicate names.

The history preview query returns a newly owned pixbuf for the stored snapshot. It copies encoded preview data during the typed model borrow and decodes outside that borrow. Missing preview returns NULL without an error; malformed data and invalid/unavailable entries return a contained error. Callers release successful pixbuf results with `g_object_unref`. The result does not alias a mutable source resource icon or any borrowed draft.

## Validation scope

Final validation counts, source/binary hashes, process exits, selected regression cases, independent review and visual capture provenance are recorded in report.json. Evidence covers the actual registered tool-options route, shared-editor identity, selector lifecycle and reentry, context synchronization, snapshot previews, and real stroke/Undo/cancel behavior. Native and ASan/UBSan focused runs are distinguished from the selected existing editor/compact regressions. LeakSanitizer and full-application/full-suite coverage are not implied.

Three isolated regression-fixture failures were reproduced with preserved pre-selector native binaries before correction: the search/history case lacked its corpus setup, and two compact cases lacked deferred device initialization before GUI teardown. Their setup is now self-contained; every existing assertion remains in place. Final selected regressions run separately in fresh processes.

The remaining 75 entry dispositions, separate type/handle catalog and 31 hooks, broader 08.018 work and overall progress are unchanged. Perspective work and ImageGenerator retirement are outside this slice.
