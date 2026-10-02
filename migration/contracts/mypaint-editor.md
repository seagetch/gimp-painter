# Shared Painter MyPaint editor

The editor is a distinct `GimpEditor`/`GimpDocked` widget. Its C API accepts a
real `GimpContext` and obtains an owned model using
`gimp_painter_mybrush_options_ref_for_context()`. Arbitrary contexts are never
cast to Options. The tool-options panel, dock dialog, horizontal presentation
and popup all use the same widget and canonical model. Ordinary contexts follow
later resource changes bidirectionally. NULL/wrong/disposed construction fails;
dock detachment stops callbacks and previews and can be followed by reattachment.

## Editing and resources

All 45 numeric settings, five switches, two resource-reference texts and nine
mapping inputs are available. Curves retain zero or two through eight points,
including equal-x points, with input enable/disable, graph add/drag/right-delete,
exact numeric coordinate editing and the old whole-curve input/offset range
rescaling operation. Range rescaling is an explicit atomic action; Fit View only
adjusts display bounds. Empty point editors display zero and are disabled. Edits pass through the lossless Options resource, preserving
unknown JSON and unresolved resource names. Unsupported engine settings remain
inspectable/saveable and give a preview diagnostic rather than using a stale
configuration. Brush shape and paper selectors use native GIMP resources.

The list searches names, legacy groups and native tags case-insensitively.
Native tags can be edited and are retained in Save As copies. Application-owned
history restores whole drafts, curves, unknown fields and resource references;
independent Options and all presentations share that history.

Save uses the native data factory and refuses read-only sources or conflicts.
Save As and Duplicate create a new complete resource and do not replace a
colliding name. A failed filesystem save retains the draft even after its
separate memory commit. Rename preserves the native resource and save path.
Delete confirms its captured target and removes the disk file before factory
membership so an I/O failure cannot discard selection/draft. Dialog callbacks
are weak and generation-checked; changed selection/context never operates on a
newer unintended target. Separate Painter folder preferences and first-profile
installation directory are registered alongside standard MyPaint paths.

## Preview and lifecycle

The pinned preview's 256×256 red/white diagonal background, 257-sample spiral,
pressure/time sequence and copy-only mapping/startup order are retained. This
is deliberately separate from the active stroke's foreground setup. The real
extended Engine, `GeglSurface` and `GimpResources::Purpose::Preview` render it;
upstream libmypaint is not involved. Native resource isolation copies masks,
paper and pipe state and has its own seeded pipe-selection RNG.

Each widget has exactly one typed BindingStore slot and a named owned handle.
Signal payloads hold weak, generation-checked owners. UI replacement disconnects
old resources, and destroyed controls remain retained through synchronous
callbacks. Synchronization checks its generation after signal-emitting GTK
updates. Preview work runs in owner-context chunks of four input events; newer
requests clear visible stale pixels immediately and cancel the old source. Only
a complete, still-current buffer is published. This is a bounded sample count,
not a universal millisecond latency bound for arbitrarily expensive brush data.

## Independent evidence and scope

`legacy-mypaint-preview/` contains a genuine unchanged old
`GimpMypaintBrushPrivate::get_new_preview` capture: 16 synthetic combinations of
bitmap shape, paper, smudge and nonincremental painting, each repeated exactly.
All 16 complete 256×256 RGBA buffers match the actual new GTK widget output.
The 33-record/8,389,189-byte capture is independent of the previously sealed
129-record drawable-session oracle. A standalone reproduction using the sealed
link plan also matches byte-for-byte. Four offline fixture integrity tests
verify provenance, complete records, repeat equality and the comparator hashes.

Native editor cases exercise constructor and context routing, all generated
controls and input mappings, tagged list/history, default-directory creation
and file roundtrips, conflicts/read-only sources, extended previews, replacement
and close, registration/preferences, the old preview corpus, destruction during
numeric refresh, actual graph events, old-resource notification disconnection,
and failed save/delete retention. Run reports state exact completed counts,
source hashes and focused sanitizer scope. LeakSanitizer and a fully instrumented
GIMP/dependency stack are not claimed. Native GTK stimuli do not establish real
hardware tablets, Windows/macOS, all177 preset renderings, arbitrary ICC or
precision modes, or pixel equality of the old GUI layout.

The final checkpoint passes all 12 automated native GTK cases and all 12 under
37-source ASan/UBSan/float-cast-overflow (including C++ vptr checks), plus one
manual visual acceptance case. `mypaint-editor-runtime.json` and
`mypaint-editor-sanitizers.json` seal matching source/header and executable
hashes. The standalone initial-profile installer was not executed; the test
removes only its empty default Painter folder and verifies that native Save As
recreates it and saves/reloads successfully. The known test-profile shutdown
search/writable-path diagnostic is retained, not hidden.

On a real GTK display, source the configured build environment, then run
`bash migration/tests/run_mypaint_editor_ui.sh build-debian13 normal`. Use `asan`
after `build_mypaint_editor_sanitizers.py`, or `demo` for the final interactive
layout and Save As/Cancel inspection. The runner holds the shared build lock
before hashing or launching the binary.
