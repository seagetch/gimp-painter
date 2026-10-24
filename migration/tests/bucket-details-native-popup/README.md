# Bucket Fill Details native popup

## Bounded source contract

Exact old gimp-2-8 source at `afa43fae3e920210146abed514f136fd49f671b5`, `app/tools/gimpbucketfilloptions.c:224–245`, calls the horizontal frame-popup helper for Details. `gimptooloptions-gui.c:327–365` creates a label/down-arrow button anchored to the button itself. Its GimpPopup carrier uses GTK_WINDOW_POPUP and GTK grab; Escape/outside dismiss, outside handler returnsFALSE, no global Return/Space confirmation or Apply/rollback transaction. This is source reference, not execution of the old application.

Only the actual Bucket `gimp-painter-fill-details` entry changes. A small Bucket-named wrapper reuses task302's retained-body self-anchor controller. Other callers of the generic Details helper, task301 toggle API and task302 Transform API remain unchanged. Current controls and model bindings are retained; ordinary dock widgets are not stolen or reconstructed. Accessible clicked release/keyboard activation remains an explicit modernization of old press timing.

## Deliberately unchanged layout/model differences

Old Fill Type was inside Details; the current fill mode remains inline and Pattern remains in Details. Old `fill-selection` boolean maps to the current `fill-area` enum; similar-color controls remain sensitive only for SIMILAR_COLORS. Modern diagonal-neighbors/antialias and current numeric/enum definitions remain. Line-art/stroke controls stay in the full native dock. This carrier change does not claim all old control/layout parity.

Outer edits remain live without implicit save/rollback. The stock Pattern child retains its own staged context/confirmation policy, including native Return/Space and outside consumption; native enum menus keep their normal behavior. These nested controls are not blindly converted to the outer popup policy.

The forced-dispose renderer/context crash recorded during task301 was reproduced in actual task300 GtkPopover baseline and task301 native-carrier candidate, not the old2.8 runtime. It remains a separate unresolved lifetime issue, not an unsupported-operation label or universal safety claim.

## Evidence status

Final acceptance on actual303: **119 distinct normal cases pass with zero Xvfb skips** (own6+compact62+standard-toggle17+Transform8+toolbar26). Scoped17TU ASan/UBSan repeats six cases, excluding vptr with LSan disabled and remainder uninstrumented; not whole-application proof. One exact actual303 native-entry negative uses the same final test object and fails old GtkPopover as expected. Two portrait executions are repeats, not additional unique cases. Source/binary/registration/filter qualifications are in JSON receipts and `regressions/`. The broad standard-tool-options parent, other popup families, full port and old-runtime/full-layout visual identity remain open.

## Input, geometry and image qualification

Actual entry, numeric keyboard edits and enum menus use XTest X-server events, not human physical input. Pattern row choice uses the GTK selection API; its invoker/Escape/Return use XTest. Five actual-input cases require runtime X11; ordinary private caller-release retains its portable path. Authorized Xvfb runs exercise all six with zero skips; no non-X11 runtime claim.

Anchoring is checked against the exact synchronous opening snapshot and edge-flip equation. Later scroller movement is logged separately; no continuously tracking popup promise or numeric tolerance is invented. Raw scale1/2 PNGs are1080×1920, official singlewindow/default-oncanvasTRUE with Openbox and shipped file-backed Pattern resources. The target popup and named control bounds are visible; no all-brush corpus or full UI fit claim. Existing horizontal adjustment is used to reveal the real entry; no edited pixels or synthetic popup wrapper.

Public X-property files are bounded derived excerpts matching the measured mapped override-redirect popup. Sampling occurs after raw framebuffer capture, not proven simultaneously; original hashes and matching XIDs are recorded. Raw full samples remain private. The two capture executions repeat the actual-canvas case and do not add unique normal cases.

`fixture-input-diagnosis.json` documents the removed later-stationarity assumption: an earlier criterion test observed opening301 versus later306 as viewport/adjustment changed. The final exact opening-snapshot/flip oracle reflects the source contract without tolerance or production repositioning; prior failed diagnostics are not passing counts.

## Rebase and prerequisite roles

The implementation directly reuses completed task302's self-anchor controller. Acceptance also consumes actual303's completed test-only input/oracle prerequisite. The Bucket production delta is separate from that prerequisite; only the declared Bucket wrapper/route and its test registration/include are in this change. Pre-dependency rectangle/Transform failures and earlier held candidates remain historical private evidence. Fresh passing rebased gates do not prove their exact historical missed-input cause, erase those failures or complete unrelated UI work.
