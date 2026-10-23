# DrawableFeature buffer writes — 09.004

Original action: **DrawableFeature の buffer write を実装する**.
Original acceptance: **範囲外に書かず dirty 領域を返す**.
Dependency: 09.003 (completed read criterion, not whole-file closure).

The production replacement is `GeglSurface::draw_dab`/`Impl::write`, not a new
parallel DrawableFeature implementation. Coverage intersects the requested dab
with the target child-buffer extent and the translated selection extent before
any pixel write. The private writer receives that clipped rectangle. Native
storage and destination formats remain unchanged. The tests execute this actual
producer; no substitute bounds implementation is used as the tested subject.

## Dirty contract

* A successful dab publishes the clipped rectangular ROI through its dirty
  observer and includes it in the session bounding box returned by `dirty()`.
  It is a conservative invalidation region, not a minimal list of changed pixels.
  Selection holes and gaps between dabs can remain unchanged inside that box.
* The accumulated dirty state is available when the Surface observer executes.
  GEGL's own `changed` emission is distinct, and may be frozen/coalesced by its
  owner. Tests use **gegl_buffer_signal_connect**, as GEGL's API requires; plain
  g_signal_connect intentionally does not enable change tracking.
* Empty/touching requests perform no writes or dirty notification. Sampling
  performs no writes. Session commit retains pixels and dirty state; the next
  session resets counters and dirty state. Undo/cancel integration and every
  reentry/failure lifecycle remain separate existing owner tasks.

## Actual execution coverage

The current coordinate target has seven TAP groups. Two added groups exercise:

* 144 RGB/Gray, alpha/no-alpha, component precision, TRC and space formats;
  incremental/nonincremental modes; 16 negative/partial/outside/translated-mask
  cases each: **4,608 actual producer operations**. An independent per-pixel
  membership oracle checks every parent-storage byte: every allowed pixel
  changes, every disallowed pixel (including the populated parent halo) does not.
  Actual GEGL changed rectangles, Surface dirty callback and returned rectangle
  agree for these full-coverage cases. Subsequent sampling/commit adds no writes.
* RGB/Gray byte/double × two accumulation modes, overlapping and separated dabs,
  observer-time dirty publication, GEGL freeze/thaw coalescing, preserved gaps,
  empty dabs and next-session reset. The union is deliberately conservative.

The five existing coordinate groups also test signed mask offsets, empty/touching
intersections, child-buffer sampling, overlapping/adjacent dirty unions and
nonincremental cancellation. Standalone normal and ASan/UBSan/float-cast-overflow
run the actual Surface source. Dependency libraries are uninstrumented and
LeakSanitizer is disabled. Native integration results are recorded separately.
No new UI feature is introduced; these tests do not claim screenshot coverage.

## Historical duties remain represented

`drawable-buffer-write.json` records all 14 assigned source identities and their
relationship to the 09.003 resolution. Their inventory WBS assignments and TODO
checkpoint statuses are unchanged. Criterion completion must not mark the whole
hunks complete:

| Duties | Explicit remaining owners |
|---|---|
| Six dispatcher hunks | 31.017: independent serial five-region composition, registration, descriptor-copy, parallel scheduling/lifetime replacement |
| One whole pixel.hpp | 31.017 scalar replacement, 09.015 full precision/color handling |
| Six TempBuf hunks | 09.005 conversion, real TempBuf subwindow writes, copy/export/stride/ownership |
| Whole DrawableFeature header | 09.005–09.012 and 09.003/region-lifetime: conversion, masks, snapshot/work allocation, application dirty aggregation, Undo, commit/cancel and complete lifetimes |

Thus the original write criterion does not depend on its own successor TempBuf
conversion task. All outstanding semantics survive in explicit existing owners;
no extra child task substitutes for the original task or hides its obligations.
