# Perspective display: original26.005 remains open

This checkpoint fixes two proven valid legacy path differences, not an invented
new UI requirement: rounded fractional point centers and missing radial segments.
See `migration/contracts/perspective-display-compatibility.md`. The unchanged
original193 work assignments/criteria/other owners remain open in `audit.json`.

## Evidence design

- Unchanged full old draw function, pinned source/blob/hash, source-only standalone
  path adapter: one/two/three-point paths have1/1/3 radial segments.
- Native actual provider versus the unchanged old function: expected-red at
  integer zoom1 (34 mismatched pixels), separately fractional.25 (242).
- Four zooms1/.25/1.25/4, two image-resolution aspect ratios, angles0/37/90,
  all four flips and1–3 points:288 raster comparisons and288 every-ink containment
  scenes. A shared current outer matrix/native stroke is an explicit adapter
  boundary; old runtime and independent old-transform parity are not claimed.
- Actual oncanvas incremental redraw:24 poses ×7 model transitions =168 checks.
  Fractional pan is applied before mutations. Incremental screenshot is captured
  before the full-redraw reference, so a later repair cannot fix the saved result.
  Masks come from independently rendered old/new guide states, never the faulty
  screenshot or get_extents(). Both removed and added ink plus1px AA fringe count.
- Global full-canvas interior differences are also logged. Every unexplained
  difference above2 channel levels fails. Only independently rendered pre-existing
  image/layer-border ink can admit the separately observed3-level clip-compositing
  difference; guide tolerance stays2. No arbitrary rising tolerance.
- Missing-damage production mutation must fail the actual redraw gate, then source
  is restored. No held-provider diagnostic is performed.

## Preserved diagnostics and exclusions

The first source sync missed the committed canvas-factory caller; its compile
failure is preserved, not a production defect. Byte-identical redraw initially
failed361 pixels (353 at1 channel level and8 at2); a second diagnostic isolated a
single yellow image-border pixel at3 levels. Captured buffers and source archives
are retained. The earlier three-new-case aggregate timed out at60 seconds; it is
not acceptance. Final angle scopes use separate processes, each with the original
60-second budget; the original24 UI cases are retained separately, not weakened.

The private source is committed ce40 plus explicitly scoped committed/candidate
changes, not the entire evolving checkout. All nine held providers are sealed;
Clone is the unchanged committed22adeddd… provider and no extreme-offset/resize
Clone diagnostic is run. Earlier8-provider baseline receipts remain unchanged;
its ninth-provider check is explicitly retrospective, not a fabricated pre-build
seal. Final receipts bind all9 before build. Ordinary Meson ASan/UBSan, O2,
assertions enabled, no debug symbols; LSan disabled, dependencies uninstrumented.

All receipts are allowlisted; raw Meson environment JSON is not included.

## Final result

**77 normal +77 ordinary ASan/UBSan PASS**, maximum27.69 seconds per invocation.
All72 original cases remain, plus two geometry cases and three independently
budgeted mutation-angle cases.288 old-path comparisons and288 ink-containment
scenes,168 actual redraw transitions per configuration. The causal missing-damage
mutation fails on224 guide pixels; restored-source full suites pass afterward.
All35 evidence negative controls reject altered claims and incomplete coverage.
The final immediate post-test executable hashes match the build provenance.
`final-first/` preserves the preceding77+77 source-bound receipts without claiming
an executable seal that had not been collected then.

`baseline/model-oncanvas.png` and `final/normal-oncanvas.png` are actual matching
before/after1040x1800 application-window captures on the verified1080x1920 Xvfb
screen, with oncanvas asserted. They show the current app, not the old runtime.
Markers intentionally have8-screen-pixel radius regardless of zoom; the tiny
radial difference is primarily established by the unchanged-source pixel oracle.
Run `check-display.py`, `test-display-evidence.py`, and the fixture's
`verify-source.py --run` in the pinned native dependency environment.
