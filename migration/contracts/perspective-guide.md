# Perspective rulers: model, editor, overlay and ownership

Source baseline: gimp-painter `afa43fae3e920210146abed514f136fd49f671b5`.
Port baseline: GIMP 3.0.9, C entry points and native GObject vfuncs; C++14 feature
implementation uses the common `ObjectRef`, `Connection`, and one `BindingStore`
per implementation owner. No old interface/NewGClass/delegator layer is retained.

## Preserved behavior

* Zero to three vanishing points, insertion order, image-coordinate storage;
  Shift-click adds at the clicked coordinate, ordinary drag moves a hit point,
  Ctrl-click removes it and collapses the remaining indices
* First pressed Shift/Control selects the edit operation until that modifier is
  released; plain clicks do not silently create a ruler
* Point selection first chooses nearest in image coordinates, then tests the
  standard 13-display-pixel circular tool handle. This preserves the legacy
  ordering even under nonuniform scaling
* Overlay has 8-scaled-pixel-radius point markers and joins the first two points
  only for a two-point ruler; the editing tool joins the first two for both two
  and three points, exactly as its old drawing function did
* Canvas coordinates use GIMP's normal zoom/pan transformation, with rotation
  and both flips applied by the canvas group. No second rotation is applied
* Direction selection starts at 32 scaled pixels (inclusive). One point has
  horizontal/angle, perpendicular, then its vanishing direction; two points have
  the perpendicular to their horizon before the two point directions; three
  points use their three directions in insertion order. Strict `<` comparison
  makes the first candidate win ties
* Input direction uses scaled displacement, while candidate angles use image
  coordinates, retaining the old anisotropic-scale behavior. Constraint keeps
  the dominant coordinate and computes the other, rather than orthogonal
  projection. Exactly 45° and 135° use the vertical-coordinate branch
* Rulers were not written to XCF, not copied by image duplication, and not
  restored from presets in the pinned implementation. They remain session-only;
  no new implicit persistence contract is introduced

Shell lazy-start state and motion-buffer/stabilization ordering are integrated
and tested in a separate dependent event-route slice. Pure helper tests alone do
not establish that the production event route is connected.

## Intentional corrections and explicit enhancement

The actual old model runtime is captured in
`migration/fixtures/legacy-perspective/runtime*.{json,log,stderr}`. Its unchanged
behavior source and existing old core archive are hashed. This is distinct from
the extracted source-only direction helper oracle.

The old constructor overwrote id97 with0 through the `angle` property. Setting
angle1.25 changed id and angle to1. The new independent double angle property
preserves id97 and1.25. The old `removed` registration used an invalid void
signal parameter and reported a GLib critical. The new signal has zero arguments
and no recursively emitting class default handler.

Negative/out-of-range indices and null output pointers return failure without
accessing storage. Removing a point releases its storage (fixed-size value array,
no heap-allocated leaked points). NaN/infinite point coordinates are rejected.

Image setters borrow their argument and acquire their own strong reference before
releasing the former guide. Self-assignment is a no-op. The new value is published
before removed/changed notifications; observers may reenter and replace it.
Dispose publishes null and prevents resurrection through a reentrant setter.
A caller-retained guide remains valid after its image is gone.

The overlay snapshots points and wraps snapshot replacement in canvas
begin/end-change, invalidating both old and new screen extents. Extents include
all points and the entire two-point connecting line. Empty rulers return no
region; extreme off-canvas coordinates are safely bounded for Cairo integer
regions. The overlay observes shell construction's deferred display property,
display image changes, image guide replacement, and guide edits using scoped
weak-owner, generation-checked `Connection` callbacks.

Editing Undo is an explicit enhancement required by WBS26.010: one image Undo
per completed add/delete/drag, deep value snapshots, normal Redo, cancel without
an Undo entry, and no session state leaking into subsequent edits. The Undo has
`GIMP_DIRTY_NONE` because the ruler is not an XCF field. Cancelling a stale edit
must not overwrite a replacement model installed externally.

## Registration compatibility

The pinned ruler used G and the pinned blend tool used L. GIMP3's gradient used G
and plain L was unused (Handle Transform uses Shift-L). The migration restores
ruler G and gradient L defaults, without changing user-saved accelerator maps.
The default toolrc includes the ruler. Reading a pre-ruler toolrc appends the new
ruler instead of resetting the user's existing groups and ordering.

## Evidence boundaries

* `runtime-capture.log` is an actual old model execution against the unchanged
  pinned source's archive, using the documented legacy compatibility build
* `snap-angle-source.inc` is the verbatim pinned direction helper, name-mapped
  and logging-disabled in the test; 7,500 seeded cases compare its arithmetic
  with the new helper. It is explicitly source-oracle evidence, not old GUI input
* Core tests cover property/index bounds, old model fixture, helper parity,
  threshold/ties/coordinate constraint, ref ownership, replacement/disposal
  reentry, Undo/Redo and signal-time last-reference release
* Native GTK tests cover edit operations and one-Undo-per-drag, zoom/rotation/
  both flips hit tests and cancellation, overlay extents and actual Cairo output,
  image replacement/disconnected old models, default shortcut uniqueness and
  pre-ruler toolrc group preservation
* Focused ASan/UBSan instruments the new implementations, image owner path,
  shared BindingStore, registry adapter and tests. Other upstream components and
  linked dependencies are not instrumented; app-level leak detection is disabled
