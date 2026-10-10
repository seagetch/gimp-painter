# Registered extended MyPaint tool

## Ownership and dispatch

`GimpPainterMybrushTool` is a `GimpColorTool` subclass with one typed
`BindingStore` implementation slot and the named `PainterMybrushToolRef` handle.
Its only painting implementation owner is `GimpPainterSession`, which owns the
validated extended evaluator, GEGL Surface and native transaction controller.
It does not add an outer `GimpPaintTool` transaction. The standard GIMP 3 MyPaint
tool and its Y shortcut remain registered independently.

The new identifier is `gimp-painter-mypaint-tool`; the menu and default toolbox
contain Painter MyPaint. Its temporary options GUI exposes the native resource
selector and all 45 numeric, five switch and two text properties. The canonical
shared editor is a subsequent integration checkpoint. No replacement shortcut
is assigned: old `gimp-mypaint-tool` profile identifiers and its P shortcut still
need explicit migration without stealing the standard Paintbrush shortcut.

Full-motion tracking and exact motion mode receive inactive samples and
stationary/equal-timestamp pressure changes. Button/control state, rather than
pressure alone, selects painting. Inactive input advances the same evaluator and
shape-aware color sampling. Cold/settled hover cannot write pixels or make empty
Undo entries. Following a real press, positive-pressure interpolated release
dabs finish through the already active Surface; see `mypaint-active-pipe-release.md`. Both evaluator and ordinary brush selector
receive hover pressure zero, while tilt and the other copied device axes remain
available. Image-space press coordinates are retained intact, then only x/y are
translated to the selected drawable's local coordinates for the session.

Normal release supplies a zero-pressure inactive sample and preserves the old
evaluator's positive-pressure interpolation tail and logical split timing. It does not force an extra Undo group. Reentrant release
queues one inactive sample until the active sample unwinds. HALT/COMMIT finishes
the pending group; cancel rolls it back. Cancel wins when both requests occur
during one sample. A stop requested while native start is freezing previews
cancels the nascent transaction. Recursive painting/configuration remains
rejected. Closing or changing the target clears queued input, preventing a later
release from reviving an ended session.

The tool weakly observes display/image/drawable identities and owns temporary
leases while processing input. The controller separately retains both drawable
and image while a real transaction is pending. Native display replacement HALTs
the active tool before replacing its image, so that route commits according to
the old HALT semantics. Bare last-owner release without HALT cancels unfinished
pixels. The public button-press wrapper's whole-call lease is covered by the
separate `tool-button-press-lifetime.md` checkpoint as well as this native tool test.

Pending-paint preflight reports a currently executing sample before a Save
destination is opened. The image saving signal finishes a pending group after
input returns. This checkpoint validates that cooperation; asynchronous dialog
continuation is owned and tested separately. Projection flush and throttled
outline redraw use the actual display path. The constrain modifier uses the
native foreground color picker.

## Canonical options

`gimp_painter_mybrush_options_ref_for_context` returns an owned reference to the
registered canonical model. An ordinary context selects its Painter resource,
or the internal standard resource when none is selected; this can change the
canonical tool's selected brush. The existing model preserves drafts/history
while switching. An invalid, disposed or closed context/options object is
rejected. Nested selection cannot silently return a model for a different brush.
The shared dock/popup can therefore edit the tool's same draft. Histories of
arbitrary independently constructed or config-copied options still remain
per-instance; old singleton equivalence across such instances is an open gate.

## Explicit generic stroking gate

`GimpPainterPaintGate` supplies the native PaintInfo/options association. Direct
low-level PaintCore start remains explicitly rejected before a transaction is
allocated. The later [generic stroking implementation](mypaint-generic-stroking.md)
connects public coordinate, path and boundary operations to an owned extended
Session and its native paint transaction. The adapter has one common-store slot
and does not create a second renderer. Interactive input continues using the
tool's persistent Session. This distinction preserves the original tool suite's
direct-start refusal assertion without describing generic stroking as missing.

## Verification

The native GTK suite has 13 synthetic cases: registration and generic refusal;
default-pressure hover; stationary/equal-time pressure with real Undo/Redo;
cancel; ordinary/unselected/invalid/disposed editor contexts; actual perspective
delayed-start origin pressure and tilt; callback HALT/tool switch/last reference;
display image replacement; saving a pending group; busy Save preflight; outline
and projection; public press releasing the last owner during preview freeze;
and last-owner release without HALT rolling back pixels.

`mypaint-tool-headless.json` records eight options, eight session, six hover and
nine Surface cases, plus the independent old 32 warmed scenarios/96 full image
snapshots/129-record comparison. Those old scenes retain their specified
unprofiled nonlinear RGBA-u8 scope. The new lifecycle and GUI stimuli are native
GIMP tests with synthetic input, not old GUI captures or hardware tests.

`build_mypaint_tool_sanitizers.py` builds a private 33-source ASan/UBSan/
float-cast-overflow executable, including base Tool/DrawTool/ColorTool/Display,
the actual subclass, options/session, evaluator/Surface/resource adapters and
native transaction/lifetime paths. All instrumented C++ uses RTTI, including
UBSan vptr; LeakSanitizer is disabled. Unlisted GIMP/dependency sources are not
instrumented. No production object/archive is replaced. Build and run reports
record source and executable hashes. A separate 26-source hover run covers the
new ordinary-selector regression.

The pre-fix selector test failed because the selector observed pressure one
while the evaluator observed zero. The corrected path keeps tilt unchanged and
still produces zero pixels/Undo. The failed and successful evidence is kept
separately. The known writable-folder/search-path diagnostic on test-profile
shutdown is retained in raw logs, not suppressed.

## Remaining gates

Full editor curves/preview/file workflows; isolated deterministic brush-pipe
previews; old brush-pipe mode/sequence equivalence; other drawable channel and
precision formats; arbitrary ICC behavior; all 177 rendered brush scenes;
generic Stroke Path/PDB; old tool-profile/hotkey migration; and physical tablet,
Windows and macOS acceptance are not established by this checkpoint. Inactive
hover currently creates transient resource adapters, so full pipe sequence and
previous-coordinate behavior requires its own comparison. Sampling-only hover
is an intentional safety correction, separate from active-stroke pixel parity.

## Native Gray XCF workflow follow-on

The registered GTK suite now has14 groups. Its added group covers all four
Gray/Gray-alpha and incremental/nonincremental combinations through native
image conversion, registered public tool press/motion, actual `gimp-xcf-save`,
XCF reload, native-format byte comparison, Undo and Redo. Save finishes the
pending logical stroke once. Both normal14 and ASan/UBSan/vptr14 exit0, with
37 instrumented and24 RTTI-only sources plus15 checked headers; LSan is off.
Exact source snapshots and raw diagnostics accompany the workflow report.
The input remains synthetic Tool API events on a real GTK display, not a
physical tablet test. Built-in D65 Grayscale/sRGB TRC is exercised; arbitrary
ICC and high-precision work remain in `mypaint-remaining-renderer-handoff.md`.
