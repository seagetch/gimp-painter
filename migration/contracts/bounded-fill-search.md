# Bounded fill: native byte search checkpoint

The pinned `afa43fae` old `gimp_image_contiguous_region_by_seed_full` and
`gimp_channel_grow` were executed directly from the already built old application
archives. The new implementation is not linked into the capture. 128 synthetic
cases cover native Gray, Gray-alpha, RGB, RGBA; threshold0/30/100/255;
antialias off/on; a zero/partial/full coverage mask; clipped bounds and 64-pixel
tile edges. Both search and grow outputs match byte-for-byte (256 masks plus one
completion record). The exact raw old output and normalized masks are retained;
two unrelated old startup debug lines are excluded from the normalized oracle.
The old plug-in registration and unwritable default pluginrc diagnostics are
retained; the capture exit was0. This is a direct core/runtime oracle, not an
interactive old Fill Brush stroke fixture.

## Search contract

`Fill::Snapshot` retains a GEGL copy-on-write copy in its original image-coordinate
space and native encoded byte format. It freezes the original seed color once;
subsequent dabs share the snapshot and use their own seed location. An explicit
fixed-color constructor supports a separately captured original seed. Unsupported
linear or higher precision input is rejected, never silently quantized.

`Fill::Search` copies its search mask and clips its scanline traversal to the
requested rectangle and snapshot extent. Four-connectivity is used during
traversal, so diagonal contact is not a path. Mask coverage changes the old color
distance BEFORE the threshold test:

    distance = (mask * distance + (255 - mask) * 255) / 255

Partial coverage must not be treated as a boolean barrier or only clipped after
a global flood. In particular, threshold255 can traverse zero mask coverage.
Antialiasing uses the original float arithmetic, and current dab seed alpha
decides whether alpha-only transparent selection is enabled even when the frozen
stroke-origin color differs. Fill Brush's fixed threshold remains30; this generic
entry also covers the selection boundary contract's other thresholds.

One-pixel grow is the old radius1 elliptical kernel, which rounds to all nine
neighbors. It can expand outside the search rectangle by one pixel, clipped to
the snapshot extent. This is intentional and distinct from letting the search
escape its rectangle. Empty/outside seeds return empty masks instead of making
unchecked legacy tile accesses.

## Ownership and execution limits

No alternative GObject implementation/handle mechanism is introduced. The engine
is a plain C++ value job using common typed ObjectRef ownership; a native owner
will own it through the existing BindingStore. There are no callbacks here.
Only a completed mask is exposed. Its borrowed buffer is immutable by contract;
callers must retain it before releasing the job and must not mutate it.

`step(n)` performs at most n candidate/grow pixels and permits arbitrary pause
boundaries without changing output. `cancel()` only marks cancellation (O(1));
no partial output becomes available and subsequent steps do nothing. Destruction
reclaims resources separately. Eight64x64 cached tiles per raster bound this
C++ tile-cache memory, not total memory: the pending scanline frontier can grow,
GEGL has its own storage/cache, and snapshot/mask duplication, tile eviction I/O,
allocation, begin-grow flush and destruction are outside the pixel-work bound.
Do not call this full UI nonblocking or large-image admission complete. Real
preparation/step/cancel/end-to-end latency, adversarial frontier bounds/spill and
native owner scheduling remain WBS27.010 gates.

## Remaining integration

This checkpoint does not connect the registered brush tool, transformed native
brush mask, image projection flush/snapshot lifecycle, drawable offsets, native
selection policy, paint/erase compositing, dynamics/opacity/constant mode, stroke
Undo/cancel or editor settings. Full old brush paint/Undo/rate-effect fixtures,
GUI/tablet/platform verification and high-precision formats remain open. The
source old Fill Brush passes its transformed brush mask directly to the bounded
search; ordinary bucket selection uses the separate `_by_seed_ext` path. Native
integration must preserve and test that distinction, not invent an extra implicit
selection multiplication while claiming exact old behavior.
