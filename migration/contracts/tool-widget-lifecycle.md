# ToolTileView GTK3 parent and teardown contract (08.014)

The legacy GimpToolTileView GtkBox subclass is replaced by the existing native
vertical GtkBox inside a GtkScrolledWindow and overlay frame. The display-shell
Canvas controller owns one BindingStore slot. Native GTK parents handle child
layout/destruction; weak-owner Connection hooks retain targets when needed, and
the coalesced refresh Source checks the owner generation. No second custom
widget type, decorator store or legacy Interface::cast is introduced. The two
source-specific duties are mapped in ../tests/tool-widget-lifecycle/source-duty-review.json.

## Synchronous callbacks and terminal teardown

Closing the shell binding cancels refresh, disconnects tool/model/control signals
and destroys panel children. Retained controls cannot revive a closed controller.
The binding owner lease protects storage lifetime; a controller-local tool
revision separately protects a live operation across synchronous notifications.
Tool selection, context changes and expansion check closed/revision state after
signal-emitting model calls, retaining borrowed group/tool objects across them.
A newer nested selection must not be overwritten by the interrupted operation.

The standard tool manager can complete the same requested selection while a
group active-tool signal is being emitted. That is not a conflicting newer
choice: the controller restores canvas focus without replaying model changes.
A closed owner or a newer different tool receives no stale focus handoff.

## Fresh verification

Four isolated original-baseline tests reproduce forbidden post-close continuation
at active/expanded group callbacks and overwrite of a newer nested choice.
Close fixtures use an unrelated active Paintbrush context so the standard tool
manager's earlier current-group synchronization is not mistaken for a defect.
The native-parent/pending-refresh/weak-finalization control passes baseline.
A current-group selection focus control passes the original baseline, exposed
a real interim-fix regression, and passes the final correction.

Final normal and standard Meson ASan/UBSan builds each pass all five selected
registrations, 131 TAP cases: canvas33, compact-options10, tool-owner1,
foundation70 and MyPaint-tool17. The full standard build completed before GUI
testing; final source changes were rebuilt normally. Both runs check source
hashes before/after. RTTI and vptr instrumentation remain enabled. Full logs and
failed attempts are retained privately; portable reports contain actual TAP,
source hashes and results without raw environment metadata.

Leak detection is disabled. This is not the full all-registration aggregate,
physical tablet testing, cross-platform acceptance, or complete WBS29 visual,
group persistence and null-icon parity. Those independent tasks remain open.
