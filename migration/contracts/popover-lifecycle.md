# Popover parent, events and teardown contract (08.015)

The legacy Popover/GimpPopup custom GtkWindow, popup button and popup-consumer
entry contracts are replaced by native GTK3 GtkPopover/GtkMenuButton objects
under the existing layer and compact-options controllers. The controllers retain
the single BindingStore, Connection and ObjectRef ownership design. No duplicate
legacy GType, Interface::cast or standalone lifetime manager is introduced.
All seven original source duties are mapped in
../tests/popover-lifecycle/source-duty-review.json. The existing Meson source
entries register both current controllers; the original Makefile popupbutton
hunk is accepted only for this replacement's parent/event/destroy behavior.

## Native parent and event behavior

A layer properties popup is relative to the tile owner; a creation popup is
relative to its Add button. Compact popovers are relative to their GtkMenuButton.
The native popover owns the scroller/viewport/content parent chain and handles
modal grabs, Escape and inside/outside button events. Native closure restores
the existing controller focus/pointer state. Compact controls retain their
canonical model identity and can reopen in the same native popup. Layer popup
replacement disconnects the old popup's handlers before destroying its content.

The legacy Return/Space confirm bindings and grab-loss cancel emission were
commented out. This task does not invent those semantics, export the old custom
class slots, or claim renderer drawing/pixbuf/ABI compatibility. Those separate
08.016 and 29.* obligations remain open.

## Synchronous replacement and terminal ownership

Closing one popup can synchronously invoke an owner popup-closed callback that
opens another popup on the same image. Image generation alone cannot distinguish
that newer request. A separate popup operation generation now invalidates the
outer request immediately after close_popup. The newer popup survives; the
superseded outer request returns null without replacing it or adding stale hooks.
Owner closure and image changes retain their existing independent guards.

Retained old action widgets cannot mutate the image after retirement. Popup-first
and owner-first destruction, repeated cancellation, containing-window teardown,
acquired-grab release and weak finalization of retained popup/control objects are
covered. Compact teardown disconnects handlers and restores or destroys borrowed
controls through the existing Presentation close/restore path.

## Fresh verification and limits

Two isolated baseline regressions reproduce the stale outer replacement for
properties and creation requests. Three additional positive cases verify native
parent/anchor/scroller behavior, repeated Escape/inside/outside dismissal, actual
grab acquisition/release and popup/window destruction order. Fixtures wait for
GdkWindow viewability and then sample child allocation before injecting native
GtkWidget events; mapped GtkWidget state alone precedes frame-clock remapping.
The initial compact and intermediate layout-timing fixture failures are preserved.
No compact production change was needed.

Final full standard normal and ASan/UBSan builds each pass five selected
registrations and 136 TAP cases: canvas37, compact11, tool-owner1, foundation70,
and MyPaint-tool17. Native scale-2 canvas/compact passes 48 cases. Source hashes
are unchanged before/after all final runs. RTTI and vptr instrumentation remain
enabled; leak detection is disabled. Portable reports contain actual TAP and
source hashes; full raw logs and failed attempts are retained privately.

This is not the complete all-registration aggregate, OS input delivery, physical
pen testing, all WBS29 behavior, cross-platform acceptance or leak detection.
