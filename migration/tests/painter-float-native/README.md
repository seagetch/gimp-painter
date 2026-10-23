# Original 13.012: explicit nonlinear float native integration checkpoint

The new `gimp:painter-float-mode` and IDs73..81 are distinct opt-in modes labelled
`(Painter float nonlinear)`. Existing Painter byte IDs64..72 and old wire values
are unchanged. New default-menu entries append without changing its initial
selection. The cached direct PaintCore route explicitly initializes the float
adapter. Pass-through single-child reduction is disabled narrowly for these
modes because Replace/swapped-IN/OUT mask and opacity are not generally movable;
ordinary modern reduction remains unchanged.

## Native arithmetic proof

45 independently generated scenes (nine modes x five opacities), each64 RGBA
pixels with signed/HDR RGB, alpha0/1/fractional and mask0/1/fractional, compare
against the separate Python premultiplied-double reference. Each is tested as:
real GEGL graph, cached PaintCore direct dispatch, native flat image, isolated
group, two-child pass-through group, and single-child pass-through over an
external backdrop. Thus270 contexts /17,280 RGBA pixel comparisons per run.
Native image storage here is nonlinear float32; this does not claim lossless
float64 composition or linear-light/physical HDR semantics. Source buffers are
not rewritten by the compositing operator.

All eight app tests pass, including existing exact-old byte kernel/graph/image,
216 direct byte cases, six-storage byte compatibility, group reduction controls,
XCF transport and reorder Undo regressions. Focused ASan/UBSan instruments six C
units: byte operator, float operator/kernel, cached registry, group and harness.
Other GIMP/XCF/C++/GEGL dependencies remain uninstrumented; LSan is off. This
receipt is not whole-HEAD or full persistence instrumentation.

`migration/fixtures/painter-float/` records deterministic independent extension
vectors, not old-app captures; the generator is
`migration/tests/painter-float-kernel/generate-native-vectors.py`. Existing old
capture files are unchanged. The isolated kernel's larger boundary/alias matrix
is separately documented and is not counted as native-image proof.

## Public/UI/serialization boundaries

Static C11/C++14 app/public/PDB identity probes pass, including reserved upstream
slot63, unchanged old byte values and all nine explicit new values. Runtime
public GType against the rebuilt libgimp also passes. All nine UI labels/values
select the corresponding real layer mode through the ordinary dock combo changed
signal; the actual1080x1920 Xvfb screenshot visibly shows oncanvas controls and
the full selected Replace label. This is widget-signal integration, not pointer
event synthesis. The illustrated canvas is u8, not the numerical HDR fixture.
All four UI tests pass focused standard-mode-widget/C-harness ASan/UBSan; other
UI/C++ code is uninstrumented. Registered C11/C++14 header compilation passes
including both new private headers. The saved
`public-enums-before-library-rebuild.json` records stale-library detection during
build preparation, not a mode-arithmetic regression: the first targeted build
named an existing symlink rather than Ninja's versioned library output.

XCF persistence is owned by a separate companion checkpoint: native enum numbers
must not be treated as universally reserved wire IDs. No backwards-readable
appearance promise is made; the pre-extension reader diagnosed unsupported
modes. The reviewed versioned-capsule/fallback policy supersedes direct raw
PROP_MODE persistence. Original13.012 remains DOING pending integration and its
original dependencies; no original criterion is removed or weakened.
