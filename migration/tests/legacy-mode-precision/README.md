# Precision storage and source-extent checkpoint

`/painter-layer-modes/precision_and_offset_projection` in
`app/tests/test-painter-layer-modes.c` runs all seven custom modes through real
GEGL graphs with six nonlinear RGBA buffer storage formats: u8/u16/u32,
half/float/double. Each of 42 cases compares 64 pixels to the genuine old
application opacity50/mask128 capture and 118 pixels outside the source extent
to the unchanged backdrop. The source is offset relative to the requested ROI;
the backdrop extends to negative coordinates. The embedded genuine fixture has
transparent and partial-alpha source pixels, so rectangle exclusion is distinct
from zero alpha inside the source rectangle.

This verifies **byte-compatible results with higher-precision storage**, not a
new high-precision blending algorithm or group/artwork/UI acceptance. All prior
72 fixture cases and 216 direct paint cases run in the same native test binary.
The ordinary native report omits the inherited environment deliberately.
Xvfb is authenticated 1080×1920, but this graph/core test has no UI or oncanvas
claim and supplies no screenshot. Original 13.012 and 13.013 dependencies remain
open. The focused sanitizer instruments only the Painter operation, cached
layer-mode dispatch and test adapter, not all GIMP dependencies; LSan is disabled.
