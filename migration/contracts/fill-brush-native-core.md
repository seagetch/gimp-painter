# Fill Brush native owner-thread transaction checkpoint

`GimpFillBrush` subclasses the real GimpBrushCore; options subclass native
GimpPaintOptions. Both use one BindingStore implementation slot and the common
ObjectRef route. The registered paint callback keeps the old
`gimp-bucket-fill-brush` identifier. The real GUI/controller is a separate
dependent checkpoint, not established merely by registering this callback.

Begin and motion accept image-coordinate GimpCoords, including pressure and
other axes. Begin starts native paint/INIT and freezes no synthetic dab. The
first motion paints and seeds the interpolation origin; later motion uses native
brush interpolation. Legacy spacing comes from the brush resource rather than
GIMP3's newer independent paint-options spacing default. The old hardness-output
as force behavior is retained. Rate0/50/100 remains a serialized option without
inventing a new effect; its inertness is observed in actual old stroke fixtures.

The first dab freezes projection bytes and stroke-origin color. Each immutable
dab owns its transformed mask, rectangle, target/image, color, mode and opacity.
Search stays inside the old brush/drawable/projection bounds and applies the
legacy mask penalty and one-pixel grow. Final painting is clipped back to the
old brush rectangle, as the old canvas subwindow did. Selection is applied by
native PaintCore at composition, matching the original Fill Brush; it is not an
invented second selection multiplier in the brush search.

Completed masks feed genuine native paint/erase, constant application mode and
one Undo transaction. Paint buffers must be backed by GimpTempBuf; a plain
GeglBuffer makes the native non-applicator loop return without painting. Direct
Painter blend operations use the separately tested native dispatch route.
The original old native drawable opacity scaling by255.999 is preserved.

## Lifetime and error behavior

The wrapper owns the drawable, image and options before native start callbacks.
Reentrant cancellation during start is deferred until native start unwinds.
Finish refuses to commit while queued work remains; it never waits for it.
Cancel or disposal rolls back already-published dabs while the target raster remains the same. Publication callbacks
may request cancel or release the last caller-owned core/image reference without
using freed state. Resource members are detached before finalizers run. Nested
motion/drain transitions are rejected, and work errors abort the transaction.
Fourteen floating coordinate/axis fields are checked before native start or
interpolation. Buffer identity, format, dimensions, offsets and independent
pixel writes invalidate queued work. A stale raster is never overwritten with
the transaction's old Undo snapshot; cancellation leaves the independent edit
intact and reports failure. A content-lock-only change can safely roll back our
pixels without changing the lock. The read-only image pending-paint query stays
true through native start, publication and deferred close/rollback, and never
runs the executor itself. Save continuation belongs to the tool integration.

The owner API is required; bypassing it through bare native paint-core calls
bypasses its lifecycle guarantee.

A completed dab is published in input order. The scene trace matches the old
result both when drained after each input motion and when every motion is queued
before draining. This is not a license to share mutable UI objects with workers;
all native calls remain on their BindingStore owner thread.

## Evidence

- Twelve actual old RGB8 paint/erase/selection scenes, all36 full finish/Undo/Redo
  snapshots (1,814,962 normalized bytes) match the new native controller exactly
- The original harness forgot to seed the interpolation last-coordinate after
  its first stamp. Its output is retained under `initial-harness/`; the corrected
  authoritative old capture mirrors the normal tool initialization
- A transparent vfunc observer records actual old dab coordinates under `dabs/`;
  it forwards to the original vfunc and reproduces all corrected output bytes
- Twelve native test groups cover repeat transactions, one Undo, early/published
  cancel, disposal rollback, start/publication reentry, last caller-owned
  core/image ref loss, detached targets, all14 nonfinite coordinate axes, external
  pixels, resize/precision/lock invalidation and start/close pending-query lifetime
- Seventeen focused units pass ASan/UBSan/float-cast-overflow, including native
  PaintCore/BrushCore/Undo paths and both full-scene queue variants. Other host
  components/libraries are not instrumented; leak detection is disabled. The
  registered MyPaint options/session owners are compiled with RTTI in this harness
  so the shared instrumented BindingStore can validate their base types; mixed
  -fno-rtti startup objects are not interpreted as an application failure

## Remaining gates

The proven full-scene contract here is unprofiled nonlinear RGB8 with one
drawable and one symmetry origin. Gray/profile/color-management, high precision,
all transformed/generated brush cases, expansion, line-tool mode and multiple
symmetry origins require their own old/runtime comparisons or explicit native
policy before claiming full compatibility. Current high precision/indexed or
multi-symmetry requests fail explicitly rather than silently approximating.

The bounded search's per-step candidate count does not bound native projection
preparation, mask transformation, whole-dab composition, destruction, pending
queue memory or I/O. End-to-end large-stroke responsiveness/admission and actual
GUI input lifecycle remain open; the controller must preserve queued input and
settings and invalidate safely on intervening external document edits.

## Owned generic entrypoints follow-on

Raw Stroke, native Path and Boundary now use the same existing Fill/Smudge owned
transactions across all subpaths. See `owned-generic-stroking.md` for the scoped
implementation, independent old entrypoint fixtures and exact checkpoint gates.
