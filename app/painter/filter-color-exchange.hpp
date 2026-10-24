/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_COLOR_EXCHANGE_HPP
#define GIMP_PAINTER_FILTER_COLOR_EXCHANGE_HPP
#include "filter-color-exchange-private.hpp"
#include "filter-raster.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
class FilterProgress;
/* Original owner byte layout, distinct from promoted RGBA8 transport. */
enum class FilterColorExchangeLayout { rgb8, rgba8 };
/* Independent RGBA8 raw shadow. Original RGB owners promote alpha255.
 * Pure typed options; caller validates original owner/ROI before publication.
 * Throws on invalid extent/storage/alias before I/O. Fixed32KiB scratch.
 * Cancellation/errors may leave partial private output, never publish it. */
bool filter_color_exchange_raw_raster(FilterRaster&,FilterRaster&,
  std::size_t width,std::size_t height,const FilterColorExchangeOptions&,
  std::atomic<bool>&,const std::shared_ptr<FilterProgress>& progress = {});
}
#endif
