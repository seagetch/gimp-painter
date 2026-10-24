# Ink Sensitivity and Blob native popups

## Bounded source contract

Exact legacy gimp-2-8 source `afa43fae3e920210146abed514f136fd49f671b5`, `app/tools/gimpinkoptions-gui.c:99–108`, creates Sensitivity and Ink Blob via the horizontal frame-popup helper. Size and tilt angle remain inline. Sensitivity124–164 contains three sensitivity values; Blob168–203 contains blob-type choices and GimpBlobEditor with bidirectional type/aspect/angle binding. The frame helper anchors its own label/down-arrow button, not a checkbox row. Old GimpPopup uses GTK_WINDOW_POPUP, GTK grab, Escape/outside dismissal with outsideFALSE, no global Return/Space confirmation and no Apply/rollback transaction. Old executable behavior was not run.

The intended scope is only the actual independent Ink entries `painter-paint-ink-sensitivity` and `painter-paint-ink-blob`, reusing the completed self-anchor retained-body controller. Existing current models/bindings, inline controls, other helper IDs, compact adapter and stock selector policy remain separate. Accessible clicked release/keyboard activation intentionally modernizes old press timing.

Current Blob editor is160×160 with direct packing; old80×80 editor sat inside a GTK_SHADOW_IN frame. Size, framing and spacing differences remain. Current retained editor identity across openings also differs from old freshly constructed per-open content; this modern ownership architecture is not object-identity parity. This task does not claim pixel/layout identity, full control fidelity, old-runtime parity or a completed standard-tool-options parent. No stock Pattern selector exists inside these Ink bodies; inherited Pattern regressions are shared-controller coverage, not invented Ink behavior.

## Open neighboring routes

Clone and Perspective Clone have source-proven old Options popups, but Heal's old sample-merged/alignment were inline. The current shared Source grouping cannot be blanket-converted or declared aligned. Gradient, Region selection, Brush/Dynamics entry/editor mapping and other popup families remain distinct open work. The forced-dispose renderer/context residual documented against task300/301 also remains separate, not an old2.8 runtime or universal lifetime-safety claim.

Final acceptance: **125 distinct normal cases**, own6 plus compact62, shared toggle17, Transform8, Bucket6 and toolbar/window26, all zero Xvfb skips. Scoped18TU ASan/UBSan repeats six cases (vptr excluded, LSan disabled, remainder uninstrumented), not whole-application coverage. Two actual304 carrier negatives and one same-final-test no-unmap mutant fail as expected. Four portrait executions are repeated cases, not additional unique passes. Registration/filter/runtime/binary/source details are preserved in `regressions/` and sanitizer receipts.

Implementation reuses the completed task302 self-anchor controller; acceptance genuinely uses task303's completed test-only input/oracle prerequisite. Task304 is the publication base, not an artificial Ink dependency. All broad feature parents remain open.

## Interrupted drag correction and input provenance

The new native Ink route reproduced a real interrupted drag defect: press/motion, Escape, release outside, reopen and unpressed motion changed canonical aspect/angle while transient `active` remained set. A narrow `gimp_blob_editor_cancel_interaction()` API clears only active/in_handle and queues redraw. It is called solely from the new Ink editor unmap path; it performs no canonical property setters, rollback, synthetic release or global class/unmap behavior change.

Exact actual304 comparison must not be overstated: real Escape dismisses its GtkPopover after bounded animation wait, and reopening exposed active1, but sampled canonical values did not drift. Thus the same full value leak was **not** demonstrated on actual304. Tool-switch/owner-close comparisons passed both baseline and candidate. The final same-test no-unmap mutant separately reproduces native canonical drift; earlier setup errors and diagnostic comparisons are nonadditive supporting evidence.

All three sensitivity properties roundtrip through API-driven canonical/peer/native-dock assertions; one typed sensitivity edit uses XTest. Blob shape choices and actual handle dragging use XTest X-server events, not physical human input. No claim is made that every numeric/API edit is native input. Opening geometry uses the actual button snapshot and exact edge flip, not continuous later-scroller tracking. Raw scale1/2 images cover both entries; X-property excerpts are bounded, derived samples after capture, not simultaneous pixel/XID proof.
