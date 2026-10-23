/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_SOBEL_HPP
#define GIMP_PAINTER_FILTER_SOBEL_HPP

#include "filter-context.hpp"
#include "filter-raster.hpp"
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class FilterProgress;

/* Original plug-in-sobel INT32 arguments. Every nonzero value is true,
 * including negative values; retain the original bits, not dialog booleans. */
struct SobelOptions
{
  std::int32_t horizontal = 1;
  std::int32_t vertical = 1;
  std::int32_t keep_sign = 1;
};

enum class FilterSobelShadowOutcome { cancelled, shadow, no_merge };

/* Allocation-free scalar preflight. Throws std::invalid_argument for an
 * unsupported full extent or inconsistent captured region, before raster I/O.
 * All INT32 flag values and all positive ROI heights/widths, including 1x1,
 * are supported subject to the legacy width bound described below. */
void filter_sobel_shadow_validate (std::size_t width, std::size_t height,
                                   const FilterSelectionRegion& region,
                                   const SobelOptions& options);

/* Worker-only full-drawable RAW shadow for the pinned plug-in-sobel, distinct
 * from plug-in-edge's Sobel mode. Input/output are tightly packed, straight
 * RGBA8 with exactly width*height*4 bytes and independent backing storage.
 * Direct object aliasing is rejected; callers must exclude indirect aliases.
 * GrayA can be represented by equal RGB channels, with no color conversion.
 * This is an alpha-bearing calculation; it does not claim native RGB/Y alpha
 * semantics. It owns no GObject, GEGL, owner callback or live context state.
 * Input, captured region and options must remain immutable until return.
 *
 * The region is a checked half-open, drawable-local selection intersection
 * from filter_selection_region(). An unselected region is the full drawable.
 * An outside nonempty selection returns no_merge, with zero private output;
 * this means preserve the original, never merge the initialized zeros.
 *
 * Full dimensions are positive INT32 values and their RGBA8 extent must fit
 * signed 64-bit storage. An intersecting ROI width is at most INT32_MAX/4-2:
 * the old (width+2)*4 row allocation otherwise overflows signed gint. A narrow
 * ROI can lie in a wider full raster. Unlike Sharpen, tiny nonempty ROIs are
 * defined. No legacy preview allocation or no-alpha counter is modeled here.
 *
 * Zero-fill covers the complete private shadow, then only the captured ROI
 * is filtered. Horizontal halo endpoints replicate the ROI's first/last
 * pixels, while vertical neighbors clamp against the FULL drawable, and may
 * lie outside the ROI. Hidden RGB participates regardless of input alpha.
 * Alpha is synthesized as 255 if any computed color gradient is greater than
 * 10, otherwise 0; input alpha is not copied. Both disabled flags yield color
 * 127/alpha 255 with keep_sign, and color 0/alpha 0 without it.
 *
 * Preserve legacy ROUND as truncation of value+0.5, even for negative values.
 * With both directions enabled, round the RMS before division by 5.66, then
 * truncate to integer before byte conversion. One-direction keep_sign adds
 * 127 after ROUND(gradient/8.0); absolute mode uses ROUND(abs(gradient)/4.0).
 * No selection weights, components or alpha lock are applied to raw output.
 * The owner must perform the distinct final-context shadow merge later.
 *
 * Scratch is fixed: three (1024+2)-pixel halo rows, one 1024-pixel result
 * strip, and a 16-KiB zero buffer; no scanline/raster-sized scratch allocation.
 * Reads are at most 4104 bytes, filtered writes at most 4096, zero writes at
 * most 16384. Cancellation is checked before/after every read/write/flush and
 * during arithmetic. Storage errors propagate. Cancellation or exceptions may
 * leave partial PRIVATE output; publish only on shadow/no_merge after owner
 * generation/lifetime checks. Progress is monotonic across zero-fill/filter
 * writes and reaches one only after flush and a final cancellation check.
 * Reset progress before a new operation. No native route is enabled here.
 */
FilterSobelShadowOutcome filter_sobel_shadow_raster (
  FilterRaster& input,
  std::size_t width,
  std::size_t height,
  const FilterSelectionRegion& region,
  const SobelOptions& options,
  std::atomic<bool>& cancel,
  FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Same calculation with transactional vector publication. Output is unchanged
 * on cancellation/exception and may be the same vector as input. no_merge
 * publishes initialized zeros, with the outcome still forbidding a merge.
 * Only this vector convenience API allocates full-sized private output. */
FilterSobelShadowOutcome filter_sobel_shadow_vector (
  const std::vector<std::uint8_t>& input,
  std::size_t width,
  std::size_t height,
  const FilterSelectionRegion& region,
  const SobelOptions& options,
  std::atomic<bool>& cancel,
  std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
