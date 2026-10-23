# 09.014 — exactly-once color placement

No production change is needed: the current placement is verified, not replaced.
Context color enters the drawable's nonlinear RGB evaluator space once, engine
HSV stores floats, composition converts evaluator RGB to native working RGB/Gray
once, and sampling averages alpha-weighted native working values before its one
working→evaluator conversion. Background follows the same ingress boundary.

The new standalone production Surface test covers 24 composition/background
cells (RGB/Gray, builtin/custom profile, all three TRCs, incremental/floating),
12 sampling cells (five distinguish wrong conversion-before-average ordering),
and four byte Gray policy cells. Full native precision harness passes five groups
including the unchanged 144-cell precision/history matrix and new 16 actual
PaintCore context-ingress cells. Together with the three registered standalone
groups this is eight native TAP groups. A focused eleven-unit native sanitizer
runs the new ingress group; separately Surface's three groups pass ASan/UBSan.
Dependencies are uninstrumented; native vptr and all leak detection are excluded.

The oracle spells out Babl domain boundaries independently of NativePixel helper
calls. Custom RGB linear gamma has an additional analytic check. A direct GEGL
source→native color transform is separately bounded at 3e-4: custom Gray via an
intermediate RGB evaluator is not bit-identical to the direct profile route.
The first fixture assumed 3e-5 direct-route equality and failed by 3.14e-5;
that excluded setup receipt is retained. The final exact two-boundary oracle uses
2e-6 for actual engine float RGB/HSV ingress; Surface double composition uses
2e-14. This is not a production defect or a relaxed native pixel precision claim.

Historical nonlinear-byte Gray deliberately uses evaluator red and replicated
Gray sampling, including custom profiles. The prior 193-record original oracle
and compatibility contracts are unchanged. Floating erasure reveals the original
snapshot; it does not use incremental eraser background composition.

Four real production mutants fail: missing/doubled Surface ingress, missing
sampling egress, and omitted native context→profile conversion. Native sources
are restored and retested. These are regression sensitivity controls, not claims
that the preexisting implementation failed. Portrait Xvfb is verified for native
runs; these model tests do not render UI or claim screenshot evidence.

verify.py verifies immutable receipts/archive hashes, counts and rejection
results; --check-source additionally binds current sources. Other high-precision,
boundary, Undo and whole legacy feature obligations remain open in their tasks.
