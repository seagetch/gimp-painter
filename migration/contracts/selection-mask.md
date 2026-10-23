# Selection mask — original 09.006 acceptance

Action: **選択マスクの読取りを接続する**. Acceptance: **オフセットと coverage が一致**.
Dependency: 09.005.

The production `PaintCore::Impl::begin` obtains the current native
`GimpPaintCore::mask_buffer` and the drawable's signed item offset and passes both
to `GeglSurface::set_selection`. Surface translates local pixel `(x,y)` to image
mask `(x+offset_x,y+offset_y)` exactly once. The target extent and translated mask
extent both restrict writes. Coverage is linear scalar Y, not a gamma-encoded
color channel. Byte rendering uses the existing Y-u8 mask conversion; native
high-precision rendering reads the native scalar mask without byte quantization.
`Impl::read` calls BufferRead without an override format; `NativePixel::Format`
decodes U8 with double division by 255 and float/double as their stored scalars.
The wide path does not introduce an intermediate Y-float conversion.
A missing/empty native selection is unrestricted painting, not zero coverage;
a zero pixel inside a nonempty selection excludes that pixel. Layer masks are a
separate 09.007 duty, not silently multiplied in by this selection criterion.

## Actual native integration tests

`test-painter-selection-mask.inc` uses the real GimpImage selection channel,
GimpLayer offsets, current PaintCore and Surface implementations:

* Both incremental and non-incremental strokes are byte-identical under four
  asymmetric signed translations `(0,0),(-7,5),(9,-6),(-5,-4)` when independently
  generated image-space masks describe the same local region. Five interleaved
  coverage levels include zero and fractional values. Every zero-coverage pixel
  retains all original channels; fractional locations actually change.
* Empty selection and full selection are equivalent for both accumulation modes.
* A real native GimpPaintCore start supplies exactly the image's mask buffer.
  Surface retains it after the native transaction is finished without Undo and cleaned up, and the original
  image owner is released.
  A constant-shaped opaque red dab on transparent double storage has an
  independent per-pixel alpha oracle equal to local scalar coverage, including
  fractional levels, zero guards and signed offsets. U8 image masks use the
  independent rounded-byte oracle (64/255,128/255,191/255); float and double
  image masks retain exact 0.25/0.5/0.75. Three image precisions and three signed
  offsets are covered. This separately checks
  exact coverage rather than inferring it solely from translation equivalence.

No production behavior is duplicated in a replacement model and no private
PaintCore test hook is introduced. The simple mask-coordinate oracle is written
independently in the test. This criterion does not claim mask-edit-during-stroke,
all reentrant callbacks, arbitrary huge offsets, or whole-app sanitizer coverage.

## Retained entire-header duty

The single assigned source is the entire 429-line
`gimpmypaintcore-drawablefeature.hpp`, not merely its selection methods. The JSON
retains its exact source identity/range/hash and inherited 09.003/09.004 wording.
Its whole-hunk status remains TODO. Snapshot, allocation, dirty/projection, undo,
commit/cancel, layer-mask, preview, sampling and non-incremental consumers retain
explicit existing owner tasks and reverse links. 31.017 still owns removal of
all old pixel-representation consumers. Bounded selection tests cannot complete
that whole header or those future obligations.
