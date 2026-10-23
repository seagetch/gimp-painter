# Native brush interpolation arithmetic

The mask and RGB pixmap bilinear kernels used signed `gint` weights. With
12 fractional bits, a weighted horizontal sum is at most 255 * 4096 =
1,044,480. Multiplying by an opposing weight of 2304 exceeds INT32_MAX.
The complete nonnegative numerator is bounded by 255 * 4096 * 4096 =
4,278,190,080, which fits unsigned 32 bits (4,294,967,295).

Use `guint` for the four weights in both kernels. Usual arithmetic conversions
then keep products and sums unsigned. No formula, rounding, clamping, final
24-bit shift, signed coordinate, loop counter or legacy renderer is changed.
Unsigned arithmetic is safe here because the mathematical bound prevents wrap;
this does not merely hide an overflowing computation.

`python3 migration/tests/test_brush_interpolation_arithmetic.py` extracts the
production expressions and weight declarations, compiles them with UBSan, and
compares all 4096-by-4096 fractional pairs for three extreme patterns plus one
million deterministic mixed samples per kernel to an independent 64-bit result.
It also checks that the three RGB expressions agree after channel renaming.
The unchanged source reproduces signed overflow; repaired source passes
102,663,296 comparisons. This is arithmetic-kernel proof, not full-app ASan.

Seven ordinary Meson registrations also passed, including native Fill core,
three old-pixel Fill comparisons, brush geometry, strokes and pixmap oracles.
See `../tests/brush-interpolation/validation.json` and adjacent logs. Full
standard application sanitizer acceptance and WBS 08.012 remain open.
