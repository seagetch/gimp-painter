# MyPaint legacy popup presentation — task 298

Bounded leaf `24.015/legacy-popup-presentation`, based on published task297. This corrects the actual oncanvas MyPaint compact button and its Popup Editor action, not a test-only GtkWindow wrapper. One editor implementation and shared draft model remain authoritative.

## Legacy and current contract

The verified `seagetch/gimp-painter` `gimp-2-8` reference is `afa43fae3e920210146abed514f136fd49f671b5`. Its MyPaint toolbar uses GimpPopupButton→GimpPopup with GTK_WINDOW_POPUP, shadow frame, GTK grab, Escape/outside cancellation, and button-relative monitor-edge flipping. Return/Space global confirmation is disabled; cancel/confirm close the popup without reverting live changes. Old runtime was not executed. Exact source locations are in legacy-source-contract.md.

The new GimpPainterPopup derives from the existing guarded GimpPopover2 carrier, retaining that carrier's default behavior for all other users. The MyPaint-specific outside handler returns FALSE like the old GimpPopup. GTK event dispatch can itself return TRUE after widget destruction; both handler return and event-dispatched dismissal are tested. No new device grab or unconditional focus-loss cancellation is introduced.

Only the MyPaint compact group switches from GtkPopover to a fresh native popup. Its borrowed canonical hierarchy is detached before host destruction and reused on reopen. Anchor unmap/destroy and parent closure clean up hosts; early construction watchers reject retired anchors. Standard compact groups retain GtkMenuButton/GtkPopover. A native-only minimum width fixes the visible 64px allocation regression found by screenshot inspection; actual width>=340 is asserted. Naming prompts are modal only when hosted by the grabbed native popup, preserving reviewed Save As/Duplicate behavior and restoring the parent grab.

## Verified gates

- Baseline RED: actual default-oncanvas compact→Popup Editor returned GTK_WINDOW_TOPLEVEL instead of GTK_WINDOW_POPUP.
- Final native editor43: new popup5 plus unchanged original25, Duplicate4, named-entry3, page-memory3, control-lifetime3.
- Final compact62: original control identity/order/parent invariants retained; MyPaint actual entry, width, four cancel/confirm reopens, same store/page/draft, anchor hide, popdown and full restoration.
- Existing native-popover14: original carrier contracts preserved. Total119 distinct native cases; these are focused registrations, not the entire repository suite.
- Scoped8TU ASan+UBSan popup5 passes, excluding vptr and with LSan disabled. Application remainder/dependencies are uninstrumented. The prior full-vptr baseline gate remains unresolved.
- Nested Cancel/Save use pointer-device enter/press/release events routed through GTK, not direct clicked/response emission. This is not a claim of physical X pointer testing.

New popup registration: `painter-editor-popup-parity`, using official `sessionrc-native-oncanvas` with product default TRUE and56 shipped file-backed standard brushes plus the177-resource MyPaint corpus. Existing38 editor cases retain their original fixture. Builds used the changed source objects linked against immutable integrated297 archives; exact binary/source hashes are recorded. Full clean application/dependency rebuild is not claimed.

## Genuine portrait images

`compact-popup.png` and `editor-popup.png` are raw1080×1920 Xvfb captures, with actual Openbox3.6.1 Debian3.6.1-12+b2 decorations on the main window. Native popup windows have X override_redirect=yes and no WM frame extents, unlike the framed main window. No preload, image editing or synthetic chrome. Only the main window is resized for portrait evidence; popup position comes from production anchoring. The external utility-minimizer is confined to the isolated capture session; the official fixture has no floating dock windows. Real shipped brush thumbnails are visible.

The nested editor retains the existing860×630 content layout (including its scrolling/preview limitations). These images prove native popup presentation and real entrypoints, not complete legacy control-layout/visual equivalence. All original parents/full-port gates stay open. The separate layer-filter popup route and ordinary Colors-menu Colorify dialog are not changed here; their legacy contracts differ.

For a normal configured build, run the registered popup, editor, compact and native-popover tests through Meson. Local source-and-image hashes are provided for replay validation.
