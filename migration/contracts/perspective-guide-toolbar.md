# Perspective Guide horizontal factory

This bounded child of `29.010/standard-tool-options` restores the factory at
legacy pin `afa43fae3e920210146abed514f136fd49f671b5`,
`app/tools/gimpperspectiveguidetool.cpp:576–657`. Its active horizontal body
only calls `gimp_tool_options_gui_full(options, TRUE)`. The base constructor in
`app/tools/gimptooloptions-gui.c:48–65` returns an empty, nonhomogeneous
horizontal GtkBox with spacing 4. The rate and eraser controls are disabled
under `#if 0`; they are not part of this restored contract.

The GTK3 dispatcher recognizes only the Perspective Guide tool identifier,
creates a fresh floating horizontal box, and does not reuse the ordinary dock
factory or its cached widget. It does not accept arbitrary base tool options.
Guide gesture/model behavior remains outside this factory-only change.

## Unsupported tool classification

The remaining eight current tool IDs are deliberately outside this legacy
factory restoration, rather than all being labeled unported legacy factories:

- Brightness/contrast and GEGL exist in the pinned source but explicitly
  registered NULL ordinary/horizontal factories. Their filter dialogs are
  separate surfaces.
- Unified transform, handle transform, 3D transform, warp, offset and operation
  are absent from the pinned legacy tool tree. Adding independent modern
  controls would be a separate feature, with the full current model contract.

A factory count does not establish legacy control, behavior, popup, window or
session parity for the supported tools. Those broader parent tasks remain open.

## Verification

The two new compact-options cases cover independent floating views, both
destruction orders and recreation, the actual native Tool Options Dockable/editor
and its parentage, and Guide → populated Move → each unsupported tool → Guide
on a real canvas. They check panel visibility, exact child counts and old-view
weak finalization. The inventory rejects unclassified missing factories and
unexpected support for any of the eight exclusions.

Independent source/API review and whitespace validation pass. Native focused,
full compact-options, canvas/perspective regressions and HiDPI results remain
pending while the frozen aggregate owns the build lane. Visibility-state
assertions alone are not rendered-pixel evidence. The inventory uses the normal
test configuration with experimental playground tools disabled. No runtime
acceptance is claimed by this draft.

## Native validation

The [receipt](../tests/perspective-guide-toolbar.json) records 3 focused cases,
all 60 compact-options cases, 99 canvas cases, 22 Perspective UI cases and
9 Perspective event cases, plus both new cases at GDK_SCALE=2. All passed
with zero skips or unexpected diagnostics under the strict runner classifier.
Runtime inventory is 42 supported factories and 8 deliberate exclusions.
An installed actual-application 1080×1920 screenshot shows Perspective Guide
selected with its empty bottom bar; normal GUI Quit returns zero. Initial
tag-cache, host-font and session-bus notices remain visible. These results
do not cover sanitizers, other platforms, or full legacy UI semantics.
