# Original 25.009: shared image/overlay display matrix

Original acceptance: **canvas と overlay が同じ角度を使用**. Final native results are **97 normal + 97 ordinary ASan/UBSan PASS**,
with all 88 scoped inputs bound. `check-parent.py` validates the completed
manifest; every invocation remains below 60 seconds (maximum 36.16 seconds).
This package does
not claim whole-port completion, historical-application execution, physical
input-device validation, or cross-platform parity.

## Source coverage

`audit.json` is the initial unchanged snapshot of all 196 original work rows.
`full-hunk-audit.json` and `pinned-hunks/` retain their complete pinned diff
payloads, including 265 removed lines. `check-hunk-provenance.py` reproduces
those payloads from Git. The 38 renderer/draw/expose/selection, 113 primitive,
and 45 navigation/transform/header source reviews identify native replacements
and limitations. Their historical OPEN/pending labels are preserved. The final
`coverage.json` links each original duty to its source review and native receipts;
it does not reassign or close any other whole-hunk owner.

Native image-space items, rendering and selection use the shared display CTM;
local scale/translation is not a second rotation. Screen-space oncanvas controls
intentionally remain unrotated. Primitive snapping/shape construction is native,
not a claim of pixel-identical historical application rasterization. Dead old
helpers are not recreated. Native GEGL replaces old TileManager ownership. The
old zero-size render helper's quiet return is not claimed equivalent to native
internal positive-size preconditions; actual native callers clip/intersect first.

## Native matrix

The final matrix comprises 47 new registered cases and the unchanged original
50 cases (35 original + 2 old rotation + 10 applied zoom + 3 configured Space).
Each runs in normal and ordinary ASan/UBSan builds, with assertions enabled,
optimization 2, debug symbols disabled, leak detection disabled, and uninstrumented
pinned dependencies. Every Meson invocation retains the original **60-second**
budget; separate process scopes avoid test-state leakage, not budget relaxation.

New coverage in each configuration:

- 792 raster/extent combinations across eight primitive families, 0/37/90 degrees
  and all horizontal/vertical flip combinations.
- 336 actual GDK-window primitive show/hide scenes; 120 anisotropic precise-grid
  hides; 12 selection hides, with no forced final repaint and zero residual pixels.
- 12 checkerboard scenes, 220,900 independently transformed ROI pixels per scene.
- 24 multi-chunk image scenes, each checking more than 20,000 interior pixels;
  isotropic/anisotropic scales and fractional inherited pan.
- 36 image-edge scenes at 0.5/1/4x, checking both interior and outside-image pixels.
- 96 signed-rectangle and 1,152 signed-guide geometry comparisons, byte-exact
  against canonical positive-size forms. These offscreen comparisons are not
  mislabeled as actual UI screenshots.

Actual UI fixtures explicitly enable oncanvas UI and run on authenticated
1080x1920x24 Xvfb. The unchanged historical raster detector settings remain as
originally tested; they are **not** relabeled oncanvas-enabled. Screenshots are
actual rendered X-root captures, not composites. Central bounded ROIs exclude
the overlaid toolbar. The generic rectangle-guide NONE screenshot is not used
as visible-guide evidence.

The old selection padding difference was investigated with real fractional-pan,
rotated/flipped hide tests. No remaining selection trail reproduced, so no
speculative production change is included. Separate committed grid, handle,
corner and path fixes preserve their original failures and independent evidence.
`replay-path-checkpoint.py` replays the immutable a114439826 path checkpoint.

## Honest attempts and input binding

Initial prototype/compile failures and earlier bounded bug investigations are
retained. `checker-compile-red.*` is a compile-only missing-header diagnostic,
not a runtime result. `partial-manifest-attempt/` preserves 47 runtime-passing
normal receipts whose collector omitted additional native fixture includes.
`partial-oracle-manifest-attempt/` preserves 19 receipts whose collector omitted
old pan/rotation oracle includes. Neither is final complete-input acceptance.
Affected groups were rerun; receipts were not retroactively rehashed. Final
receipts bind every registered navigation include and every oracle `.inc` in
all three fixture directories, plus the scoped production/test dependencies.
Negative controls specifically reject either omission, even if all receipt and
artifact hashes are coherently rewritten.

The private build is committed ce40ac4cffa5b6dba48cd9100303c3c94653a5bc plus only
scoped navigation/display fixtures, not a build of the entire evolving checkout.
`held-source-isolation.json` seals all eight held production paths (popup,
oncanvas retirement, MyBrush/viewable/viewrenderer and Bucket): seven equal
committed ce40 baseline bytes and the new Bucket header is absent. No current
main held candidate was imported and no held test was executed. All four
platform holds remain in force; this evidence does not verify those candidates. Receipts retain allowlisted
results/stdout/stderr/source hashes, not raw environment-containing Meson JSON.

Run `python3 migration/tests/navigation-item-geometry/check-parent.py`,
`test-parent-evidence.py`, `check-hunk-provenance.py`, and
`replay-path-checkpoint.py` from the repository root.
