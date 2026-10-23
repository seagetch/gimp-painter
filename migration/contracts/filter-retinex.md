# Isolated Retinex compatibility route

WBS 16.003/isolated-retinex-route connects saved `plug-in-retinex` definitions
to the accepted asynchronous process bridge, typed BindingStore, scheduler,
admission and owner context. Its six integer arguments and binary64 cvar remain
saved verbatim, including stale image/drawable IDs; the child binds its private
native objects. Opaque definition bytes remain unchanged. GEGL reads only the
completed cache and never starts a Retinex operation.

## Native and numerical contract

The literal selector 3 resolves only the bundled `contrast-retinex` executable
using the accepted executable-relative locator. Both its public and hidden
registrations, complete signatures, native object bindings, ranges, defaults
and distribution choices are checked before the hidden entry is registered in
the private PDB. `plug-in-painter-retinex` has no menu and rejects interactive
and last-values invocation. The public modern `plug-in-retinex` retains its
16..250 scale range and double arithmetic; the hidden entry admits old scale
16..256, nscales0..8, modes0/1/2 and finite cvar0..4.

The old PDB double is narrowed to gfloat before multiplying float variance.
That product and subsequent addition/subtraction with float mean retain old
float arithmetic. The transport preserves the original binary64 bits; narrowing
belongs only to the hidden native kernel. Statistics sum RGB but divide by
width*height*BPP. Standalone old RGB therefore uses a real three-channel drawable;
RGBA uses four. RGB's RGBA carrier must be opaque everywhere, including outside
the selected ROI. Explicit packing/expansion introduces no color conversion.
Raw RGB shadow has implicit alpha255, whereas unwritten RGBA shadow is zero.
The live old FilterLayer domain is always RGBA, not the standalone RGB oracle.
Gray and higher precision remain unsupported by this Retinex route.

Selected width and height must each be at least16. An outside or narrow start
selection fails with retained definition/cache, matching the old entry's gate.
This differs from the accepted Small Tiles no-merge behavior. Execution uses
the selected rectangle as the native kernel domain. Start ROI and final
selection/components/own-alpha-lock phases use the existing shared owner context.
Standalone merged output applies the documented old zero-alpha hidden-color
repair; raw output does not. Final live merge owns that operation.

The source, BPP float output, two float color planes and two recurrence buffers
require23 bytes/pixel for RGB or28 for RGBA plus8*(max dimension+3). The owner
conservatively reserves full-drawable scratch in addition to bridge rasters,
owner input/selection snapshots and the existing256MiB native runtime reserve.
Signed native geometry is validated before multiplication/allocation; captured
start geometry is validated before helper launch. Allocation failure at any of
src/dst/in/out/w1/w2 propagates failure before shadow acquisition or publication.

## Transport

The original Retinex checkpoint used GPF4 / `--filter-worker-v4`. Convolution
expanded this to GPF5; current GPF6 retains these fields in its first88 bytes and requires canonical unused
Convolution fields; see [Convolution transport](filter-convolution.md). GPF4 used the existing24-byte header and88-byte request.
Offsets0..56 keep the accepted selector, dimensions, Blinds scalars, Gray tag,
background, start region/flags and Small Tiles factor. Native storage is at60;
Retinex scale/nscales/mode occupy64/68/72; word76 is reserved zero; cvar is an
IEEE754 binary64 little-endian payload at80. Blinds/Small Tiles require storage4
and canonical unused Retinex values; Retinex rejects noncanonical Blinds/tiles
scalars. Old versions, invalid geometry/types/options/storage and inconsistent
RGB opacity fail closed. Exact framing, terminal disposition, EOF, successful
reaping and cleanup remain mandatory before outcome publication.

## Evidence and limits

`../fixtures/retinex-evidence.tar.gz` is a compact exact-byte subset of the
preserved historical mixed captures, not a new old-runtime capture. Its member
manifest links every retained file to original archive/member SHA256 values.
The174 actual old PDB outputs cover87 RGB and87 RGBA calls, rounding-sensitive
cvar values, scale boundaries and zero/eight scales. Six genuine old live scenes
provide12 observed merges under no/hard/soft selection. Historical32 public
before/after calls and174 hidden captures corroborate those semantics.

Current helper/native comparisons, public metadata/output checks, source hashes,
focused sanitizer/allocation injections and fresh relocated Save/reopen outcomes
are recorded in `../tests/filter-retinex/`. Generated zero-scale and padded-ROI
assertions are labeled analytic and are not counted as additional old captures.

This is a bounded U8 nonlinear RGB route. Conservative1GiB memory admission,
configurable spill with8GiB default, logical rather than physical reservation,
disabled-swap failure, global latency and Windows/macOS
process gates remain explicit. Focused normal/sanitizer and dirty prototype
packaging checks are not whole-application, platform or release acceptance.
XCF multipart, other procedure families and unrelated migration gates remain
outside this task.

Generation-scoped owner/editor progress is now covered by `filter-progress.md`.
