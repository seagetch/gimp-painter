# CellRendererPopup native type and callback contract (08.016)

The original acceptance, “cell renderer の vfunc と callback を接続”, is
implemented through the native composition already recorded in cpp-vfunc-order.json:
GimpPainterLayerTiles, native GtkButton/GtkListBoxRow/GimpView presentation and
GtkPopover. No obsolete GtkCellRenderer vtable, duplicate legacy GType, global
C++ class singleton or Interface::cast is introduced. All six assigned source
files retain source-specific duties in ../tests/renderer-type/source-duty-review.json.

## Type and dispatch replacement

Legacy constructed installs the view factory and popup decorator. Current native
widget construction registers typed Hook/Connection callbacks in the owner slot.
Legacy get_size/render responsibilities route to native preferred-size, allocation
and Cairo draw dispatch on the real popup button, layer row and preview. Native
activation routes include preview right-click, overflow-button clicked and owner
Menu/Shift-F10. Typed retained layer targets replace borrowed tree-path lookup.
The public popup entry and these widget routes resolve the same controller slot.

The source-specific popupbutton and popupper support interfaces are accepted only
for their participation in this native construction/activation/popup callback
chain. The inherited popup parent/dismissal/destroy behavior retains the verified
08.015 contract. These files are not claimed to define a GtkCellRenderer themselves,
and none of their independent duties has been removed or silently reassigned.

Exact old dropdown appearance, active shadow/pixbuf, per-row rectangle anchoring,
old exported C/C++ renderer ABI and complete old tree-view feature equivalence are
not proven here. The separate pixbuf-owner, 29.006/cellrenderer-gtk3, other WBS29,
and C-entry/handle migration tasks remain open. This type acceptance does not waive
those original requirements or invent custom parent calls for an absent subclass.

## Synchronous selection replacement

A right-click first chooses its layer, emitting selected-layers-changed. A callback
can select a different layer on the same image before the original press returns.
The prior image-identity guard allowed that interrupted activation to open a popup
for the newer selection. The native regression reproduces exactly that failure.

choose now returns whether its requested selection still equals the final selected
set and the image generation is unchanged. The press callback stops if not. Set
ordering is not significant. Same-target synchronous completion remains valid;
CTRL add/remove, SHIFT ranges and unmodified multiple-selection preservation are
unchanged. CTRL on the sole selection intentionally leaves the model unchanged.
This is a final-selection guard, not a universal action/selection-ABA detector.

The Escape-after-owner-destruction test passes without an implementation change:
GTK suppresses grab-focus emissions for the insensitive disposed owner. A global
owner-filtered emission hook survives object disposal and observes that safety
property; it does not assert that no call to gtk_widget_grab_focus was attempted.

## Verification and limits

Seven added tests cover native size/draw, four activation routes, superseding
selection, same-target completion, modifier behavior, Escape teardown and retained
stale preview signals. RTL is set/asserted on every measured target; allocation
is explicitly synchronized before Cairo drawing. Native events wait for GdkWindow
viewability. Stale preview signals intentionally bypass native event delivery after
destruction, when no GdkWindow exists; they prove inert disposed widgets, not
explicit Connection teardown, finalization, or leak freedom.

The unchanged accepted 08.015 controller fails only the intended newer-selection
no-popup assertion; all six positive controls pass. Final normal and standard
Meson ASan/UBSan builds each pass five registrations and 143 cases: canvas44,
compact11, tool-owner1, foundation70, MyPaint-tool17. Native scale-2 UI passes 55.
Source hashes are checked before/after. RTTI/vptr stay enabled; LSan is disabled.
Portable reports contain actual TAP and source identities, with raw attempts
retained in the checkpoint. This is not the all-registration aggregate, OS input
routing, tablet hardware, exact visual parity, installed-prefix or platform acceptance.
