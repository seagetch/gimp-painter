# Bounded Surface buffer reads (09.003/bounded-surface-read)

This is an implemented and tested child of 09.003, not completion of the parent.
The actual existing replacement for the MyPaint image-buffer read path was
`GeglSurface::Impl::read`; adding a parallel DrawableFeature class would duplicate
that path. It now calls the small `BufferRead` helper. Target, initial-snapshot,
floating and native selection reads use it, and legacy selection coverage calls
it with an explicit `Y u8` output format. The synchronous owner remains Surface.

## Read contract

- Requests are buffer-local half-open rectangles, including negative origins.
  Image-to-local translation subtracts the item offset once. No image-canvas
  clipping or zero-origin assumption is introduced.
- Requests with negative dimensions or unrepresentable gint endpoints are
  rejected before GEGL access or output mutation. Zero-area requests are no-ops.
- Intersections use signed 64-bit arithmetic. An extent is not rejected simply
  because its exclusive endpoint lies outside gint. In particular, subtracting
  an extreme drawable offset from a distant selection cannot overflow or make
  an empty selection appear inside the target.
- Requested output dimensions and origin remain intact. Only the source extent
  intersection is read, with `GEGL_ABYSS_NONE`; outside pixels are zero, even if
  a GEGL sub-buffer has populated parent storage beyond its own extent.
- Output rowstride is bytes, may exceed packed width, and must fit GEGL's gint
  stride. Capacity includes the last pixel, not trailing row padding. Prefix,
  suffix and row-padding canaries stay unchanged. Multiplication is checked
  before allocating or addressing memory. Per-operation resource budgets remain
  09.009 work; this does not impose an arbitrary whole-image size limit.
- Default output format is the exact buffer format. Native component bits and
  palette indices are preserved. Explicit conversion is caller-selected. The
  tests check exact u8-to-u16 scalar expansion; no ICC/colorimetric accuracy or
  native-versus-legacy blend equivalence is inferred.
- The buffer is borrowed for the synchronous call; its owner keeps it alive and
  prevents concurrent mutation. Returned packed bytes are independently owned,
  survive source finalization and do not change with later source edits. A read
  is not a snapshot transaction, and the helper retains no persistent reference.

## Production regression

The prior selection code executed `mask_rect.x -= offset_x` and the corresponding
Y expression in gint. A mask at zero with an INT_MIN drawable offset overflows.
The regression compiles the exact previous production Surface from revision
`000527cc3931dff4df3347a375104ee90da23816` and runs the same extreme-offset
Surface case under UBSan. The checker requires an actual signed-overflow failure,
then runs the corrected current production source under ASan/UBSan and
float-cast-overflow. Baseline and current source hashes are saved with the logs.

## Coverage and remaining duties

The adjacent JSON retains all 48 source-duty identities, pinned blobs, ranges,
hunk hashes, individual meanings and remaining work. All broader parent duties
remain open: six five-region dispatch hunks, one scalar pixel definition, six
TempBuf/subwindow hunks, 34 contiguous-fill iterator hunks and the DrawableFeature
header. The fill hunks preserve their changed evidence and existing
27.004/gegl-bounded-search, 27.006 and 27.010 successors. This child does not touch
the current fill implementation, so static correspondence does not establish
its behavioral equivalence. TempBuf preview conversion, generalized iterator
lifetime, undo, write and clone/filter paths are not closed by these tests.

Eight new executable test groups cover 144 native RGB/Gray/alpha/precision/TRC/
space choices across 15 inside/partial/outside/touching/empty requests, four
palette choices, explicit conversion, signed offsets, 20,736 production
intersection comparisons against an independent pixel-membership oracle,
invalid-input no-mutation, result ownership and extreme Surface selection
positions. Five existing coordinate-boundary groups execute the modified Surface
for ordinary positive/negative selection offsets, child-buffer sampling, dirty
unions and nonincremental cancel.

Reproduce against the configured dependency environment:

    python3 -B tools/check_drawable_buffer_read.py --report migration/tests/drawable-buffer-read/native.json
    python3 -B tools/check_drawable_buffer_read.py --sanitize --prove-baseline --report migration/tests/drawable-buffer-read/sanitizers.json
    python3 -B tools/tests/test_drawable_buffer_read.py

The standalone checker compiles the real current Surface and helper with GLib,
GEGL and babl. It does not rebuild GIMP. Sanitizers instrument those sources and
the test harness, not dependency libraries; LeakSanitizer is disabled. The
registered native test is `painter-mypaint-buffer-read`.

## Registered native integration

The ordinary Linux native build was reconfigured with Meson to register the new
executable, then the new target and six neighbors passed 7/7, with no failures,
skips or timeouts. The new target passed all eight groups in 0.339 seconds.
`migration/tests/drawable-buffer-read/registered-native.json` retains allowlisted
test names, case results, durations and the two exact legacy-trace summaries,
plus hashes proving the 20 relevant implementation/header/test/registration
files equal those in the native source tree. No raw environment is retained.
An initial target lookup before regeneration failed; it is a setup diagnostic,
not an executed behavioral failure, and was retained by the parent.

Registered neighbors: coordinate-boundaries, region-lifetime, native-precision,
legacy-surface, legacy-selection and snapshot-isolation (each with the
`painter-mypaint-` prefix). These are normal native tests. The separate standalone
report supplies sanitizer evidence; no full-app sanitizer, full native aggregate,
LeakSanitizer or cross-platform pass is implied. Parent 09.003 stays open.
