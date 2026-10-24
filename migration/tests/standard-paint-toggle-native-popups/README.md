# Standard-paint Jitter / Texture / Smoothing native popups

## Bounded legacy contract

Exact legacy `seagetch/gimp-painter` gimp-2-8 source at `afa43fae3e920210146abed514f136fd49f671b5` routes these three controls from `gimppaintoptions-gui.c:512,557,598` through `gimp_tool_options_toggle_gui_with_popup`. In `gimptooloptions-gui.c:403–413` the enable checkbox changes arrow sensitivity only; line440 anchors the popup to the entire checkbox/arrow row. Values are live, with no Apply, save or rollback transaction. Old GimpPopup uses GTK_WINDOW_POPUP, Escape/outside dismissal, no global Return/Space confirmation, and no unconditional close on grab loss.

The port changes exactly these three call sites. A row/button-owned controller retains the existing controls and model bindings, lending its body to a fresh task298 GimpPainterPopup host. Closing detaches the retained body before host destruction. The existing stock Pattern selector remains a real nested GimpContainerPopup with its own confirmation policy; pending-child admission and deferred body release protect nested cancel/confirm/direct-destroy/reopen and multiple-child lifetime transitions. Other helper callers, compact adapter, brush/dynamics/source/Ink/Expand routes and stock selector policy are unchanged.

Accessible GtkButton::clicked release/keyboard activation deliberately replaces old button-press timing. This is not a claim of byte-for-byte input timing or execution/visual identity of the old application. The broad standard-tool-options parent and full port remain unfinished.

## Evidence

- `source-inputs.json`: frozen six-file source seal and actual300 base.
- `normal-summary.json`, `normal-results.txt`:17 distinct focused native cases, including three actual canvas entries, three baseline carrier oracles and nested lifecycle/reentry/model release coverage.
- `negative-controls.json`: final test source fails all three real oncanvas entries against actual300, and a source-derived first-child-release mutation fails body-retention while another child is pending. Earlier factory carrier REDs and the earlier same-test-object pending-reopen RED/GREEN pair are separately labeled supporting controls, not extra final-source tests. These expected failures are not passing cases.
- `sanitizer-summary.json`:17 repeated cases pass with15 translation units instrumented by ASan/UBSan excluding vptr, O1/frame pointers; LSan disabled, remaining archives/dependencies uninstrumented. Not a whole-application sanitizer result.
- `portrait-captures.json`: six unedited1080×1920 PNGs at scale1/2, official singlewindow fixture/default oncanvasTRUE, real app controls with Openbox3.6.1. Main window maximized and the real toolbar adjustment scrolled to expose the arrow; production popup positioning is unmodified. The tests use native GTK event/test activation, not claimed physical X input.
- `explicit-dispose-residual.md`: forced-dispose renderer/context crash reproduced in both actual300 and candidate, retained as a separate unresolved whole-port lifetime issue. Ordinary release cases pass; the forced-dispose residual is not hidden or mislabeled unsupported.

Final normal coverage is **105 distinct cases**:17 focused +62 existing compact +26 toolbar/window cases; exact names and binaries are in `acceptance-summary.json` and `regressions/`. Supplemental targets use their original registered profiles (toolbar tests deliberately vary configuration), not universally defaultTRUE. No full layer suite or Meson-runner claim.

Scale2 is qualified:14 nonactual cases pass with3 explicit actual-canvas skips in that aggregate, then each of those3 actual cases independently passes under the maximized WM fixture. This is not a single17-pass aggregate; these repetitions do not increase105. An earlier unmaximized scale2 aggregate failed the actual toolbar rowvisibility=-99 oracle; it is not counted as a passing run. Both scale bounds receipts show every popup fully within1080×1920 physical pixels. Texture uses the genuine Pattern corpus; no all-brush corpus or complete dock-population claim is made.

Public X properties are extracted derivatives selecting the last mapped popup sample matching each measured physical rectangle and override-redirect state. Raw sampling includes lifecycle reopen generations; no unique XID throughout the test is claimed. Repeated and unrelated windows are omitted. `x-properties-extraction.json` records raw-log hashes, matching XIDs and extraction criteria. Raw originals remain private; PNGs are unchanged.
