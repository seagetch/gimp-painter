# Native popup input and opening-geometry test oracles

This is a test-only qualification on actual task302 (`052a64ab0af3efa2b14835731f5f9cc85bcc9fc5`), not a Bucket implementation or production fix. Only `test-painter-rectangle-unit-popup.inc` and `test-painter-transform-popup.inc` may change; all production remains at the published baseline. Bucket's separate candidate and evidence remain pending.

## Completed inputs, not broad feature completion

The existing compact regression gate is recorded by completed `34.015/compact-options-native-oracles`. Its published `migration/tests/compact-regression-gates/results.json` (Git blob `d49c8517387a683afe60e1cd1c7e0c7f4305b274`, verified against actual302) contains `full-three-final-true`, exit0,62 passes including both `/painter-compact-options/rectangle-units-real-popup-events` and `/painter-compact-options/rectangle-units-popup-model-reentry`. This is a completed test-gate prerequisite, not a claim that the entire Rectangle independent UI is complete.

Completed `29.010/transform-details-native-popup` supplies the actual self-anchored Transform Details contract. The broad standard-tool-options and full-port parents remain open. No unfinished feature umbrella or unrelated chronological dependency is substituted for these inputs.

## Scope and historical qualification

Historical rectangle missed-input and Transform geometry failures remain recorded. Adding explicit native-input preconditions does not prove the cause of an earlier missed input. An opening-time positioning contract does not promise continuous tracking of later viewport allocations. Tests must retain native model/menu/geometry assertions without silent retries, discarded failures or numeric tolerance. This bounded change does not mark all historical failures, full UI fidelity or platform behavior resolved.

Final qualification: **113 existing distinct normal cases pass with zero skips** (compact62, shared toggle17, Transform8, toolbar26). No new distinct cases are added. Scoped16TU instrumentation runs ten cases (rectangle2+Transform8); ASan/UBSan excludes vptr, LSan is disabled, unit-store/GTK/remaining code is uninstrumented. Accepted production/reference object and archive hashes match actual302; only the main test TU is rebuilt. See `validation/production-reference-pins.json` separately from the private+7px mutant provenance.

The initial2+8 focused runs, three predeclared reentry repetitions, one representative portrait and expected offset-negative are supporting evidence, not extra unique passes. The separate mutant fails217 versus expected210 while accepted production remains unchanged. Every terminal result and source/binary pin is in `validation/validation-summary.json`. Registration fixtures differ deliberately; not all existing toolbar tests use the oncanvas profile. No Wayland runtime or full-Meson-runner claim is made.

The representative raw1080×1920 Rotate scale1 Openbox image repeats an existing case on unchanged actual302 production. It is byte-identical to the previously reviewed image, not a new UI feature/full matrix. Native Details shows live73 while the existing operation dialog remains; no image edits or old2.8 visual-identity claim. Bounded X-property excerpts are sampled after capture and explicitly derivative.

## Exact test changes and input boundaries

The synthetic rectangle reentry fixture now observes mapped/realized/sensitive/viewable state, expected parent grab and stable after-paint geometry before input. It recomputes a scale-aware physical target after pointer motion. A fresh unit is introduced only after preparation, keeping its unseen store frontier observable. No fabricated menu rows, direct model synchronization, click retries or weakened mutation outcomes are added. The GimpUnitStore unseen-frontier explanation is a source-supported race candidate, not proof of the historical missed prerequisite.

Existing reentry scenarios0/1 open through XTest. Destructive scenarios2–4 intentionally retain their prior direct `gtk_widget_show(menu)` ownership path because the GTK positioning caller does not support synchronous destruction during show. They are not claimed as five fully native-input scenarios. All five existing row/model/destruction outcomes remain, and the companion rectangle real-events test separately covers actual input.

The Transform oracle uses the intended button's synchronous premap opening snapshot and exact edge-flip equation, independently of the observed popup coordinates. Later toolbar/scroller allocation is logged, not required to remain stationary. There is no tolerance or production repositioning change. A separate private+7px carrier mutant must fail this oracle; it is never linked as accepted production.
