# Legacy layer-filter popup behavior contract

Exact `seagetch/gimp-painter:gimp-2-8` commit `afa43fae3e920210146abed514f136fd49f671b5`. All facts below are source inspection only; old application was not executed. These are the **filter-layer popup** semantics, not MyPaint editor semantics and not the ordinary Colors-menu plug-in dialog.

## Proven data flow before Apply

`app/widgets/gimplayerpopup.cpp` PopupWindowDecorator owns `proc_name` and local `proc_args` at86–87. `update_filter_edit`217–246 loads the current layer's arguments only if selected procedure matches the layer's stored procedure (226–227); otherwise it creates default argument values (238–245). It then rebuilds parameter widgets248–250.

The stored-argument getter is not a live reference to the runner's array: `app/core/gimpfilterlayer.cpp:667–675` calls runner->get_args, and `app/pdb/pdb-cxx-utils.hpp:270–272` returns `g_value_array_copy(args)`. The popup's numeric controls mutate its local GValue array: double402–405/420–423, boolean513–515, enum532–536, integer627–630/645–649. They do not call filter-layer setters or invalidate the filter before Apply. Thus these **connected filter parameters are drafts**, not live model edits.

There are legacy incomplete controls: generic RGB branch662–681 creates a GimpColorPanel but connects no color-change callback to proc_args; string683–694 similarly creates an unbound entry. Some Boolean branches also lack callbacks. Specifically do not claim that old Colorify RGB picker edits were correctly committed by this code. Colorify's procedure itself is admitted by the selector, but its old generic editing completeness is not established. Preserve working modern Colorify exact-value backend rather than reproducing missing bindings.

## Apply and repeat Apply

At1094–1099 a GTK_STOCK_APPLY button calls `FilterLayerInterface::set_procedure(this->proc_name, this->proc_args)`. No close or hide follows. `app/core/gimpfilterlayer.cpp:630–656` discards the existing runner, looks up selected procedure, creates a new ProcedureRunner, copies non-image/drawable/item arguments via set_arg650, then filter_reset655 and invalidate_layer656. `pdb-cxx-utils.hpp:275–279` transforms each supplied value into the runner argument slot.

Therefore Apply commits the current draft to the same stored layer and keeps the popup open. The local draft remains available for subsequent edits/Apply. There is **no explicit no-op equality check**: repeated identical Apply still invokes runner replacement/reset/invalidation. No explicit Undo push appears in this path; do not claim precise old Undo semantics from it. Modern safe no-op/Undo behavior should be described as preserved backend behavior, not proven exact legacy internals.

## Operation selection change

Procedure-tree changed callback1048–1061 rebuilds parameter UI then stores chosen proc_name. The rebuild loads stored layer args when matching its current procedure, otherwise defaults. It does not cache separate drafts per procedure. Source-derived consequence: switching away before Apply discards that operation's unapplied draft; switching back to the stored operation reloads saved values. After applying another operation, that newly applied operation becomes the stored one; an older operation selected later gets defaults. Selecting a category/empty name can set empty proc_name without rebuilding controls; this is a legacy edge case, not a behavior to reproduce unsafely.

## Dismissal and what persists

Carrier `app/widgets/popupper.cpp:136–149` binds Escape to cancel; global Return/Space confirm bindings are disabled. Outside click191–222 emits cancel and consumes it. Cancel241–248 removes its GTK grab and destroys the popup. Confirm/close251–262 also only destroys. Neither path calls set_procedure or restores a prior layer definition.

Consequences, based on the data flow above:
- Filter parameter draft without Apply is discarded on Escape/outside close.
- Already applied definition remains committed after close.
- After Apply, later un-applied changes are discarded; previous Apply remains.
- Reopening loads the current stored definition, not a retained prior popup draft.

Do **not** generalize this to every control in the same popup: opacity835–838 and lock-alpha852–853 directly mutate the layer immediately; layer mode selection likewise uses direct layer operations. Those live property changes persist on dismiss independently of filter Apply. The “Live update” checkbox1090–1092 is merely constructed/shown; no handler is installed, so no live filtering contract is proved.

## Stale or deleted layer

The popup remembers the originally supplied layer at823. The inspected class has no definition revision comparison, attachment check, removed/image-disconnect handler, operation-generation guard or Apply-in-progress guard. Apply blindly calls the stored layer interface; core set_procedure636 dereferences its image. Therefore **no safe old behavior for external modification/deletion/reentrant Apply can be claimed**. There is no evidence that selection changes retarget the open draft. Do not reproduce a stale dereference or lost-update hazard for parity.

Modern leaf should retain existing port's weak refs, attachment/image checks, checked definition revision, schema validity, callback generation and safe close-on-owner-loss protections. Tests for those are safety requirements of the port, not assertions that legacy implemented them.

## Implementation target distilled

1. Actual anchored GTK_WINDOW_POPUP from layer entry, inline filter draft controls.
2. Explicit Apply mutates intended existing layer and leaves popup mapped.
3. Escape/outside destroys, no implicit Apply, no rollback of prior committed Apply.
4. Operation switching behavior is specified explicitly; default reload versus retained-draft choice must not be invented accidentally.
5. Separate live layer-property behavior from staged filter definition behavior.
6. Preserve modern supported-route validation, numeric identity, no-op/Undo safety, stale protection and working Colorify controls; disclose those as port guarantees rather than literal old internal behavior.
7. Ordinary/new-filter dialogs remain unaffected. Actual published298 base and independent design approval required before implementation.
