# Original WBS 08.004: CloneLayerUndo and native parent behavior

CloneLayerUndo already derives directly from native GimpItemUndo with a common
ReferenceUndoSlot. Its constructed chains the parent before capturing the source
snapshot; pop chains the parent before replay; free closes the slot before native
item release; dispose closes before parent disposal; memsize includes inherited
accounting. The source-reference operation remains distinct from the dormant old
GROUP_LAYER_CONVERT helper. In the pinned old Clone implementation that static
push helper has no caller and the convert override binding is commented out.
The existing contract retires it. Actual precision conversion uses inherited
native drawable Undo, whose conversion/Undo/Redo regression runs here unchanged.

Three public type macros present in the exact legacy header had been omitted:
CLASS, IS_CLASS and GET_CLASS. They are restored with ordinary native GType macros.
The C fixture and a separately compiled C++ probe use all three and compare the
registered instance/class sizes with their public structs. The native direct
parent is checked as GimpItemUndo. No alternate C++ registration system is added.

The focused test creates a Clone without adding it to an image layer tree. A
native Undo receives image/item/undo-type properties, retains that item and
replays two pending-source snapshots through public gimp_undo_pop in both
directions. When the caller releases its Clone reference, only the inherited item
ownership keeps it alive. gimp_undo_free must clear item, destroy the unowned
Clone and remove its item ID. Both UNDO and REDO free modes are tested. A third
case deliberately supplies the wrong Undo kind and checks inert construction
followed by the same cleanup. It does not claim inert pop behavior was tested.

Repeated free and dispose remain safe. Native parent memory accounting is checked
both before and after free. A distinct, never-replaced GObject qdata notifier
counts final object/qdata destruction exactly once after the last unref; it is
not a directly instrumented ReferenceUndoImpl destructor. Native GimpItemUndo's
explicit free-before-unref contract is followed. GimpUndo is born owned, unlike
GimpItem's forced floating state, so the fixture does not ref_sink the Undo.

All 39 native Clone cases pass with 21 current source seals, including the Undo
header, GimpItemUndo, GimpUndo, GimpObject and both C/C++ test units. The preceding
38-case report stays unchanged as its historical snapshot. The first fixture
incorrectly added an extra Undo reference; that was corrected in the test and is
not evidence of a production leak. No runtime parent-chain defect was found.

Exactly two assigned legacy source duties close. Whole-file anchors are scoped
to this original type/parent acceptance, not every Clone behavior. Existing
source-state, reference ownership, geometry and precision regressions accompany
the focused test; broader feature, sanitizer and platform gates remain separate.
