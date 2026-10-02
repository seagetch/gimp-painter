# Canonical compact tool options

Source baseline: `afa43fae3e920210146abed514f136fd49f671b5`, with
`a61915a8e62aad8855cd8c620bdb7195a25ffb90` separating Painter presentation changes
from ordinary GIMP 2.8 controls. This contract covers WBS 29.010,
29.010/standard-tool-options, 29.010/toolbar-ref-balance and compact 29.011.

## One live model and one native control tree

`gimppaintercompactoptions.cpp` adapts the canonical GUI returned by
`gimp_tools_get_tool_options_gui()`. It does not invoke tool GUI constructors a
second time: rectangle, transform, path and align options retain their native
widget handles, actions, modifier hints, sensitivity and resource connections.
The adapter owns one typed `BindingStore` slot on the root box; callbacks use
weak owner/target refs and generation checks through the common `Connection`.
Native control identity is read from GIMP's existing widget-search property
metadata, not a new qdata model.

The exact existing controls are moved into horizontal/portrait groups and GTK3
popovers. Turning compact off restores original parents, sibling order,
box packing, grid attachments, orientations, size requests and show-all policy.
Original widget visibility remains native-property-driven. Parent references
are weak, preventing a root → store → parent/root ownership cycle. Retained
children survive synchronous parent-change and destroy callbacks. Destroying
an active root also destroys controls temporarily borrowed into popovers.

Resource thumbnail selectors remain native inline selectors. The Brush popup
retains original name/edit controls, per-stroke angle/aspect/spacing/hardness
and link/reset controls; it lazily embeds the same `GimpBrushEditor` used by a
dock, including generated shape/radius/spikes/hardness/aspect/angle/spacing.
The Dynamics popup lazily embeds `GimpDynamicsEditor` for the native output
curves. There is no copied brush/dynamics state. Existing read-only-resource
rules still apply. The original shared Painter MyPaint editor stays intact;
its actual opacity, logarithmic radius, slow-tracking and hardness rows are
borrowed inline. All remaining settings, resource references, mappings,
history, preview and save actions remain in its popup.

## Pinned presentation correspondence

| Legacy source/family | Compact arrangement |
| --- | --- |
| `gimppaintoptions-gui.c` | Mode, opacity, brush thumbnail, size and reset/link inline; Brush geometry/editor, Dynamics/fade/gradient, Jitter, Texture and Smooth popovers; incremental/hard/erase flags and tool-specific rates remain actual native controls |
| `gimpairbrushtool.c`, `gimperasertool.c`, `gimpsmudgetool.c`, `gimpbucketfillbrushtool.cpp` | Motion/rate/flow, anti-erase, rate/color blending and Fill rate/eraser controls retain the native fields and callbacks |
| `gimpclonetool.c`, `gimpperspectiveclonetool.c` | Source/pattern/sample-merged/alignment in Options; perspective mode remains inline |
| `gimphealtool.c` | Sample-merged/alignment remain inline |
| `gimpconvolvetool.c`, `gimpdodgeburntool.c` | Convolve type/rate or Dodge/Burn type/range/exposure in respective detail popovers |
| `gimpinkoptions-gui.c` | Size/angle inline, Sensitivity and Ink Blob popovers retain native blob editor |
| `gimpblendoptions.c` → native `gimpgradientoptions.c` | Gradient resource/type/repeat/offset/dither plus Adaptive toggle/popover; current extra gradient controls retained |
| `gimpselectionoptions.c` | Operation icon ordering and modifier hints retained; antialias and native feather toggle/radius retained |
| `gimprectangleoptions.c`, `gimprectangleselectoptions.c`, `gimpcropoptions.c` | Rounded-corner toggle/radius popup; Selection details contains center/fixed rule, dimensions, position, highlight, guides and shrink actions |
| `gimpregionselectoptions.c`, `gimpiscissorsoptions.c` | Threshold, transparent/merged/criterion and interactive-boundary fields remain native inline controls |
| `gimpforegroundselectoptions.c` | Selection controls inline; full current foreground-engine controls in Details (upstream change described below) |
| `gimptransformoptions.c` | Transform target and direction inline; interpolation/clipping/preview/guides/constraints in Transformation details; current transform-specific controls retained |
| `gimpmoveoptions.c`, `gimpflipoptions.c`, `gimpcageoptions.c`, `gimphistogramoptions.c` | Native radio groups reoriented, with unchanged values and notifications |
| `gimpcoloroptions.c`, `gimpcolorpickeroptions.c` | Average toggle/radius popup; merged, pick mode and info-window controls retained |
| `gimptextoptions.c` | Font selector/size/editor flags and text color inline; hinting/justification/indent/spacing/box/language/current outline controls in Details |
| `gimpalignoptions.c`, `gimpvectoroptions.c`, `gimpmagnifyoptions.c`, `gimpmeasureoptions.c` | Native action buttons and model controls retained; no substitute actions or duplicated tool handlers |
| `gimpmypaintoptions-gui.cpp` | Four pinned Basic fields inline; the existing shared full Painter editor provides resource list/history/mappings/save/preview |

The old MyPaint reset-size callback is an empty body. The port does not invent
an effect for that button. The old selection wrapper incorrectly used the
antialias property around feather radius and omitted the feather switch; the
native antialias/feather controls are preserved rather than repeating this
omission. The old toolbar hide path took an extra reference on every hide
without returning it. The reversible adapter balances all borrowed references.
GTK3 layout is not a pixel-identical GTK2 screen reproduction.

## Standard upstream evolution is not Painter feature deletion

A source diff of `app/tools/gimpforegroundselectoptions.c` between the stated
2.8 base and Painter head changes only presentation wrappers and extracts
existing controls into Details. Property registration/set/get and foreground
algorithm semantics are unchanged by Painter. The old `contiguous`,
`background`, `smoothness`, `sensitivity-l`, `sensitivity-a`, `sensitivity-b`
are ordinary GIMP2 SIOX settings. GIMP3 uses matting draw/preview modes,
engine/levels/iterations and native stroke width/mask color. No unsupported
numeric equivalence or replacement SIOX engine is claimed by this UI port.
Legacy profile originals and their unknown forms remain byte-preserved in the
existing content-addressed migration archive; no old values are discarded by
the presentation adapter. Native GIMP3 controls are all retained, including
fields introduced after GIMP2. This is an explicit upstream-standard-engine
migration, distinct from the Painter-added horizontal presentation.

## Canvas and persistent visibility

Enabling Canvas UI opens compact options by default. Compact Options and Tool
Options switch between presentations; hiding, tool switching, dock transfer,
canvas closing and reopening restore the borrowed canonical control tree.
Portrait uses the same groups vertically. Popover close returns keyboard focus
to the canvas. Native overlay input/retreat and multiple-display ownership stay
with the Canvas implementation.

There was no old saved overlay visibility key: the pinned
`gimp_display_shell_update_on_canvas_views()` unconditionally created/showed
all overlay widgets for an image. The new native `painter-canvas-ui` boolean is
false for ordinary GIMP profiles. A proven legacy Painter profile materializes
true as a behavior-default migration; origin-ambiguous profiles are unchanged.
An explicit source value or existing destination file wins. View/Hide user
actions persist the preference; the per-shell C setter remains useful for
transient presentation and testing. New shells read the persisted value.

## Evidence status

Native registered inventory covers all 50 current tool GUIs, retaining the
2,868 original widgets and restoring parents/root order. This is control-tree
coverage, not proof of every underlying standard tool algorithm. Exact final
normal/native/sanitizer counts and hashes are recorded in the compact-options
checkpoint only after all runs finish. Focused sanitizers distinguish actual
instrumented units from the additional RTTI-only C++ compatibility closure;
GTK/shared dependencies and LeakSanitizer are outside that claim.
