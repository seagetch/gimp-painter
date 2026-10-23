# Original25.009 — common image/overlay display matrix

**Verified original acceptance:** `canvas と overlay が同じ角度を使用`.
The original196 work IDs, actions, criteria and source payloads are retained.
Final source-bound verification passes 97 normal and 97 ordinary ASan/UBSan
cases, each invocation below the unchanged 60-second budget (maximum 36.16 s).
The proposed ledger patch closes only original25.009 and its196 rows after
independent review; no additional test-only child is introduced.

## Architectural replacement

The legacy code manually transformed primitive vertices, individual selection
segments, renderer chunks and Cairo contexts. Native GIMP instead has one
forward/inverse display matrix. Image rendering, checkerboard and image-space
canvas items consume that matrix; item-local functions only scale/translate.
Damage rectangles are projected through the same matrix before invalidation.
Unrotated oncanvas toolbar/control widgets deliberately remain screen-space;
they are not image-space overlays and must not be rotated as a second image.

All196 full legacy diff payloads, including265 removed lines, are pinned under
`migration/tests/navigation-item-geometry/pinned-hunks/`. `audit.json` preserves
the initial exact work-row snapshot and other whole-hunk owners. Three source
reviews map38 renderer/draw/expose/selection +113 primitives +45 navigation/
transform/header duties. They are source-replacement reviews, not fabricated
old-application executions. The original criterion is not pixel-identical
legacy rasterization: native snapping/primitive construction is retained while
common matrix/angle, bounds and actual invalidation are verified.

## Verified native coverage

- Independent old viewport Cairo transform against native image-space primitive
  rendering and damage, all eight primitive families,0/37/90°, H/V flips.
- Actual GDK-window show/hide for arc, guide, path, rectangle and rectangle-guide
  families; prior separately committed grid/handle/corner/path repairs retain
  exact historical failures and source-bound normal/ordinary sanitizer proof.
- Transparent checkerboard alignment, fractional pan, anisotropic multi-chunk
  image interiors, image-edge clipping at0.5/1/4×, and signed rectangle/guide
  representations with multiple guide counts.
- Actual selection hide with fractional pan and flips, without final forced
  repaint; geometry-only signed comparison is not mislabeled an oncanvas GUI run.
- Precise rotated grid clearing with anisotropic scaling and show-all mode.
- Final-source original50 navigation scopes, retaining the original35-case
  budget and separate old rotation/zoom/Space scopes. Their historical detector
  settings are not rewritten or mislabeled oncanvas-enabled. New actual UI
  fixtures explicitly enable oncanvas and use authenticated1080×1920 Xvfb.

## Explicit boundaries

Native GEGL replaces old TileManager ownership/fault paths; dead old helpers
and a declaration-only expose API are not resurrected. The old render helper's
quiet zero-extent return differs from the current internal positive-size
precondition: native canvas callers clip/intersect before rendering. This is
recorded, not claimed universal old API equivalence. Old selection expose
padding is reviewed separately; current actual native hide tests determine
whether a defect exists before changing production code.

Other whole-hunk duties remain with their existing owners (e.g. full-motion
tracking/snapping portions of000672). This criterion must not steal their
completion. No historical app runtime, physical-device, cross-platform,
whole-port/release or arbitrary malformed-geometry acceptance is claimed.

All eight held production paths in the private build match committed ce40
baseline bytes (the new Bucket header is absent), as recorded by the read-only
isolation seal. No held candidate or held test was imported/executed.
