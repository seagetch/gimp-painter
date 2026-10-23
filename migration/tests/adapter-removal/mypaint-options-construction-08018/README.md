# Original MyPaint options type and constructor entries

Bounded child of 08.018, whose original criterion is `C 入口から旧 Interface::cast を排除`.
The inventory requires `TypeTraits<GimpMypaintOptions>::type` to delegate to the
original C GType entry, and `MypaintOptionsRef::create` to delegate to the sole
original constructor entry. Legacy pin: afa43fae3e920210146abed514f136fd49f671b5,
app/paint/gimpmypaintoptions.cpp:95 and :334–343. The original constructor accepts
GimpToolInfo*, creates independent owned options using tool_info->gimp and exact
name `Mypaint`, and does **not** assign the `tool-info` property.

The two symbols now forward to the sole native options GType/model. There is no
new GType, store, class singleton, backend, Interface cast or canonical-options
replacement. Original object/class runtime names, old class layout and binary
ABI are not claimed: GimpMypaintOptions aliases GimpPainterMybrushOptions. The
native model initializes its valid draft/standard resource at construction;
this does not reproduce the historical initially-null options->brush field.

The constructor returns transfer-full nonfloating independently editable options.
It captures weak endpoint witnesses before acquiring references, validates the
application and existing binding state, and publishes only an active native
options binding. NULL/wrong ToolInfo is a GLib precondition failure; detached,
closed or incompletely initialized application and contained native construction
failure return NULL. No tool-info assignment is added. The typed create method
adopts the result and rejects NULL without inventing a second owner/model.

Final proof uses an actual C consumer, C++ owning handle and nine subprocess
cases: original signatures/type/class macros; independent ownership/name/Gimp,
no tool-info and draft independence; typed destruction; detached app; closed app;
missing config; NULL input; outliving ToolInfo; injected native initialization
allocation failure with exactly-once final release and canonical options intact.
Selected sanitizer instrumentation is four translation units (options adapter,
binding store, C/C++ fixtures), not the whole application. LSan/vptr are disabled
consistently with the existing no-RTTI ABI. Two compiled negative controls change
the required original name and publish failed initialization, respectively.

Execution uses authenticated Xvfb 1080x1920x24; the model fixture explicitly sets
and reads back painter-canvas-ui TRUE. This is a model API proof, **not rendered
UI evidence**. The eight production providers for the four paused paths match
81bc24545d before compilation. Their candidate changes/tests are excluded. The
archive/manifest seal actual selected inputs, not a whole-HEAD clean checkout;
existing Filter native Undo/fixture overlays remain ancillary, not exercised by
this fixture. No publication or old evidence resealing is performed.

## Remaining scope (not waived)

Original gui/gui_horizontal/is_ready entries stay TODO. Legacy readiness was
`options->brush != NULL`: a null change was guarded and retained the old brush.
Native select_source(NULL) clears selection but retains a valid unsaved Resource
and resets its draft. Neither an active-binding nor selected-pointer alias can
silently claim this lifecycle equivalence. Legacy GUI used a Basic/Details
notebook and horizontal basic controls; native editor/toolbar workflow mapping
and actual portrait UI verification remain separate. Parent 08.018 and the other
original option operations remain open.

Reproduce normal fixture through the existing Meson target
`mypaint-options-construction`; caller holds the shared build lock. Run
`migration/tests/build_options_construction_sanitizers.py BUILD --report FILE`
for selected instrumentation, optionally `--mutant wrong-name|publish-failed`.
Evidence validator: `python3 verify.py --check-owned-source`.
