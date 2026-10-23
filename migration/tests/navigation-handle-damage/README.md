# Handle invalidation geometry — bounded25.009 checkpoint

Original25.009 and all196 original work duties remain open. This checkpoint fixes
handle-local invalidation bounds, not draw or hit policy. Corner safety and other
primitive audits remain independent work, excluded from the tested binary.

Two independent failures were retained:
* A35x27 circle draws radius17.5 on both axes, but old bounds used height27.
  Actual GDK-window hide retained129 pixels after full exposure.
* Old unchanged square bounds fail at local angle0.63: recorded ink begins y186,
  invalidation y187. The old radius formula also ignored the anchor-relative
  rotation pivot, unequal dimensions, and DROP's width-based circular section.

The new bounds conservatively enclose the unchanged draw path: square rectangle,
width-based circle, diamond/cross rectangle, width-based drop. Rotated primitives
use the same snapped original anchor pivot as drawing, not a shifted center.
The full rectangle is projected through the local angle, expanded for three-pixel
stroke/antialiasing, rounded outward, then passed to existing common display
matrix invalidation. Draw and hit functions remain byte-identical to167f2cb636.

Bounds-only internal-view admission rejects nonfinite/unrepresentable values
before shared zoom_xy_f, whose PROJ_ROUND converts to gint despite its _f name.
The initial post-transform guard failure is preserved. Six invalid scale probes
plus restored valid extents exercise this guard; this does not claim malformed
views are safe to draw through every unrelated canvas API.

Native proof per normal/sanitizer run:
*156 raster scenes:13 types x3 display angles x H/V flips; independent pinned old
 viewport CTM, current primitive, every alpha>16 pixel inside projected extents.
*8424 recording-surface scenes:13 types x9anchors x2aspects x3local angles x
 3display angles x H/V flips. This is conservative ink-bound coverage, not an
 old-runtime raster comparison.
*156 actual GDK server-window show/hide scenes,13 types x4flips x3angle/local-angle
 pairs. Only initial show gets a full exposure; hide uses real item invalidation.
 A470x470 ROI must return with zero pixels differing by more than2 channel levels.
*18 registered cases: geometry1, bounds13, admission1, actual-window3. Each scoped
 Meson invocation retains the original60s budget, with ordinary ASan+UBSan,
 O2/assertions on/debug symbols off, external dependencies uninstrumented, LSan off.

Authenticated1080x1920 Xvfb; oncanvas enabled. Actual37degree root-window images
show partial circle before/after hide. Earlier grid4+4 proof is replayed at its
immutable checkpoint, not relabeled as a fresh aggregate on this source.

Old unmirrored partial-circle angle subtraction is not missing: old Cairo helper
negates start_angle, so start_angle−rotation becomes −start_angle+rotation, matching
the native common CTM. No double-correction was made. Old runtime is USER-SKIPPED;
no whole-parent, old pixel equality, platform, physical-device or whole-branch
build claim is made.
