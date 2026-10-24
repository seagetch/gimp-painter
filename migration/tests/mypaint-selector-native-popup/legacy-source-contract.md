# MyPaint Preset/Custom selector popup contract

Reference: `gimp-2-8` commit `afa43fae3e920210146abed514f136fd49f671b5`. This is source comparison, not execution of that old runtime or a full visual-equivalence claim.

- `app/tools/gimpmypaintbrushoptions-gui.cpp` creates a notebook via the popup-button helper (325–351). `gimptooloptions-gui-cxx.cpp:45–54` and `app/widgets/gimppopupbutton.c:555–584` establish a `GTK_WINDOW_POPUP`, not a decorated top-level or GtkPopover.
- Preset uses a plain `GimpContainerEditor` with the live context (256–263). Selection updates the context. Its base editor activation handler is NULL (`gimpcontainereditor.c:132–133,470–478`): double activation does not confirm or close this selector.
- Custom connects `cursor-changed` (295–296); its handler reads the selected row and changes the context (180–190). Keyboard cursor movement therefore selects, without needing row activation.
- The notebook's only tab-specific binding is `PageRemindAction` (303). That helper remembers page; it does not set `use-gimp-brush` or change brush engine.
- Escape/outside cancellation destroys the popup (`gimppopupbutton.c:103–116,188–227,254–261`). Outside handling returns FALSE. Cleanup does not roll back selection or save a resource. Return/Space global confirmation bindings are commented out. Ordinary focused-child key behavior remains valid.

## Bounded GTK3 port

The actual production entry is `gimppaintermybrushtool.cpp` → `ContextRef::create_mypaint_brush_button` → `gimpmypaintbrushoptions-gui.cpp`. Reuse task 298's `GimpPainterPopup` carrier; retain one selector controller/model, body, notebook and views while each host is ephemeral. Detach owned content before host destruction, close on anchor unmap/context or owner teardown, and reject stale/reentrant opening callbacks. The GtkButton accessible `clicked` action opens once on release/keyboard activation rather than recreating the legacy press-only trigger.

Custom uses the existing immutable history snapshots, not legacy mutable pointers. Model synchronization suppresses restore callbacks and preserves the append-only history identity under the cursor across a rebuild so repeated keyboard navigation continues correctly. Tab changes, reopening and dismissal must not themselves restore or save. No stock `GimpContainerPopup` policy is changed: its legitimate confirmation keys and outside-event result differ. This Preset/Custom selector has no nested standard brush-selector entry; no synthetic nested test is claimed as a real legacy route.

Parent UI and full migration remain open. Per-input curve entry/layout parity, other standard toolbar/compact popup families, and nonfilter layer popup routes are outside this task.
