# Original 13.011 group reorder / Undo checkpoint

18 native contexts cover all nine Painter byte modes inside isolated
Painter-Normal and pass-through groups. Each begins with the genuine old
opacity50/mask128 projection. The source child is reordered below the backdrop
through `gimp_image_reorder_item(..., push_undo=TRUE)`. The new projection must
match an independently constructed fresh **flat native** image with the new
order, and must differ from the initial reference. Three ordinary Undo/Redo
cycles then compare all64 RGBA pixels to the old initial or fresh reordered
reference each time (108 Undo/Redo actions plus18 reorders).

This exercises real item history and projection invalidation, not direct undo
callback calls or cached-byte restoration. Only the initial reference is a
historical capture: the reordered reference is fresh native integration proof,
not a newly observed old-app result. Cross-group reparenting, arbitrary group
mask/opacity combinations and artwork acceptance remain separate; no original
criterion or dependency is removed and 13.011 remains DOING.

Native and focused ASan/UBSan pass. Instrumentation covers GimpGroupLayer,
Painter byte operator, cached dispatch and the C test (including this include),
not the entire history/projection engine or XCF/C++ dependencies. LSan is off.
The same binary retains all preceding numerical and XCF regression tests;
this is not a substitute for their separately scoped sanitizer reports.
No UI/screenshot or user-event synthesis claim is made.
