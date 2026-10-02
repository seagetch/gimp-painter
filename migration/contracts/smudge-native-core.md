# Legacy Smudge native core checkpoint

The dedicated `GimpPainterSmudge` / `GimpPainterSmudgeOptions` pair preserves
independent old accumulation rate and `use-color-blending`, using the native
`blending-output` dynamics channel. Standard GIMP3 Smudge/Flow is unchanged.
One typed BindingStore slot owns each core/options implementation.

The native owned begin/motion/finish interface retains the image, drawable,
options and original native undo snapshot for one transaction. Cancellation,
error, disposal and callbacks requesting cancellation roll it back. Intervening
external pixels/geometry/format invalidation is detected and independent pixels
are preserved rather than overwritten by a stale snapshot. This does not claim
arbitrary external edits during the renderer's own synchronous buffer callback
can be distinguished. The raw PaintCore interface is a numerical test route;
callers must check `dup_error`. The future registered tool must use the owned
error-propagating interface and owner context, never upstream's paint thread.

Numerical details retained independently of modern BrushCore raster output:
- fixed diagonal accumulation footprint, ignoring size dynamics at admission
- byte rate accumulation and separate output shading without feedback of shading
- distinct alpha Replace and opaque PaintCore composition, selection and channels
- original generated/bitmap transforms and old mask row rounding (+127/+128)
- shared native interpolation, evaluated with the old drawable-local doubles
- exact hidden color bytes under zero alpha

## Evidence

`smudge-owned-native.{txt,json}` records the final normal 10 lifecycle groups
and both raw/owned traces. `smudge-native-sanitizers.json` records 15 focused
source units under ASan/UBSan/float-cast-overflow, source/header hashes, no source
mutation during the run, and the same successful traces. Dependencies and the
rest of GIMP are not fully instrumented; leak detection is disabled by the host.

The independent pinned old runtime has 48 scenes across Y/YA/RGB/RGBA,
rate 0/50/100, blending off/on and size dynamics. Every initial/finish/Undo/Redo
image matches: 193 records, 2,952,800 normalized bytes per route. The transparent
old probe retains the same standard output and explains the rounding difference.

Lifecycle coverage: one-Undo/Redo reuse; cancellation; disposal rollback;
start-preview and publication callback cancellation; last caller core/image ref
loss; all 14 nonfinite coordinate axes; external pixel preservation; invalid
vfunc arguments. Shared PaintCore class admission ABI is included in hashes.

## Open acceptance gates

This checkpoint does not register a visible tool. Its owner-context GUI,
resumable long-event controller, old tool/profile identity migration and generic
stroking transaction integration remain open. Only nonlinear u8 RGB/Gray single
drawables are accepted; high precision, indexed, symmetry, pipe selection and
wider ICC/brush matrix require explicit integration. Full-dab vectors and mask
transforms are not yet memory-admitted/chunked; the current geometry ceiling is
not a bounded-memory guarantee. Do not mark WBS23 complete from this checkpoint.

The registered-tool follow-on adds one-dab continuations, image pending queries
and one-shot native-start admission. The old raw numerical entry is no longer a
supported unowned production call: current fixtures compare owned synchronous
and owned stepped routes. See `smudge-tool-controller.md` for that checkpoint.

## Owned generic entrypoints follow-on

Raw Stroke, native Path and Boundary now use the same existing Fill/Smudge owned
transactions across all subpaths. See `owned-generic-stroking.md` for the scoped
implementation, independent old entrypoint fixtures and exact checkpoint gates.
