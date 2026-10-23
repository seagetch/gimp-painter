# Original 13.011: group-level strength and cross-group history

72 isolated-group contexts cover nine byte modes, group opacity.5/1 and group
mask absent/0/128/255, over genuine-old child projection fixtures already using
child opacity.5/mask128. Expected group RGB is the old captured child RGB;
alpha uses unchanged old initial_inten_a_pixels INT_MULT/INT_MULT3 order, sourced
from afa43fae3e paint-funcs-generic.h. This proves a source-backed composition of
old captures and old initial-region rules, not a newly captured old group scene.
Mask0 also checks hidden RGB; opacity0 group behavior is not claimed here.

Nine contexts reparent the source from an isolated group to a single-child
pass-through sibling, disable its mask and perform three two-action Undo/Redo
cycles (108 Undo/Redo actions). Parent identity and mask state are checked.
Both masked and unmasked projections compare all64 pixels with genuine-old
captures; the two references must differ, preventing a stale unchanged picture
from satisfying the complete sequence. These are ordinary layers, not Clone
reparenting or arbitrary user artwork.

All11 native suites and focused six-C-unit ASan/UBSan pass. Other GIMP/XCF/C++/
GEGL dependencies are uninstrumented; LSan off. The float source-only extent
regression was separately captured and excluded from this group checkpoint;
its pending float-only fix is not in this tested source snapshot. No production
group change was needed. Original criterion/dependencies stay unchanged and
13.011 remains DOING; broader pass-through group-strength combinations remain
separate. No UI/user-event/screenshot claim is made.
