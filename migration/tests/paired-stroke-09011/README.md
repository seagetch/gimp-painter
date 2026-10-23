# Supported fixed-bounds paired MyPaint history

**Bounded child `09.011/paired-fixed-bounds`; original
`09.011/mypaint-undo-stroke` deliberately remains open.**

The native GimpMypaintCoreUndo now carries real controller snapshots alongside
its inherited exact-format pixel payload. Immutable Engine checkpoints and
immutable Resource configurations record both endpoints. A weak original
controller identity prevents applying history to a replacement; a temporary
prepared lease prevents destruction/reconfiguration during pop. Multiple
same-controller entries in a native group use depth-counted exclusion.
Destroyed controller history still restores pixels without inventing a target.

All new fallible preparation completes before native publication. The original
native paint rollback baseline is retained until then. Explicit publication
outcome distinguishes rejected preparation from an entry expired immediately
by observers. Native finish(FALSE) performs cleanup after publication. Cancel
and failed preparation restore Engine/Resource/native continuation coordinates
before fixed-bounds native pixel rollback, so GEGL changed and drawable update
observers see matching restored state. Pending-paint checks cover active samples
and hover, including ordinary Undo entries, not only this new subclass.

## Verified checkpoint

* 17 actual native controller groups, all 24 existing Surface groups (including
  original exact low-bit/format/extent/32-cell native payload comparisons).
* 17 selected **13-unit** ASan/UBSan groups; not full application/dependency or
  leak-sanitizer coverage. Five compiled causal controls fail their targeted
  assertions: state, Resource, pending guard, late cancel restore, memory count.
* New cancel and preparation-failure observer tests first reproduced the real
  pixel/state ordering bug. Both red receipts and the pre-fix controller remain.
* Native predecessor typed-entry lifecycle16 proves typed-property, once-free,
  disposal/reentry and payload/live-buffer callback behavior. Its immutable
  evidence is replayed, not relabeled as this controller checkpoint.
* Existing prepared publication18, native pop guard11, preset UI18 and
  perspective UI24 pass on the coupled build.
* Historical-install6, Prepared4, checkpoint9, Resource9 and the full unchanged
  legacy evaluator/decoder traces pass (5,637,911/1,125,707 bytes respectively).
* C++14/noRTTI first/double header inclusion passes.

The Surface collector now recognizes native DrawableUndo **subtypes** rather
than an exact type-name string. All original count, byte, format and geometry
assertions remain. A separate pair assertion requires the actual new native
GimpMypaintCoreUndo. The first coordinate fixture incorrectly expected incoming
coordinates; native start_coords records preceding last_coords. The corrected
fixture captures that exact preceding native state and compares complete
coordinates over repeated Undo/Redo. Initial fixture failures are retained.

## Rendered evidence (direct API, not input gestures)

Authenticated Xvfb 1080x1920x24, actual application window1040x1800, oncanvas UI
visible asserted. Four uncomposited PNGs show baseline, painted, Undo and Redo.
The normal direct-caller image flush is performed before capture. The same
actual transformed image interior (217,748;510x382) is compared as rendered
pixels: painted differs, Undo equals baseline, Redo equals painted. Native
pixel bytes and full Engine continuation trace/Resource also match.

Initial GUI setup errors (missing GTK initialization/application activation)
and the initial unflushed capture were diagnosed, not presented as valid proof.
The unflushed images remain privately as diagnostics. Delivered PNGs are the
subsequent real rendered, asserted checkpoint; no annotations/composites are
inserted into the application image.

## Retained memory accounting

The former unexplained131072 allowance is removed. Mapping reports actual
vector capacity*sizeof(Points); Brush accounts fixed arrays with sizeof and
sums every mapping allocation. Each opaque GRand receives4096 bytes. Installed
GLib2.84.4 g_rand_copy disassembly requests2500 bytes, consistent with the
[documented MT19937 implementation](https://raw.githubusercontent.com/GNOME/glib/2.80.0/glib/grand.c).
The archive pins disassembly and tested library hashes.

Resource counts every retained JSON node with a conservative512-byte allowance
for node/container headers, hash minimum buckets and capacity slack, list/value
and allocator slack, then adds UTF-8 string/key lengths. Installed JSON-GLib
1.10.6 requests32/56/24 bytes for node/object/array structures; the allowance
also covers empty nested objects. These are estimates for the pinned backend,
not a portable promise about arbitrary future allocators or process RSS.
Immutable Resource copies compact the input tree; no retained mutable table
with arbitrary removed-element capacity is shared. Shared checkpoints/resources
are deliberately counted independently in each history entry (safe overcount).
Saturating sums are computed before publication; the native getter returns the
cached amount without new C++ allocations. The dense1024-node test reports
checkpoint38048, Resource525927 and nativeUndo1194204 bytes, verifies growth
and tests the getter with allocation failure armed.

## Explicit remaining boundary

Original afa43fae `gimpmypaintcore-surface.cpp` clips dab bounds to drawable
width/height (lines180–185); current GeglSurface also intersects the existing
buffer extent. Neither implementation expands layers itself. Nevertheless an
external native resize during an active transaction can reach native
`gimp_paint_core_cancel`'s expanded-geometry branch, which emits an early update
before replacing pixels. That observer atomicity is **not solved or waived**
here. It remains owned by original `09.011/mypaint-undo-stroke`; no global signal
blocking/general cancel hook was introduced. Old raw Stroke pointer ABI,
unrelated C-entry/handle inventory and other source duties remain separately
open. Standard native pixel allocation/fatal-OOM behavior remains unchanged;
only new historical state installation is no-throw/noalloc, not the whole pop.

Eight held candidate providers are excluded: their committed tested baseline
sources are separately archived and sealed. This is a scoped private-build
checkpoint, not a claim of an entirely current working-tree build.
