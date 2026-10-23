# Original 13.011 group-order regression

## Causal failure and narrow correction

The original native single-child pass-through strength reduction lifts the
child's effective mode onto the group after projecting the child alone. That
bakes the child's opacity and mask into source alpha before the legacy blend.
Byte arithmetic is not associative: Painter Replace with backdrop alpha1,
source alpha0, opacity50 and mask128 retains alpha1 in the genuine old reference,
but the strength-reduced graph yielded alpha0. `before-fix.json` captures this
expected-red native regression. `reduction-disabled.json` runs the unchanged
code with the existing diagnostic `GIMP_NO_PASS_THROUGH_STRENGTH_REDUCTION=1`:
all 14 original seven-mode group cases then pass, isolating the optimization.

The correction in `gimpgrouplayer.c` rejects strength reduction only when the
first active child's effective mode is an explicit Painter byte-compatibility
mode. It does **not** disable the optimization for ordinary modern modes. The
existing multi-child test already rejects non-Normal modes; the new guard
addresses the previously unrestricted single-child path. All nine byte modes
(including Painter Normal/Multiply) need the same arithmetic-order protection.

## Native matrix and scope

`/painter-layer-modes/group_projection_routes` exercises nine byte modes in
both native group connection arrangements, at one, two and three group levels:

- isolated `NORMAL_LEGACY` groups containing backdrop and source children;
- pass-through groups containing the source, above an external backdrop sibling.

The source child has opacity50 and mask128. Each of 54 projected 8×8 RGBA scenes
is compared byte-for-byte with the existing genuine old **flat child-composite**
reference. Four modern-mode controls (Normal, Multiply, Erase, Screen) assert
that native single-child strength reduction remains enabled.

This checks native graph connection/order against independent arithmetic
references, not historical captures of grouped artwork. It does not cover
arbitrary nested group masks/opacity, reorder/Undo, or modern-linear isolated
group choices, and does not close 13.011's original predecessor dependencies or
section13 artwork acceptance. Existing 72 projection/fixture, 216 direct paint,
and 42 storage/extent graph cases remain in the same executable.

Evidence JSON omits inherited process environment. Xvfb is portrait but this is
a numerical core test without a UI/screenshot claim. The focused sanitizer
instruments GimpGroupLayer, the byte operation, cached mode dispatch and C test;
remaining GIMP/GEGL dependencies are not instrumented and LSan is disabled.

The first modern control attempted standard Replace, which is filter-only in
GIMP3 and rejected by the layer API. The control was corrected to standard
Screen; this was a test setup error, not a production regression.
