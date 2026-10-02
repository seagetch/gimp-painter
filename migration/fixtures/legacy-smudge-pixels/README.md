# Genuine legacy Smudge pixel kernels

The pinned old paint-funcs archive executes1,024 cases across Gray, Gray-alpha,
RGB and RGBA for every byte weight0–255. Each57-pixel case records the updated
accumulator, independently shaded output and actual native painting composition.
`blend_region()` updates in place. `shade_region()` never changes that state.
Alpha cases call `combine_regions_replace()`; opaque cases call the old mask
application and `combine_inten_and_inten_a_pixels()` with partial selection.

The distinction between the old PaintCore replacement and the similarly named
layer Replace mode is essential. The former uses double alpha interpolation
with mask*opacity/65536 and preserves blended hidden color when both alphas are
zero. Opaque native painting instead uses signed integer rounding. The new
`painter-smudge/legacy-pixels.hpp` matches all891,487 captured bytes normally
and under ASan/UBSan/float-cast-overflow. This kernel proof alone does not cover
brush geometry, resource lifetime, whole strokes or a registered GUI tool.

Source, linked archive and executable hashes and commands are recorded in
`runtime.json`. The unchanged old implementation is the oracle; the C++ port is
not used to construct expected values. Capture code is GPL-3.0-or-later.
