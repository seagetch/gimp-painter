# Painter canvas and tile UI

Source reference: seagetch/gimp-painter `afa43fae3e920210146abed514f136fd49f671b5`,
`app/display/gimpdisplayshell-overlays.cpp`, `app/widgets/gimplayertileview.cpp`,
`gimptooltileview.cpp`, `gimptooloptionstoolbar.c`, and the WBS29 hunk inventory.

## Ownership and GTK3 boundary

The new layer tile widget and display-shell controller each register one typed
slot in the common `BindingStore`. They use its existing ObjectRef, WeakRef,
Connection and Source types; no parallel decorator/qdata ownership is introduced.
Signals contain a weak owner and, when needed, a strong referenced layer/preset.
The callback first takes local owner/target leases. All sources are cancelled
on image replacement or widget/shell destroy; their callbacks validate the store
generation and return one-shot completion without deleting their own captures.

A tile uses upstream `GimpView`/`GimpViewRenderer` for its layer and mask preview.
No old TempBuf/Cairo surface capture or custom preview idle is retained. GTK3
rendering, current display scale and the renderer's viewable invalidation/idle
teardown are shared with the standard dock. Tile model rebuild is separately
coalesced on a weak, generation-checked owner source. Image refs and per-image
signals are replaced together.

A layer view has its own context image, with ordinary brush/tool/color properties
inherited from the user's context. Switching global active image must not point
one canvas's tiles at another canvas. Multi-selection uses the GIMP3 image's
selected-layer list. Reordering uses the existing item tree and Undo APIs, so
Clone/Filter dependency observers see standard structural notifications.

Layer popovers use GTK3 GtkPopover dismissal/focus behavior. Existing layer
commands open canonical Clone/Filter editors. Filter argument storage remains in
those already ported editors; this UI never shallow-copies a PDB GValue array.
Preset buttons use the canonical preset application API and its transaction
owner, not the old GObject delete of a C++ applier. Menus are new owned widgets;
no UI-manager-owned menu is unrefed as an owned return.

## Viewport behavior

The View → Painter Canvas Controls action opts into the new workflow; existing
standard display defaults remain unchanged. Panel positions use GTK logical
canvas coordinates, never image offsets or device/buffer pixels. Image pan,
zoom, rotation and flip do not move panel placement. Portrait allocation
repositions the same widgets rather than removing controls.

Normal opacity is .85 (color editor 1.0). Enter restores full opacity; leave
restores the configured panel value. During button/pen drawing, a panel within
200 logical pixels retires to .1. Hit testing is controlled independently through
an explicit overlay-child input-pass-through flag. Release, proximity loss,
broken grab, focus loss and popup dismissal restore opacity and hit testing.
Opacity changes alone do not make a control click-through.

Borrowed dock widgets receive one balanced strong reference while reparented;
returning them must restore their previous parent/position without adding the
legacy hide-time excess reference. The MyPaint popup uses the one canonical
model/editor shared with the dock and brush history.

## Verification scope

`test-painter-canvas-ui.c` and `run_canvas_ui.sh` exercise the actual GTK widgets
and callback paths on a native display. `build_canvas_ui_sanitizers.py` builds
private instrumented archives, including every live BindingStore registration
unit for consistent RTTI. It records exact source hashes and rejects source
changes during a build. A focused sanitizer pass is not a claim that all GIMP
or third-party code is instrumented, and leak detection is separately disclosed.

Completion of standard-tool horizontal compact control parity requires the
per-control inventory comparison; access to full standard Tool Options by itself
does not prove the old horizontal/compact presentation. Full WBS29 closure also
requires the native pointer/pen, multiple-image, popup, dock, HiDPI and lifetime
cases to pass against the final source snapshot.

## Canvas checkpoint coverage

The native harness now has 19 cases: exact RGBA/Gray-alpha channel-preview pixels; selection/visibility and Undo; group moves
and cycle rejection; image replacement and unrelated global-image changes;
long-press cancellation, one-shot activation and grabbed-widget hide/destroy;
preview-idle teardown; repeated popup close/reopen and owner destruction;
canonical Clone creation; mode/opacity/lock/mask editing; reentrant owner close;
multiple-layer single-Undo movement; independent overlay hit testing and opacity;
dock parent destruction and exact GtkBox packing restoration; tool/context and
FG/BG synchronization; tool-group active/expanded state; two simultaneous
canvases with independent images and dock transfer; and late callbacks after
shell destruction.

Native pointer demonstrations used an explicitly synthetic unsaved image. They
verified plain selection with modifier state 0, dragging Ink into Group and
reversing it through the on-canvas Undo button, opening the real layer popup and
creating a Clone Layer, reparenting/restoring the canonical Tool Options GUI,
and painting visible black and red strokes after changing the native wheel.
Layer previews updated from those real strokes. A portrait window retained the
same controls with scrolled overflow. The original open GIMP image was preserved.
The reproduction entry point is `run_canvas_ui.sh build-debian13 demo`; its
Finish Demo button exits early and its four-minute limit bounds profile ownership.

The native demonstration caught and corrected two layout defects: recursive
`show_all` had revealed the inherited editor name label, and GtkNotebook's
tallest page had centered the wheel below the compact viewport. Names are hidden
after panel showing; native selector pages are top-aligned, and the selected
radio button uses the editor's native tab-change path. Full channel editors,
color-management labels, picking and color history remain in the same scroller.

The final evidence manifest states the exact instrumentation and runtime scope.
Leak detection is disabled for the focused GTK run; physical tablet hardware was
not available. The harness reports the pre-existing synthetic-profile writable
resource-path warning on shutdown, separately from its test result. Full WBS29
is deliberately not claimed here: horizontal compact standard-tool controls and
the remaining legacy toolbar inventory require the next implementation slice.

### Native channel-preview defect exposed by instrumentation

The preserved `canvas-ui-alpha-preview-failure.log` is a UBSan bounds failure
in the upstream `gimp_view_render_temp_buf_to_surface`, exposed while a real
channel dock rendered during image replacement. Its three-byte RGB conversion
was read with source alpha index 3. Gray-alpha source index 1 also selected green
instead of alpha, and Cairo ARGB preview pixels lacked the opaque alpha byte.
The narrow renderer correction retains four RGBA bytes, remaps the source alpha
component, bounds-checks the requested source component, and writes opaque
grayscale preview pixels. Indexed-image preview alpha is derived from its
expanded preview format rather than index/alpha storage. The regression asserts
two exact color and alpha preview pixels for both RGBA and Gray-alpha formats.
No sanitizer category was disabled to pass this regression.

### Reproduction and retained results

On the configured Debian 13 dependency environment, take the shared lock for
`ninja -C build-debian13 app/tests/painter-canvas-ui app/gimp-3.0`, release it,
then take it separately for
`python3 migration/tests/build_canvas_ui_sanitizers.py build-debian13 --report build-debian13/canvas-ui-sanitizer-build.json`.
The runner takes the same shared lock itself; invoke each native phase with
`bash migration/tests/run_canvas_ui.sh build-debian13 normal` or `asan`.
The sanitizer runner validates every recorded source/header and the executable
hash before running. A separate `GDK_SCALE=2` normal invocation exercises the
same native cases at scale two. This is GTK scale coverage, not a claim of
physical tablet or multiple-monitor hardware coverage.

Final results on 2026-10-02: native 19/19, focused ASan/UBSan 19/19, and native
GTK scale-two 19/19, each exit 0. There are 52 instrumented implementation/test
translation units; remaining GIMP and third-party code is not instrumented.
The sanitizer list includes address, undefined (including vptr) and
float-cast-overflow; leak detection is disabled. Source hashes matched both
before execution and after the final run. `canvas-ui-checkpoint.json` records
the exact source archive, logs, binary hashes and staging allowlist.
