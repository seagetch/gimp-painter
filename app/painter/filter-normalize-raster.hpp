/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_NORMALIZE_RASTER_HPP
#define GIMP_PAINTER_FILTER_NORMALIZE_RASTER_HPP
#include "filter-raster.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
class FilterProgress;
enum class FilterNormalizeOutcome { complete, cancelled, empty };
/* Compact actual-ROI RGBA8 only. Use sole point-shadow CropRaster for region
 * integration; do not include zero-filled padding in this scan. Independent
 * input stays immutable across both passes; indirect aliases are excluded by
 * caller. Positive area <= INT32_MAX preserves old iterator arithmetic.
 * Empty exact-sized rasters return empty without I/O, not an invented old
 * normalization result. Unsupported nonempty statistics throw before output
 * writes. Cancellation/storage errors may leave PARTIAL PRIVATE output: only
 * complete permits publication after separate owner/generation validation.
 * Throws on geometry, direct alias, extent mismatch, undefined old domain or
 * storage failure. No native owner, GObject, history or ROI backend lives here. */
FilterNormalizeOutcome filter_normalize_raw_raster(
  FilterRaster&,FilterRaster&,std::size_t width,std::size_t height,
  std::atomic<bool>&,
  const std::shared_ptr<FilterProgress>& progress = {});
}
#endif
