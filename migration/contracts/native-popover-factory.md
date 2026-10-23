# Original `gimp_popover_new` native popup-window provider

Original oracle: `afa43fae3e920210146abed514f136fd49f671b5`,
`app/widgets/popupper.{h,cpp}` (full files archived with the evidence).
This task restores the sole public C factory, not the old C++ Interface or
Decorator family and not the paused cell-renderer popup ownership candidate.

| Original behavior | Native provider |
|---|---|
| `GtkWidget *gimp_popover_new(GtkWidget *view)` | Exact C signature and callable symbol |
| Runtime `GimpPopover2`, GtkWindow popup, nonresizable | Same runtime name/base/window mode; not GtkPopover |
| GTK_SHADOW_OUT frame, inserted view shown/focused | Same GTK containment/focus behavior |
| `cancel` and `confirm`, RUN_LAST/action default close | Class action slots close the native window |
| Parent map then GTK grab | Native map acquires grab only while still active; unmap/dispose balance it |
| Escape cancel, Return/space confirmation disabled | Same binding choice |
| Outside button strict `<0` / `>width,height`; other top-level | Same strict boundary, child remains inside; NULL event target safely rejects |
| Constructor / GTK ownership | GTK-managed top-level; caller retains for extra ownership and destroys to close |

A single BindingStore slot records the frame weakly; GTK alone owns the child.
The C++ PopoverRef calls the same C factory and **retains** its GTK-managed
nonfloating result. It must not adopt or sink GTK's own top-level reference.
GTK destruction disposes the widget and clears weak pointers even if a strong
C++ handle survives; qdata finalization is a separate event. Releasing a handle
alone does not close the GTK-managed popup.

Native safety differences are deliberate: NULL/non-widget/top-level/already
parented/destroying views reject without publication. Constructor failure before
insertion leaves a floating input untouched. After child insertion, arbitrary
synchronous GTK callbacks may destroy/reparent the view: a NULL result does not
promise rollback. Provider exceptions at initializer and post-activation are
contained at C/GObject boundaries and leave a closed, disposable raw object.
No unsupported rollback, old GTK2 binary layout, or foreign-thread access claim.

Remaining original C++ duties stay separate and traceable: show positioning,
show_over monitor/anchor placement, replacement view semantics, Interface casts,
and decorator callbacks/scroll/confirm integration. The paused renderer source
and its tests are neither edited nor invoked by this fixture. The independent
popup is displayed over a real Gimp image with oncanvas UI enabled for portrait
Xvfb evidence; ordinary GTK entry/label children do not activate the renderer.
