# 38.004 initial implementation review (not acceptance)

`initial-111-review.tsv` preserves all 71 Binder + 40 direct callback sites once,
with candidate modern source/callback, disposition and outstanding verification.
`tools/check_gimp3_vfunc_coverage.py` passes the original seven inventory tables.
The new review is deliberately NOT a status update: replacement routes and
inherited slots need further proof; the static inventory's DONE means signature
research, not runtime exception acceptance. Inspection source hashes are snapshots
only, and may change while other assigned workers finish their changes.

## Immediate bounded production gap

`app/widgets/gimppainterlayertiles.cpp:1117` instance initialization catches only
`std::exception`, activates its BindingStore before `Tiles::build()`, and neither
closes partial state nor records a failure the public factory rejects. A thrown
unknown exception can leave the C callback; a standard allocation/build exception
can produce a seemingly successful, partially active widget. Fix must:

1. Put all throwing instance setup under the common catch-all boundary.
2. On failure mark an explicit failed state, close partially created store and
   owned sources/connections without throwing, and keep native object disposal
   valid. Factory returns NULL and releases the failed instance. Direct
   `g_object_new` may still return an inert object; operations must reject it.
3. Add catch-all diagnostic to `dispatch()` (currently noexcept with only
   std::exception catch). Its command string copy is a concrete allocation seam;
   unknown-exception test must distinguish structural containment from a claim
   that std::string naturally throws an integer. Do not inject a C++ exception
   through a GTK C library callback to simulate this.

Do not edit this source until parent seals 07.008 fresh receipts. A source change
requires fresh relevant normal/sanitizer receipts, not reuse of old hashes.

## C caller / real native callback failure test design

Extend or create a dedicated executable based on the existing
`test-painter-widget-exceptions.c` plus test-only C++ fault wrapper. Existing
`--wrap BindingStore::check_thread()` seam supports callback bodies; add a
separately armed `--wrap BindingStore::ensure(GObject*)` seam for instance init.
Use the actual object type to filter the fault, one-shot disarm before throw, and
record entry count. Link actual production object files; no normal build hooks.

The C translation unit invokes `gimp_layer_tile_view_new()` or `g_object_new()`;
the wrapper injects typed Painter Error, std::runtime_error, std::bad_alloc, and
nonstandard integer *inside* the real initializer. Preserve causal baseline:
standard kind currently returns a non-NULL failed factory result (assertion
failure); unknown kind must be isolated in a subprocess (uncaught termination).
Candidate must yield factory NULL, one consumed fault, no active store, weak
pointer finalization, and subsequent successful construction/use/destruction.
For a direct raw GObject construction verify inert state and safe dispose rather
than pretending GObject construction itself can return NULL.

A second post-activation/pre-build seam is necessary to establish rollback of
partial UI/sources; ensure-only injection proves early setup, not partial-build
cleanup. A test-only instrumented production compilation may expose that seam,
but its exact diff and object identity must be reported. No exception may be
injected into a noexcept destructor/GLib C allocation API and then advertised as
recoverable containment. Cleanup must itself be nonthrowing.

## Remaining groups and acceptance matrix

- Clone/Filter34: native class/iface invocation for constructed, duplicate and
  all eight progress slots; prove inherited memory/size/transform slots by pointer
  identity. Clone empty transforms and scalar opacity/content-lock callbacks have
  no throwing operations; don't add fake catch blocks. Removed project_region
  maps to GEGL publication, not a fabricated vfunc.
- Tool Binder18: fill paint + perspective9 + old image-generator8. Perspective
  with() boundaries need failure-state/cancel and DrawPause checks. Image-generator
  GType is absent; profile migration test is not itself approval to retire its
  eight callbacks. Resolve intended replacement/retirement explicitly.
- Core Binder6: Clone Undo pop, canvas draw/extents, JSON save/extension/duplicate.
  LayerPreset is native C; determine all former JSON-derived users before claiming
  this replaces every old JsonResource obligation.
- External Binder13: Clone Undo constructed, guide removed closure, cell renderer4,
  layer tile constructed, tool tile constructed, old popover5. Renderer render/size
  inherit native C slots. GtkPopover native event handling and NULL removed-class
  closure need identity/behavior proof, not synthetic exceptions into GTK.
- Brush15, MyPaint tool10, remaining15: coordinate assigned corebrush/editor/action
  workers. Map absent legacy custom brush slots to session/resource routes;
  options/finalize to modern disposal; old MyPaint Undo to native PaintCore Undo.
  Generic property-helper tests are supporting evidence only.

For each actual throwing C++ callback record exact public/native C caller,
return/output fallback, GError behavior when applicable (NULL output too),
pre/post state, one RAII cleanup, owner refcount, and retry. Four exception kinds,
normal and bounded ASan/UBSan; destructive faults isolated. Class initialization
is separate from instance initialization: no retry of partially registered GType;
any fallible C++ setup must precede callback publication or record terminal failed
state. C-only/native inherited/empty callbacks require explicit proof rather than
unexecuted throw-injection labels. All111 rows must end with an evidenced
ported/inherited/replaced disposition, never bulk-promoted by a shared test.

## Executed causal-red baseline

`initializer-causal-red/report.json` records five real C-driver subprocesses linked
with ordinary production archives. Typed/standard/allocation each returned a
non-NULL failed object and hit the expected NULL assertion. Unknown through the
public factory returned NULL only at the outer catch after crossing GTK C frames;
raw `g_object_new` unknown terminated on uncaught integer. Therefore an outer
factory result alone cannot certify inner callback containment. No production
change or post-activation rollback/sanitizer acceptance is claimed here.
