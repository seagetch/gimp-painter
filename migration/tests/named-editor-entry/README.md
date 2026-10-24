# Named native Painter brush editor entry (bounded, partial old parity)

This restores the exact C name and signature
`gimp_mypaint_brush_editor_new(GimpContext *, GimpMenuFactory *)` and
`MypaintBrushEditorRef::create`, delegating to the one native
`GimpPainterMybrushEditor` and canonical shared Options model. It does **not**
restore the old GimpDataEditor subclass layout/type or close original section 24.

A supplied menu factory is used, not discarded. It must register
`<MypaintBrushEditor>`; the native application registers this menu and the actual
Save / Save As action group. NULL creates an editor without a menu; wrong context,
wrong factory, or an unregistered factory returns NULL. The C result is floating;
the typed owner sinks it. The registered native dock uses this named entry.
The compatible header does not promise the old C struct or GType alias.

Save and Save As call the existing native save/prompt paths, preserving one shared
editable draft, collision checks and resource semantics. The menu XML is parsed
by the real UI manager and its two action references are checked. The named menu
owns its cache entry through the existing single editor slot: context replacement
keeps the menu, slot close clears this named group’s borrowed user_data and
removes its manager/group exactly once. Action signal
connections are object-bound, so an externally retained action cannot invoke a
freed editor. No second model or external ownership registry is introduced.

## What is explicitly not restored

The old base DataEditor `edit-active` flag gates context-driven base data/name/
read-only-title refresh and serializes `edit-active` / `current-data` auxiliary
session fields. However, the old MyPaint override ignores the supplied data and
re-reads the active context brush; its controls remain context-bound. The pinned
old constructor also contains the wrong Options subtype check and a NULL Options
variable, which original task 24.015 explicitly asks to correct. Therefore this
proof does not claim verified old runtime pinning, arbitrary current-data restore,
old read-only information-view refresh, or edit-active session parity. These
remain open. No metadata-only checkbox is presented and the editable native draft
name is not frozen or redirected away from the shared model.

## Evidence boundary

The private native link uses the already built, sealed predecessor from
`nonincremental-display-21013` (commit 40a1001ff1), including exact committed
Options 3f831a8a862dabcc60f0049c154f93655d8004e2. Only named editor/menu/action/dialog
objects and the isolated fixture are recompiled. It is **not** a current-HEAD or
whole-application rebuild and does not claim the later readiness/public-enum
provider integration. All linked archive/object inputs are hashed before and
after; held provider exclusions are checked against immutable predecessor seals.
Normal and scoped ASan/UBSan use ordinary API calls only. No paused acquisition,
tree-popup, Canvas timeout, preview hook, Bucket or Clone diagnostic is run.
The sanitizer instruments seven selected translation units, not all dependencies;
leak detection is disabled because the native GUI owns process-lifetime caches.

The portrait screenshot is the actual authenticated Xvfb 1080x1920 desktop with
oncanvas controls enabled and asserted visible. It shows API-activated native
menu Save As, not a physical-pointer interaction claim. The initial capture where
the main window obscured the editor is retained as a rejected fixture capture,
not presented as menu evidence.
