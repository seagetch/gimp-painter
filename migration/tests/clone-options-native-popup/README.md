# Clone / Perspective Clone Options native popup

Exact legacy gimp-2-8 source at `afa43fae3e920210146abed514f136fd49f671b5` creates an Options frame popup in both horizontal factories: `gimpclonetool.c:105–176` and `gimpperspectiveclonetool.c:885–970`. The frame helper anchors its own label/down-arrow button with reliefNONE; GimpPopup uses GTK_WINDOW_POPUP and GTK grab, Escape/outside dismissal with outsideFALSE, no global Return/Space confirmation or rollback. Old executable behavior is not claimed. The old Clone source's uninitialized table attachment at171–173 is not copied or treated as evidence of a working old layout.

The bounded target is only the current shared Source entry when its options are GimpCloneOptions (including derived PerspectiveCloneOptions). It becomes a self-anchored native Options entry while retaining its stable widget name. Heal is not GimpCloneOptions: its current Source/GtkPopover grouping remains untouched and unresolved against the old inline sample-merged/alignment layout. Other helpers and toolbar groups remain separate.

Current clone-type enum, sample-merged versus Pattern visibility, alignment binding and Perspective Clone's inline mode remain native current controls. Old radio grouping versus modern enums/layout and old fresh construction versus current retained body are explicit differences, not full pixel/control/identity parity. Accessible clicked release/keyboard activation modernizes old press timing.

The outer popup edits live state without implicit save/rollback; nested stock Pattern keeps its staged child context and Return/Space confirmation and native consumed outside-click confirm/cancel policy. Shared pending-child lifetime behavior is reused, not replaced with the outer policy. The renderer forced-dispose residual from actual task300/301 remains a separate issue, not old2.8 runtime evidence or a universal safety claim.

Implementation reuses the completed self-anchor controller; acceptance consumes completed native input/oracle prerequisites. Bucket/Ink publication order is not itself a semantic dependency. The standard-tool-options parent, Heal grouping, other popup families, full port and old-runtime/full-layout fidelity remain open.

Final qualification: **132 distinct normal cases** (own7+existing125), zero Xvfb skips. Scoped18TU ASan/UBSan runs nine cases: Clone7 plus the two inherited rectangle input cases; vptr excluded, LSan disabled, GTK/unit-store and remaining libraries uninstrumented. Two exact actual305 carrier negatives use the final test object and fail as intended. Four WM captures repeat existing cases, not extra unique passes. Final source hashes, binary/registration/filter/runtime details and prior diagnostic generations are retained in `regressions/` and sanitizer receipts. No whole-application sanitizer, full-Meson-runner or non-X11 runtime claim.

## Input, lifetime and image qualifications

Actual entry/checkbox/native enum menus and stock open/Escape/Return use XTest X-server events, not human physical input. Pattern row staging uses GTK selection API; model/peer/native-dock API roundtrips are distinct. Outer outsideFALSE is inspected through the real event vfunc, not a physical outside click. Retained owner/tool/Undo invariants and ordinary caller-release remain scoped tests, not universal lifecycle proof.

Four unchanged1080×1920 raw PNGs show both entries at scales1/2 using the official singlewindow/default-oncanvas fixture and genuine file-backed Pattern resources. The target popup is fully onscreen; compact enum/name text may be ellipsized and high-DPI surrounding docks clipped. No full UI fit or old-layout identity claim. Bounded X-property excerpts are derived postcapture samples with raw hashes, not simultaneous screenshot/XID identity. Capture repetitions do not add unique normal cases.

`fixture-diagnosis.json` separates the earlier borrowed-invoker cleanup mistake from production behavior; a tool switch legitimately retires that toolbar. Corrected cleanup preserves the existing assertions and uses the surviving canonical option object, without a production fix for that diagnostic.

## Narrow inherited-test adaptations

Ink's unrelated-scope control previously used Clone and asserted it stayed GtkPopover. Since this task intentionally converts Clone, that control now uses untouched Heal with the same carrier/lifetime assertions; no case is skipped or production policy relaxed.

A subsequent inherited rectangle test captured a non-viewable native window immediately before input: the task303 readiness helper asserted viewability after its wait but had omitted it from the waited predicate. This task adds nonnull/viewable native-window conditions to that same bounded3-second wait, retaining the final assertion, one-click policy and every row/model/reentry outcome. It repairs the test helper, not Clone production, and does not establish the cause of older uncaptured callback failures. The superseded adapted run's64 passes,1 failure and60 unexecuted cases remain separate history, not final acceptance.
