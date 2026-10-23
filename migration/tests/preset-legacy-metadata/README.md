# Original preset GUI metadata, bounded restoration

The original public preference descriptor now preserves all seven fields from
pinned afa43fae's LayerPresetGuiConfig, including capitalization, icon identifier
and empty help string. The native dialog action likewise preserves its original
label, NULL short-label (native getter falls back to label), and help identifier.
The sole native descriptor/action provider remains authoritative; no duplicate
factory, mutable C++ backend or obsolete Interface is restored.

Native presentation deliberately maps the missing historical folder icon to the
installed tool-preset folder icon, and keeps the GtkStack page identity separate
from the public empty help string. Historical visual icon equivalence is NOT
claimed. The existing reset wording and native dock-factory help remain native;
restoring an action's identifier does not create an installed historical help page.

An actual baseline run rejects the old capitalization mismatch. Final normal24
(6 public metadata/ownership and18 existing real GUI cases) and selected7-unit
ASan/UBSan6 pass. One initial GUI failure was an earlier native-only assertion
requiring GIMP_HELP_DOCK for the action; that assertion now requires the pinned
original gimp-layer-preset-dialog, while the separate dock/factory assertions
remain unchanged. The excluded failure is preserved, not labeled a product bug.

The image is actual1080x1920 Xvfb root rendering, with oncanvas enabled and the
preferences page selected through GtkTreeView cursor API. It is not a physical
pointer-gesture claim. Source is committed81bc24545d plus four explicitly archived
owned files, copied before their final builds. Five platform-paused source paths
are exact committed baseline bytes, not candidate callbacks. This is not a whole
current-HEAD run. The sanitizer covers seven named sources, not dependencies,
whole application, vptr or leak detection. The final normal and sanitizer receipts
share the unchanged owned sources; the image-specific final UI case passes too.

08.018/preset-legacy-gui-compatibility remains DOING: dynamic registry enumeration
and mutable config registration still require implementation/verification. Earlier
metadata receipts remain immutable historical native-only checkpoints, not newly
claimed legacy parity. No original source duty or acceptance criterion is removed.
