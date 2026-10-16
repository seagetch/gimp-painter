/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_POINT_SHADOW_HPP
#define GIMP_PAINTER_FILTER_POINT_SHADOW_HPP

#include "filter-context.hpp"
#include "filter-raster.hpp"
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class FilterProgress;

/* An outside, nonempty start selection returns before the old iterator merge.
 * Its private output is still initialized; no_merge forbids merging zeros. */
enum class FilterPointShadowOutcome { cancelled, shadow, no_merge };

/* Allocation-free scalar preflight; no raster or native state is accessed.
 * Full dimensions must be positive INT32 and the RGBA8 extent fit INT64_MAX.
 * Every positive ROI dimension is supported, including 1x1, 1xN and Nx1, but
 * ROI width*height must not exceed INT32_MAX: the old iterator's signed
 * total_area multiplication is undefined above that boundary. An outside ROI
 * has zero area. Invalid dimensions/region throw std::invalid_argument. */
void filter_point_shadow_validate (std::size_t width, std::size_t height,
                                   const FilterSelectionRegion& region);

/* Worker-only Threshold Alpha RAW shadow over a captured start intersection.
 * Input/output are tightly packed straight RGBA8 with exact width*height*4
 * extents and independent backing. Gray is replicated into RGB by the caller.
 * Direct object aliasing is rejected; callers must exclude indirect aliases.
 * Input and the checked half-open drawable-local region remain immutable.
 * Plain signed INT32 threshold is unrestricted, including negative and >255;
 * the existing point kernel copies color and writes alpha iff threshold<alpha.
 * No color normalization or hidden-color exception is applied.
 *
 * The full private shadow is zero-filled before processing only the ROI.
 * Transfers to full rasters are at most 16 KiB, split at ROI row boundaries;
 * the existing point kernel uses its fixed 32-KiB stack buffer. No row-sized
 * allocation, scratch raster, GObject, GEGL or owner closure is used. Final
 * selection weights/components/alpha lock belong to the owner-side merge.
 *
 * Invalid arguments throw before I/O; storage errors propagate. Cancellation
 * and exceptions may leave partial PRIVATE output. Publish only after shadow
 * or no_merge and owner generation/lifetime checks; no_merge means retain the
 * original pixels. Progress monotonically covers zero-fill and crop writes,
 * reaches one only after successful flush and a final cancellation check, and
 * must be reset before a new operation. It is transport progress, not a claim
 * to reproduce the old iterator's tile-progress callback cadence.
 */
FilterPointShadowOutcome filter_point_shadow_raster (
  FilterRaster& input,
  std::size_t width,
  std::size_t height,
  const FilterSelectionRegion& region,
  std::int32_t threshold,
  std::atomic<bool>& cancel,
  FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Transactional vector adapter. Output is unchanged on cancellation/exception
 * and may be the same vector as input. On no_merge it contains zeros, but the
 * outcome still forbids merging those bytes into the original drawable. */
FilterPointShadowOutcome filter_point_shadow_vector (
  const std::vector<std::uint8_t>& input,
  std::size_t width,
  std::size_t height,
  const FilterSelectionRegion& region,
  std::int32_t threshold,
  std::atomic<bool>& cancel,
  std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
