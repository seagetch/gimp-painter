# 09.013 — actual projection notification and visible canvas

Original acceptance: **変更領域が表示へ伝わる**. Dependency: 09.012.

The real painter tool, Surface/native paint core, drawable update, projection
signal, display handler and GTK canvas are exercised together. An actual
1080×1920 Xvfb monitor/window has `painter-canvas-ui=TRUE`. Unmodified root-window
captures and a transformed canvas-pixel oracle show initial blue, a live red
stroke, canceled blue and committed red. Between tool motion/cancel and the
capture the fixture only runs the main loop: it does not flush projection,
invalidate the canvas or trigger pixel reads to force hidden reevaluation.
Projection notifications must cover the changed point. The native buffer is
independently compared exactly after cancellation; commit creates one Undo item.

The original failure was causal: native pixels were fully restored by cancel,
but the actual canvas stayed red. Its before/live/canceled PNGs, failed native
receipt and exact tested production/fixture sources are preserved. The final
fixture adds explicit setup-only projection readiness for sanitizer timing; a
new cancellation-flush-removal mutant fails this exact final fixture. Production now publishes
restored projection/display pixels on a non-busy cancel release, retaining image
and display across cancellation and checking their current association. Reentrant
busy cancellation remains published by the ongoing input's existing final flush.

Separate setup diagnostics are excluded: an accidental conditional registration
produced zero selected TAP tests and enabled the optional mask test without its
capture directory; an initial null fill ROI left a transparent baseline, and a selected sanitizer
run observed the initial screen before setup projection was evaluated. Exact
TAP counts and pixel assertions caught both. No failed setup is counted as UI
verification. The corrected explicit 128² fill asserts native opaque blue first.

The UI proof uses the default byte-precision, zero-offset layer. Existing25 tool
and22 Surface tests remain intact. This does not close the entire429-line legacy
DrawableFeature duty or the source-node/cache invalidation child
09.013/gegl-graph-invalidation (including all CloneLayer/FilterLayer consumers,
offsets and exact reevaluation bounds). Those original duties and the engine/
stroke-state child retain explicit successors and reverse links.

Selected15-unit sanitizer instrumentation includes the actual tool, Surface,
PaintCore, projection/display notifications and GUI test. Dependencies, other
application units, vptr and leak detection are not instrumented. This is not a
whole-application sanitizer claim. Native mutations suppressing painter drawable
notification, projection-to-display forwarding during the active phase, or
cancel display publication are independently tested. The phase marker is inert
in production; it prevents the forwarding mutant from merely failing setup.
