# Original WBS 08.008: registered MyPaint tool and owned lifetimes

The current native GimpPainterMybrushTool derives directly from GimpColorTool,
registers in gimp_tools_init, receives GimpPainterMybrushOptions and associates
with the extended GimpPainterPaintGate. The ordinary GIMP MyPaint tool remains a
separate registered type. Three missing public class macros are restored here.
The actual GTK registration test selects Painter through the real tool manager,
checks its type, parent, instance/class sizes, class interface, options and paint
core association, then constructs its options GUI. The direct low-level PaintCore
start refusal still passes; public generic stroking has its own accepted route.

## Tool construction, stop and final ownership

The old tool creates a raw GimpMypaintCore, registers it as a qdata C++ object and
also deletes it in finalize. The replacement has one common BindingStore slot
owning a strong Session, which owns current options and the GEGL/native paint
controller. Closing moves retained Session/connections into locals, disconnects
observers, clears tool state and cancels before releasing Session. Session keeps
its required options/controller alive while disconnecting and closing. No second
raw core owner or separate production qdata registration is added.

The new standalone lifecycle case exercises idle, pending and committed branches.
It disposes twice, checks control halted, display/drawable lists cleared, preview
thawed, and releases the final tool reference. Pending pixels roll back with no
Undo; committed pixels remain. Real Undo and Redo then reproduce buffers after
the tool object has been finalized. Test-only qdata destructors witness actual
final object release. A first fixture incorrectly used a weak notification for
that distinction: GLib invokes it during explicit dispose, before final memory
release. Its failed result is retained separately; no production failure is
claimed for it. Existing HALT/last-owner, public-press last-owner, bare last-owner,
cancel and stationary pressure/Undo cases also pass on current sources.

## Existing GUI weak-pointer child

The pinned old GUI helper registers raw weak slots into its options and widget
members without unregistering them in its destructor. The native editor instead
owns its context/options and controls, with common-store lifetime and Connection
callbacks holding GWeakRef-backed owner handles. There is no callback writing a
NULL pointer into a freed legacy helper slot.

The new direct-widget test avoids the extra model reference held by the older
Editor fixture. Dropping the caller's options ref first keeps options alive while
the editor updates it, then releasing the editor destroys the model. Destroying
the editor first leaves a caller-owned model and retained control usable; changing
the control cannot alter the detached model. After the model is released, another
control emission stays safe and releasing that control destroys it. Actual object
release is witnessed individually. Replacement/close and close-during-refresh
cases are rerun alongside this two-order test. This closes the existing child,
which has no additional source-work rows, without inventing a new GUI obligation.

## Evidence and scope

Seven tool and three editor cases pass on the existing native cloud GTK display
with 29 current source seals. Four focused ASan/UBSan/float-cast-overflow cases
cover the new tool disposal/Undo ordering, public-press ownership, editor order
and close during refresh. The build reports enumerate instrumented versus
RTTI-only sources; remaining GIMP and dependencies are uninstrumented and
LeakSanitizer is disabled. No historical source hashes are rewritten into a new
pass. The first attempted run stopped before any test because a generated menu
was missing; the normal menu build targets restored it. Nonfatal missing test
application-icon and configured test data-folder diagnostics remain visible in
the logs. They are not sanitizer failures or proof of a clean full application run.

One broad-profile routing error is corrected separately from acceptance. Hunk
001098 adds only perspective-guide registration, proved against its pinned blob
and payload hashes. Its erroneous MyPaint TODO is removed; its eight remaining
duties, including 08.010, remain unchanged and unresolved. That removal yields
zero completed duties. Seven actual MyPaint source-specific duties are accepted.
The restored Git database lacks the old full source/base trees, so the routing
proof uses the exact target hunk/rule and unchanged-other-row checks instead of
claiming a complete historical regeneration. Whole-file anchors close only this
type/lifecycle contract. Full brush rendering, all physical input/platforms and
separate rendering or UI acceptance remain outside this task.
