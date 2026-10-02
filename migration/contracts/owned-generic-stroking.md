# Atomic native Fill Brush and Painter Smudge generic stroking

The existing prepared-coordinate route in `gimppaintcore-stroke.c` now dispatches
native raw Stroke, Path and Boundary to the existing typed Fill/Smudge owner
slots. There is no second renderer, implementation store or native base ABI
change. Bare unowned native start remains refused.

Each operation retains the core, original options, drawable, image and watched
resources through callbacks. It validates all fourteen numeric coordinate axes
before starting and rejects nested or already-pending image transactions without
cancelling them. One native start/snapshot spans every disconnected subpath;
FINISH/INIT resets family-local Fill projection/search or Smudge accumulation
state while native cumulative interpolation state and one original Undo snapshot
remain intact. Each accepted motion drains the existing resumable step API.
Image-space inputs retain the existing per-family offset arithmetic.

An operation-local shared cancellation record watches options and original
brush/dynamics/pattern/gradient resources through common Connection and weak
signal payloads. It neither captures an Impl pointer in a callback nor clones
live brush-pipe state. Before native commit, cancellation, close, preparation or
publication failure rolls back the whole owned transaction when the target is
still current. Independently changed raster/format/extent data is preserved
instead of receiving a stale snapshot. The public coordinate/distance bookkeeping
is restored on failed operations. There is no Undo/Redo stack surgery.

The requested push_undo policy is stored in the existing slot. Commit is sealed
after FINISH callbacks and a fresh target validity check, immediately before
native finish consumes the snapshot. Later notifications emitted while native
Undo/image-dirty dispatch is already committing cannot retroactively report a
rollback. Pending-paint queries remain true through actual commit/rollback.
Outside-canvas strokes are genuine no-ops. Existing zero-opacity native extent/
Undo behavior is retained rather than silently replaced with a new policy.

## Native preparation fixes

The native boundary sorter changes visited flags despite its const signature.
Owned dispatch now sorts a copy, preserving caller-owned arrays. Offset additions
are promoted before arithmetic, avoiding signed integer overflow.

Real Path tests exposed uninitialized view metadata in native Bezier subdivision:
coordinate arithmetic assigns paint axes but does not assign xscale, yscale,
angle or reflect. The producer now seeds each subdivided coordinate from its
source endpoint before arithmetic. This changes no paint-axis interpolation and
does not weaken validation of raw or custom-stroke coordinates.

## Independent evidence

`legacy-owned-generic` compiles the actual pinned old Stroke/Vectors/Boundary
entrypoints from unchanged archives. It has three routes for each of the existing
12 Fill and 48 Smudge scenes: raw input, two separated path subpaths, and two
boundary loops with offset and emulated dynamics. Its fixture header constructs
equivalent stored direction: old Bezier lineto prepends anchors, whereas GIMP3
appends. Using superficially identical constructor argument order produces
opposite traversal and was rejected as an invalid comparison. Initial mismatched
runs and the producer diagnostic are not described as successful evidence.

The existing family native test fixtures add actual generic API lifecycle groups:
one Undo/Redo, no-Undo with existing Redo, no-op, offset/selection and separated
Path/Boundary geometry, immutable caller arrays, nested/active admission,
first/second-subpath failure, every nonfinite axis, option/resource changes,
explicit owner close, last caller-owned core/options reference loss, FINISH
cancellation/external pixels, pending Save queries, coordinate restoration and
harmless postcommit notification.

All six independent generic comparisons pass exactly: 690 records / 14,303,286
normalized bytes across 180 scenes. The final normal run passes 36 Fill lifecycle
groups, 23 Smudge lifecycle groups, all five unchanged old family trace variants
and the MyPaint generic regression suite. See `owned-generic-native.log`,
`owned-generic-native-tests.{txt,jsonl}` and `owned-generic-native-oracle.json`
for the exact checkpoint outcomes. The focused sanitizer builder is provided but no successful
sanitizer run is claimed until its report is present. This is a native API
checkpoint, not a physical-device, standalone PDB transport or full GUI claim.

The existing nonlinear-u8, symmetry, ICC, pipe, large-dab memory/latency and legacy
geometry acceptance boundaries remain. In particular, this work does not infer
legacy geometry from a standard brush's blend mode or paper option.
