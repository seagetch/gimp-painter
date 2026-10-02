# Independent Painter Smudge tool and resumable owner-context controller

`gimp-painter-smudge-tool` registers `GimpPainterSmudgeTool`, with the dedicated
legacy core/options and independent `use-color-blending` dynamics. The standard
GIMP3 `gimp-smudge-tool` and its S shortcut remain unchanged. Provenance-aware
old Painter profile/tool/shortcut migration is a separate integration step;
registration alone does not claim the old user workflow is fully migrated.

The BrushTool base supplies cursor/outline/color-picker and standard paint
options UI, but actual painting never enters its threaded paint path. One typed
BindingStore controller owns a Source, weak display and ordered input envelopes.
Each stroke snapshots options, brush/dynamics and context values. Input is not
expanded into a dense point array or dropped: native interpolation is prepared
once and each owner-context step emits at most one native candidate. The same
native numerical iterator is used by synchronous compatibility calls and queued
calls, retaining old drawable-local arithmetic. No second bridge is introduced.

Press/release, consecutive strokes and settings edits retain input order. A
stroke uses one native Undo transaction; cancel, HALT, target invalidation,
owner/display loss and renderer errors rollback rather than commit partial
successful-looking output. Input accepted during cancellation callbacks belongs
to a new generation. COMMIT seals accepted input and its automatic HALT does not
discard the queued transaction. The image's pending-paint query covers both
queued tool input and an active owned core. Saving seals current input; a busy
synchronous save/export/PDB route refuses incomplete pixels. The separately
implemented GUI Save continuation can wait on this same image-level query.

Native PaintCore start admission is now one-shot and owned: public and direct
vfunc starts without the adapter permit fail before native state changes.
Generic Stroke Path/Boundary integration remains explicit unsupported work,
not a silently partial stroke. The numerical fixture formerly exercising raw
PaintCore calls now compares the synchronous owned API and separately stepped
owned API. The earlier raw-core checkpoint remains in history.

## Verification scope

The native suite exercises tool registration/options, ordinary Smudge shortcut
preservation, foreground picker, queued frozen settings, Undo/Redo, repeated
press, cancel before/after painting, target/image/format/lock changes, external
pixels, preview/projection, native Save/export/PDB guards and synchronous
reentry/lifetime cases. Large-input probes measure admission, cancellation and
whole main-loop iteration costs rather than inferring responsiveness solely from
a one-dab budget. Exact 48-scene old stroke fixtures remain the numerical gate.

Source manifests and run logs accompany this checkpoint. Focused sanitizer
builds include all exercised sibling option owners with consistent RTTI;
uninstrumented -fno-rtti bridge entries are not diagnosed as real UAF without
reproduction. Vptr checks remain enabled. The rest of GIMP and dependencies are
not fully instrumented and LSan is unavailable in this environment.

## Remaining boundaries

A single dab still performs synchronous brush transformation, accumulator
updates and publication; its vectors and the raw-input queue are not admitted
under a strict total-memory budget. Long-event yielding does not prove a bound
for every preparation/cleanup/observer. Pipe sequence cloning, paper texture,
high precision/indexed/symmetry, complete ICC/brush matrices, generic stroking,
old-profile identities and real hardware/platform acceptance remain explicit
follow-on gates. No arbitrary format/Flow substitution is used to pass them.
