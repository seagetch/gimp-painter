# Painter MyPaint native dockable integration (08.018)

Base: 98d5ea9708658e4319da28ba7eb828bd49987156.
Legacy source: afa43fae3e920210146abed514f136fd49f671b5.

The bounded correction restores the existing native Painter MyPaint Brush Editor
dialog action. The dialog table and corresponding action had NULL help IDs, while
gimp_dockable_new requires a non-NULL ID. The inner shared editor constructor was
already functional through the tool-options Popup Editor route. Earlier /07
registration-presence and /15 direct-constructor checks did not create the real
GimpDockable wrapper and therefore did not establish this integrated route.

Both registrations now use the existing GIMP_HELP_MYPAINT_BRUSH_EDIT identifier.
No new widget implementation, dialog-factory behavior, help-ID constant, or
resource/model behavior is introduced. The action's metadata and factory wrapper
receive the same ID. This does not establish that external help documentation
covers every extended Painter feature, and no F1 network/help-browser acceptance
is claimed.

The exact legacy constructor row 38, gimp_mypaint_brush_editor_new, gains bounded
native integration evidence. Supporting original button/GUI rows 35,53,54 retain
the previously accepted shared-widget workflow. None of these complete an
original exported ABI/named-handle migration obligation. Original row 36's
name-preserving duplicate and overwrite compatibility, row 50's borrowed-draft
API disposition, and row 58's foreground/background colorpicker semantics stay
open. The blocked perspective guard stays excluded. The separate original
76-entry and 31-core-hook inventories are not merged or marked complete; 08.018
is open and overall migration count remains 141/661.

Layer Presets has a neighboring NULL-help registration and remains an independent
known defect. It is not changed by this MyPaint subset.
