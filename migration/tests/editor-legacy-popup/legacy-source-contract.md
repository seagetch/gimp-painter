# gimp-painter 2.8 popup source contract

Read-only source investigation, 2026-10-24. Exact remote `seagetch/gimp-painter` branch is **gimp-2-8**, not the literal product label gimp-painter-2.8. GitHub Git ref and branch collection independently resolve it to `afa43fae3e920210146abed514f136fd49f671b5`; the reference is recorded in legacy-reference.json. All line numbers below refer to that commit. No legacy application runtime was executed; this is source parity evidence.

## Actual MyPaint routes

1. `app/tools/gimpmypaintoptions-gui.cpp:573–576`: `gimp_mypaint_options_gui_horizontal()` creates toolbar-mode options.
2. Same file `495–515`: toolbar mode constructs a popup button using `MypaintDetailOptionsPopupPrivate::create`.
3. That callback `357–362`: constructs `MypaintBrushEditorPrivate(context, TAB)`, binds page reminder, and packs the dynamics editor into the popup content.
4. `app/tools/gimptooloptions-gui-cxx.cpp:45–54`: C++ helper creates `gimp_popup_button_new(label)` and connects create-view.
5. `app/widgets/gimppopupbutton.c:555–582`: button press creates view, passes it to `gimp_popup_new`, and calls `gimp_popup_show_over_widget` anchored to explicit parent or invoking button.
6. Preset/custom brush selector follows the same popup carrier through `app/tools/gimpmypaintbrushoptions-gui.cpp:325–351`; content notebook creation at `253–300`.
7. Each nonconstant input-mapping `...` control uses the same helper at `app/tools/gimpmypaintbrusheditor.cpp:758–763`; CurveViewCreator::create_view is at `571–687`.

Do not confuse this tools implementation with the separate dock/data-editor constructor in `app/widgets/gimpmypaintbrusheditor.cpp`. A test harness that wraps a widget in GTK_WINDOW_TOPLEVEL does not establish production popup parity.

## Popup carrier behavior

All following lines are `app/widgets/gimppopupbutton.c`:

- `59`: GimpPopup subclasses GtkWindow.
- `287–290`: constructor sets **GTK_WINDOW_POPUP**, nonresizable. Not a decorated normal GTK_WINDOW_TOPLEVEL, and not a GtkPopover child.
- `127–130`: GTK_SHADOW_OUT frame contains view.
- `404–411`: add/show view and attempt to focus it.
- `166–184`: map invokes parent map and gtk_grab_add. No explicit GDK pointer/keyboard/seat grab in this implementation; owner_events comment is not proof of one.
- `143–160`: ignore child grabs; cancel-on-grab-change is commented out. Do not infer dismissal merely on focus loss or grab loss.
- `103–116`: Escape binds cancel. Return/KPEnter/Space confirmation bindings are intentionally commented out.
- `231–250`: popup bindings are activated before the focused child can consume them; otherwise parent key handling.
- `188–227`: outside bounds or another widget toplevel emits cancel; handler returns FALSE even after cancel. This matters for event propagation.
- `254–261`: cancel removes own current GTK grab and destroys popup.
- `265–278`: confirm invokes close; close also removes own grab and destroys.
- `303–357`: anchor rectangle position, monitor selected at target left/top, horizontal edge alignment/flip and vertical above/below flip. It does not universally clamp oversized popup content into the monitor.
- `361–388`: anchor widget origin plus allocation for no-window widgets, default bottom-left alignment.

No modal/transient setter appears in this carrier. GTK's popup window mode supplies popup semantics; it should not be replaced with ordinary framed transient TOPLEVEL semantics.

## Editing and dismissal

Cancel is presentation dismissal, **not transactional rollback**. Mapping edits immediately call gimp_mypaint_options_set_mapping_point at `app/tools/gimpmypaintbrusheditor.cpp:425,427,503`. Explicit resource save remains in `app/tools/gimpmypaintoptions-gui.cpp:177–195`. Dismissal should not silently save a resource either. Distinguish current draft persistence from disk save.

## Existing 3.x carrier comparison

The combined source has `app/widgets/popupper.cpp` GimpPopover: despite its name, this subclasses GtkWindow and sets GTK_WINDOW_POPUP. It already uses a nonresizable shadow-frame carrier, GTK grab, Escape only, and destroy-only cancel/confirm. Prefer reuse over parallel UI implementations. Its outside handler currently returns `outside` rather than legacy FALSE, a semantic divergence to assess explicitly. Its current source has no legacy show/show_over_widget positioning helper. Upstream `gimppopup.c` is not an exact substitute: it binds Return/Space and actively seat-grabs/cancels on grab loss.

## Scope boundaries

This proves MyPaint toolbar/detail/mapping popup presentation. It does not establish Colorify-specific legacy presentation; that route needs separate source trace. No generic dialog, saved user layout, publication plan, repository source, or remote ref was changed during this investigation.

## Additional legacy carrier distinction and Colorify trace

The same legacy commit also contains `app/widgets/popupper.cpp`: a newer GLib::Popover carrier, independently constructing GTK_WINDOW_POPUP at391–406. Its outside-button handler returns `cancel` at222. Therefore current popupper.cpp's TRUE-on-outside behavior matches this second legacy carrier; the mismatch is specifically against gimppopupbutton.c, and must not be globally 'fixed' without selecting the applicable route.

Layer-tree popup route:
- `app/widgets/gimplayertreeview.c:351–361` adds gimp_cell_renderer_popup_new dropdown.
- `app/widgets/gimpcellrendererpopup.cpp:161–165` decorates it with LayerPopupWindow::create_view; activate298–321 emits parent-clicked.
- `app/widgets/popupper.cpp:478–505` creates content, constructs PopoverInterface popup, connects cancel/confirm and shows over widget/cell area.
- `app/widgets/gimplayerpopup.cpp:1148–1153` creates PopupWindowDecorator content.
- That content includes filter-layer configuration at1020–1133: procedure list, option pane, and explicit Apply1094–1099 calling filter_layer->set_procedure.

Colorify has two distinct legacy routes:
1. Ordinary plug-in menu invocation: `plug-ins/common/colorify.c:241–255` uses gimp_dialog_new("Colorify",...), a dialog. This route is not evidence for changing every Colorify dialog into popup.
2. Filter-layer dropdown: Colorify registers procedure `plug-in-colorify` (same file29), image/drawable parameters96–97, and `<Image>/Colors/Modify` menu path114. `gimplayerpopup.cpp:120–124` allows `<Image>/Colors/`; its generic query and image/drawable filtering157–181 therefore admit registered Colorify into the popup's filter list. This is a source-derived inclusion inference, not a captured legacy runtime. Selection builds generic procedure parameter controls and explicit Apply sets the filter layer procedure. Its existing filter-layer popup is not the same UI as the ordinary Colorify plug-in dialog.

Consequently the parity target must name the invocation route. MyPaint toolbar popup is established; layer-filter popup is established; replacing unrelated generic native dialogs is not established.
