# Painter GEGL Surface and native session increment

Pinned reference: seagetch/gimp-painter
`afa43fae3e920210146abed514f136fd49f671b5`.

This module is registered with Meson and owns a real current GimpPaintCore
transaction through the common typed ObjectRef machinery. It is a C++ session
controller, not yet a registered paint-core/tool/options/editor GObject adapter.
Those future adapters must use the existing single-slot BindingStore foundation.

## Rendering and ownership

- `legacy-pixel.hpp` derives from `app/base/pixel.hpp` under GPL-3.0-or-later.
  Expression arithmetic and intermediate clamps are retained, names isolated.
- `legacy-pixel-modes.hpp` preserves the original GPL-2.0-or-later notice and
  bitmap blend/sample formulas. Iterator pointers are bounded, short-lived views
  into owned vectors. Transparent lock-alpha explicitly produces the hidden black
  observed from the pinned x86 runtime, avoiding its undefined NaN-to-byte cast.
- `gegl-surface.cpp` uses actual GeglBuffer ROI reads/writes, one stroke snapshot,
  a separate floating buffer, the original accumulation/composite order and byte
  RGBA math. Other target formats are explicitly refused, never silently reduced.
  Snapshot validation finishes before publishing an active session.
- Ellipse sampling ignores paper coverage; shaped sampling includes it. Both
  preserve the old paper-wrap partition of floating sample sums. A real old
  comparison exposed and fixed 22 differences in that summation order.
- Bitmap transforms preserve the old height/width argument order, bounding box,
  12-bit reverse interpolation and blur. Bilinear products use equivalent defined
  unsigned arithmetic, and coordinate walking uses wide integers. Bounds guard
  representable indexing/fixed-point values, not an arbitrary 8192-pixel size cap.
- Generated masks preserve separate shape/spike/angle/LUT math, including old
  direct-setter radius/aspect ranges. They are never resampled GIMP3 masks.
- `gimp-resources.cpp` resolves ordinary named brushes/paper without losing missing
  reference strings. Stroke and preview missing-name behavior remain distinct.
  Balanced begin/end-use, owned brush/pattern references, weak dirty callbacks,
  bounded bitmap caches and immutable shared paper snapshots cover resource life.
- `paint-core.cpp` uses real GimpPaintCore start/finish/cancel, the native Undo
  snapshot, selection offset, FG/BG, layer alpha lock, redraw extents, and Undo.
  Stationary pressure/time events reach the evaluator. A logical split commits
  one native transaction; controller destruction rolls back an unfinished one.

## Evidence and limits

The sealed `migration/fixtures/legacy-mypaint-surface/` package contains **actual
old-code executions with synthetic stimuli**, not historical user artwork:
148 exact old Surface records (48 samples, 50 dab returns, 50 complete RGBA
snapshots), 24 bitmap-mask assertions and 72 generated-transform masks. The port
computes masks itself; measured masks are assertions, not injected render inputs.

Nine native GIMP3 cases exercise incremental/nonincremental real transactions,
exact Undo/Redo pixels, cancel/repeated cancel/destruction, preview thaw, stationary
pressure with selection, resource lifetime and missing references, paper dirty
invalidation, rejected snapshot/precision, finite transparent lock-alpha and
representable resource dimensions above the removed arbitrary bounds. They also
reject recursive start/configure, defer start-time cancel/finish, keep images alive
through freeze/thaw/Undo dirty callbacks, and safely unwind drawable removal.

This is **not yet an old full paint-session pixel comparison**. Real legacy
GimpMypaintCore/drawable/selection/Undo session captures are the next gate. Further
gates include large tiled sampling, brush-pipe selection equivalence, RGB/gray and
higher precision targets, registered options/tool/editor, options-change stroke
splits, symmetry and actual GUI/tablet/platform tests. No broader parity follows
from these small byte fixtures. See `migration/contracts/mypaint-surface-session.md`.
