# Value Invert raw raster foundation

Task: `16.003/value-invert-raster-foundation`.
Status: **DONE for the standalone raw-raster foundation**.
Integrated baseline: `b0c98d7a049285e467548873806a2a19cece22d5`.
The source-only `024092982d99bed43937c364b6864c11313a882a` checkpoint was
recovered and hash-verified, then reconciled with this current publication.
Only its new Value Invert task/progress row was merged; later accepted work
was preserved. Tests ran in a separate worktree/output tree without altering
the accepted f271db84 aggregate source or build.

## Bounded contract

`filter_value_invert_raw_raster` is a new, independent worker-only raw API for
private tightly packed RGBA8 input/output. RGB sources must be promoted by the
caller with alpha 255. This foundation makes no Gray, indexed, native-owner,
XCF, selection-observation, or normalized-real admission claim.

The pinned legacy source is
`afa43fae3e920210146abed514f136fd49f671b5:plug-ins/common/value-invert.c`,
SHA-256 `bf7c46cac7f62c13ec73590d53c12ba3dd85fd28def5f74451aaf43b969fef20`.
For original RGB, let M be the maximum and m the minimum channel. If M=0 or
M=m, every output color is 255-M. Otherwise each original channel c becomes
`((255-M)*c + M/2)/M`, using integer division. The alpha byte is copied.
Hidden RGB is transformed even when alpha is zero. The callback's original
R/G/B branch ordering, tie handling and rounding are retained in the literal
C test oracle rather than recreated as the same common equation.

`FilterPointShadowOperation::value_invert` adds this API to the existing raw
shadow adapters. Its generic integer argument is ignored, because the legacy
`plug-in-vinvert` procedure has no operation-specific parameter. Threshold
signatures and tag values remain stable, and existing Max RGB raw, byte, and
normalized-real entry points retain their contracts. In particular,
`point_byte` and `point_real` keep their historical already-merged alpha-zero
hidden-color preservation; they are not reused as the new raw API.

The shared adapter retains:

- captured immutable half-open drawable-local ROI and signed INT32 area limit;
- full zero-initialized private shadow and a distinct outside-selection
  `no_merge` result, which must never import zeros into the original;
- bounded 16 KiB row-split full-raster transfers and a 32 KiB raw stack buffer;
- no row-sized scratch, owner/GObject/GEGL references or extra scratch raster;
- pre-I/O validation of tags, dimensions, extents, geometry and direct aliases;
- storage exceptions, bounded cancellation, final flush and monotone transport
  progress, with one reported only after the final cancellation check;
- transactional vector publication and unchanged completed buffers on failure.

The area bound has separate Value Invert provenance: `vinvert` uses its own
`gimp_pixel_rgns_register/process` loop and `total = width * height`, whose
operands are signed gint even though total is gdouble. It does not invoke
`gimp_rgn_iterate2`. Transport progress does not claim to reproduce the old
first-region/every-16-regions callbacks or owner timing. Owner-side generation,
lifetime and final-context merge are still required by the later integration.

## Executed normal and focused sanitizer evidence

`app/painter/tests/test-filter-point-shadow.cpp` passed all of the following
in both the normal C++14 and focused ASan/UBSan builds:

1. All 16,777,216 RGB triples in bounded 65,536-pixel batches versus the literal
   pinned C callback, both bpp=3 and bpp=4, with varying copied alpha.
2. Every alpha byte for black/white/achromatic colors, distinct permutations,
   tied maxima, extrema and integer rounding examples; preserved legacy byte
   hidden colors and normalized-real arithmetic.
3. Tiny, full, cropped, outside and row/tile-boundary vector/memory/spill paths,
   poisoned outside pixels, offsets, ignored-argument extrema and completed
   buffer isolation.
4. Malformed tags/geometry/extents, direct aliases, overflow and signed-area
   boundary rejection before storage access, including pre-cancellation.
5. Every bounded read/write/flush cancellation and injected error ordinal for
   direct raw API and shared vector/spill shadow adapters; monotone progress,
   no I/O after cancellation, output alias transactions and no-merge behavior.
6. Four archived genuine final `plug-in-vinvert` PDB outputs, compared only
   after explicit full-coverage REPLACE_INTEN merge. These are historical
   final-output observations, not fresh raw callback or native-owner captures.
7. Existing Threshold (20 archived outputs, generated geometry and failures)
   and Max RGB (16 archived outputs, signed modes/ties and failures) tests.

The C oracle is a test-only translation unit, separately compiled as C99.
The runner verifies its full harness seal and byte-for-byte callback equality
against the complete hash-pinned source. Meson also compiles this translation
unit into the point-shadow test. All source/fixture/object/executable seals
must be unchanged for native acceptance. Focused sanitizer mode covers both
C and C++ with ASan/UBSan and explicitly excludes LeakSanitizer.

## Acceptance and provenance

`migration/tests/value-invert-raster-foundation.json` records both final runs
and their exact source, fixture, object and executable SHA-256 seals. All 20
source inputs and 50 historical fixture inputs were unchanged after verification,
compilation and execution. Every command exited zero without stderr diagnostics;
objects and executables were rehashed after execution and remained unchanged.
The C oracle is separately compiled as C99; both C and C++ receive ASan/UBSan
in focused sanitizer mode. Leak detection is explicitly disabled, so this is
not LeakSanitizer acceptance.

Independent review found a missing post-run object-hash comparison in the
source-only checkpoint runner. The final runner requires that comparison,
with new controls rejecting changed C objects, removed C++ objects, and
changed executables. All 16 mocked runner controls pass; they test bookkeeping,
not native arithmetic. Both native modes were rerun after this change. The
first normal run remains a superseded observation with no after-object seal.

The original hash-pinned legacy source objects were absent in the recovered
checkout. Their exact commit, path trees and four blobs were restored from
GitHub and independently checked against their Git object IDs. The unchanged
runner then confirmed the full source SHA-256 and literal callback bytes.
No oracle replacement or provenance exemption was used.

Complete function bodies of `point_byte`, `point_real`, `filter_point_raster`,
and `filter_max_rgb_raw_raster` remain byte-identical to b0c98d7a. The WBS
validator and whitespace checks pass. The parent 16.003 task remains open.

## Reproduction

With the recovered dependency environment loaded and the shared execution
lane free, use fresh report paths and freeze the source during each run:

```sh
python3 -B tools/check_tasks.py
python3 -B app/painter/tests/test_filter_point_shadow_runner.py
flock /workspace/shared/gimp-painter-build.lock python3 -B app/painter/tests/run_filter_point_shadow_tests.py ../value-invert-foundation-build --report ../value-invert-foundation-normal.json
flock /workspace/shared/gimp-painter-build.lock python3 -B app/painter/tests/run_filter_point_shadow_tests.py ../value-invert-foundation-build --report ../value-invert-foundation-sanitizer.json --sanitize
```

The runner honors CC/CXX/CPPFLAGS/CFLAGS/CXXFLAGS/LDFLAGS and pkg-config inputs.
After later integration, separately build/run the registered
`painter-filter-point-shadow` target plus existing point/native-kernel targets
against final unchanged sources. Standalone acceptance does not imply that
registered-target or full-aggregate result.

## Remaining boundaries

RGB-only owner admission, start/final owner-context timing, projection/cache
prefill, completed-generation publication, cancellation/replacement lifecycle,
real-route precision matrices, strict old-XCF reconstruction and fresh legacy
captures remain separate work. Indexed colormap mutation and Gray are outside
this independent RGB raster task. No full aggregate, whole-port, installed UI,
other operating system, enormous actual I/O, or leak acceptance is claimed.
