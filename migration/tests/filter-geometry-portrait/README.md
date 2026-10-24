# Actual Filter geometry portrait sequence

One registered GUI test passed through six actual rendered states on authenticated loopback Xvfb1080×1920×24. The runner verified proper authority acceptance and empty-authority rejection. Each state asserts painter-canvas-ui TRUE, actual oncanvas visibility, native bounds, and two independent exact derived source colors. All six root-window PNGs were visually inspected; they are not mockups or image edits.

These are **direct core API calls**, not synthesized gestures/menu actions, and show the same fixed build, not before/after binaries. The ordinary400×300 source stays red-left/blue-right. Native Filter bounds:

| State | Offset | Size |
|---|---|---|
|0 baseline|140,70|160×160|
|1 translate|100,70|160×160|
|2 scale|80,30|240×240|
|3 resize|100,50|200×200|
|4 own-center rotate180|100,50|200×200|
|5 exact matrix180|100,50|200×200|

Every operation advances generation and settles before capture. The selected layer outline changes for translate/scale/resize. Rotate and matrix transformation **correctly preserve the resulting red|blue appearance**, because their temporary transformed cache is replaced by reevaluation of unchanged lower input. Different screenshots do not imply changed source colors. Native image coordinates are asserted independently of viewport/layout settling.

Production Filter provider matches committed9fad7ffbc6, layered over the sealed20b/Options enum integration and earlier Filter checkpoints. Only two owned GUI test files were overlaid; source-before/after and52-file archive record exact inputs, no source drift. This is not an exact whole-HEAD aggregate or additional sanitizer claim. Existing missing application-icon/headless bus warnings are retained. Initial blank thumbnails are visible and not certified by this geometry test; later thumbnails settle. No held provider edits, Clone geometry or default Edge execution.

Original15.005 acceptance and prerequisites remain unchanged/open. See preceding model proofs for no-op scheduling parity and selected sanitizers. Recommended user-facing pair: baseline0 and scale2; resize3/rotate4/matrix5 document correctly identical final derived appearance after those latter transforms.
