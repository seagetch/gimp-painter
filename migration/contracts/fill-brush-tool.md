# Fill Brush native owner-context integration

The registered `gimp-bucket-fill-brush-tool` uses a native `GimpBrushTool`
subclass and the legacy `]` key (`bracketright` in GTK3). Its preview core is
separate from stroke cores, so native hover/outline updates cannot mutate the
queued paint transaction. The foreground modifier picker and native brush,
dynamics, opacity, mode and smoothing options are reused. Legacy `rate` and
`eraser-mode` remain serialized options; the independently executed legacy
oracle establishes that changing rate did not change the old output.

## Ownership and event ordering

One typed controller slot in the common BindingStore owns the input queue,
common Connection objects, and a common Source. No alternate GObject/C++ bridge,
worker paint queue, nested main-loop drain, or finish wait is introduced.

Press captures an input envelope with full device coordinates, target/image
references, copied context/options, and independent brush/dynamics resources.
Motion appends ordered input; release only seals the envelope. Repeated presses
and new strokes while earlier work is pending preserve distinct envelopes.
Admission waits for the preceding envelope's native transaction to finish, so
its projection snapshot includes earlier committed painting. Selection, target,
display and image changes invalidate incompatible pending work explicitly.

A Source invocation performs one begin, raw-event admission, one resumable
native interpolation candidate, search step, or native finish phase. The shared
continuation and exact pre-refactor oracles are documented in
`native-brush-interpolation.md`. Search steps have a candidate budget of 4096. Completed
publication and cancellation request asynchronous image/display refresh. Native
COMMIT seals active input and lets the queued transaction finish; the automatic
HALT immediately following COMMIT does not discard it. A later explicit HALT or
owner close invalidates the queue and stops its source. Connection detachment,
revision checks, native-core leases and Source rearming protect reentrant close,
rollback, and replacement-input callbacks.

Each completed stroke has one native Undo operation. Cancellation restores the
original pixels when the native raster/geometry remain valid. A core-detected
external raster replacement or unannounced buffer write must not be overwritten
with stale Undo data. That safety fallback is distinct from proving atomic
rollback across arbitrary third-party writes; its effect on partially applied
unfinished painting is a remaining compatibility gate.

## Saving while paint is pending

The image's `query-pending-paint` signal is a pure read with OR accumulation and
an explicit emitter lifetime lease. The tool answers for unadmitted envelopes;
the core separately answers for active/deferred-cleanup native transactions.
The generic Save/Save As/export entry, generic PDB file-save entry, direct
file-exporter process entry, direct XCF
entry/stream, initial writer and final XCF replacement checkpoint refuse with a
BUSY error while painting is pending. Refusal leaves the destination and queued
painting intact. The user can retry after painting completes. A save-progress
callback that starts painting is also checked before committing replacement.

This is an interim safety behavior. Automatic save continuation, with ownership
of the destination, progress UI and cancellation, is not implemented. It must
not be described as full asynchronous Save compatibility.

## Performance and unclosed gates

This integration yields between phases; that is not a total nonblocking or
bounded-memory guarantee. Press resource duplication, projection/snapshot
preparation, one native dab, GEGL I/O/publication, flushing and cleanup
can each exceed a frame budget. The raw-input queue has no arbitrary dropping
limit, while a long GUI motion now stays as numerical continuation rather than
materializing all dabs. Direct synchronous compatibility callers still drain
the shared iterator synchronously. Native
measurements include input admission, complete main-context iterations,
publication/rollback, a larger image and queued-input cancellation. They report
actual costs rather than inferring latency from the search pixel budget.

Large-image admission/spill, worst-case frontier bounds, sustained tablet input,
per-phase p95/p99 latency and all-platform tests remain open. Byte nonlinear
precision and single drawable/symmetry constraints from the current core are
reported as explicit unsupported cases rather than silently approximated.

Animated brush pipes are not covered by this bitmap checkpoint. Native
`gimp_brush_pipe_copy()` copies index arrays but resets the current brush to its
first element, and a per-stroke resource clone does not propagate runtime index
advancement back into the source pipe. Incremental/random pipe sequencing over
queued strokes requires independent old-runtime comparison and explicit state
carry. No pipe-flattening or sequence-compatibility claim is made here. Native
Shift/constrained-line and smoothing paths still need separate old-runtime
oracles beyond the bitmap freehand fixtures.
