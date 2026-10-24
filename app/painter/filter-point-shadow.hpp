/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_POINT_SHADOW_HPP
#define GIMP_PAINTER_FILTER_POINT_SHADOW_HPP

#include "filter-context.hpp"
#include "filter-color-exchange.hpp"
#include "filter-colorify-kernel.hpp"
#include "filter-video-kernel.hpp"
#include "filter-raster.hpp"
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class FilterProgress;

/* An outside, nonempty start selection returns before the old plug-in merge.
 * Its private output is still initialized; no_merge forbids merging zeros. */
enum class FilterPointShadowOperation { threshold_alpha, max_rgb, value_invert, red_eye };

enum class FilterPointShadowOutcome { cancelled, shadow, no_merge };

/* Allocation-free scalar preflight; no raster or native state is accessed.
 * Full dimensions must be positive INT32 and the RGBA8 extent fit INT64_MAX.
 * Every positive ROI dimension is supported, including 1x1, 1xN and Nx1, but
 * ROI width*height must not exceed INT32_MAX: the old iterator's total_area
 * and Value Invert's own width*height use signed gint multiplication, which
 * is undefined above that boundary. An outside ROI
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
 * to reproduce any old plug-in's tile-progress callback cadence.
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

/* Red Eye additionally uses the original signed-arithmetic domain validated
 * before allocation or I/O; only the captured RGB owner route may admit it.
 *
 * Operation-tagged raw shadow. Max RGB and Value Invert transform all RGB
 * bytes, including hidden colors, and copy alpha. Value Invert ignores the
 * argument; its public legacy procedure has no operation parameter. Owner
 * admission remains separate: neither Gray nor indexed is supported by these
 * RGB operations. All private-output/ROI rules above still apply. */
FilterPointShadowOutcome filter_point_shadow_raster (
  FilterRaster& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, FilterPointShadowOperation operation,
  std::int32_t argument, std::atomic<bool>& cancel, FilterRaster& output,
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

FilterPointShadowOutcome filter_point_shadow_vector (
  const std::vector<std::uint8_t>& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, FilterPointShadowOperation operation,
  std::int32_t argument, std::atomic<bool>& cancel,
  std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Video captures options and ROI before callbacks; absolute drawable-local
 * origin comes from the ROI, never image offsets or crop-relative zero. */
FilterPointShadowOutcome filter_point_shadow_raster(FilterRaster&,
  std::size_t,std::size_t,const FilterSelectionRegion&,const FilterVideoOptions&,
  std::atomic<bool>&,FilterRaster&,const std::shared_ptr<FilterProgress>& progress = {});
FilterPointShadowOutcome filter_point_shadow_vector(const std::vector<std::uint8_t>&,
  std::size_t,std::size_t,const FilterSelectionRegion&,const FilterVideoOptions&,
  std::atomic<bool>&,std::vector<std::uint8_t>&,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Colorify uses the original iterator's checked signed selected-area rule,
 * and the same sole zero-fill/CropRaster transaction. Kernel is validated and
 * immutable before any output or raster callbacks, including no_merge. */
FilterPointShadowOutcome filter_point_shadow_raster(FilterRaster&,
  std::size_t,std::size_t,const FilterSelectionRegion&,const FilterColorifyKernel&,
  std::atomic<bool>&,FilterRaster&,const std::shared_ptr<FilterProgress>& progress = {});
FilterPointShadowOutcome filter_point_shadow_vector(const std::vector<std::uint8_t>&,
  std::size_t,std::size_t,const FilterSelectionRegion&,const FilterColorifyKernel&,
  std::atomic<bool>&,std::vector<std::uint8_t>&,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Color Exchange uses its original RGB3/RGBA4 signed row bound, not the
 * unrelated old iterator crop-area limit. RGBA8 transport remains checked64bit.
 * Same sole private zero-fill/CropRaster transaction and no_merge semantics. */
void filter_color_exchange_shadow_validate(std::size_t,std::size_t,
  const FilterSelectionRegion&,FilterColorExchangeLayout);
FilterPointShadowOutcome filter_point_shadow_raster(FilterRaster&,
  std::size_t,std::size_t,const FilterSelectionRegion&,const FilterColorExchangeOptions&,
  FilterColorExchangeLayout,std::atomic<bool>&,FilterRaster&,
  const std::shared_ptr<FilterProgress>& progress = {});
FilterPointShadowOutcome filter_point_shadow_vector(const std::vector<std::uint8_t>&,
  std::size_t,std::size_t,const FilterSelectionRegion&,const FilterColorExchangeOptions&,
  FilterColorExchangeLayout,std::atomic<bool>&,std::vector<std::uint8_t>&,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Normalize uses the sole CropRaster/zero-fill backend. Statistics visit only
 * actual ROI bytes. Undefined nonempty input throws after possible PRIVATE
 * zero-fill, never authorizing publication; vectors retain prior output.
 * Nonintersecting selection is ordinary no_merge, not old undefined math. */
FilterPointShadowOutcome filter_normalize_shadow_raster(FilterRaster&,
  std::size_t,std::size_t,const FilterSelectionRegion&,std::atomic<bool>&,
  FilterRaster&,const std::shared_ptr<FilterProgress>& progress = {});
FilterPointShadowOutcome filter_normalize_shadow_vector(const std::vector<std::uint8_t>&,
  std::size_t,std::size_t,const FilterSelectionRegion&,std::atomic<bool>&,
  std::vector<std::uint8_t>&,const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
