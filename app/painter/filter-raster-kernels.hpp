/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_RASTER_KERNELS_HPP
#define GIMP_PAINTER_FILTER_RASTER_KERNELS_HPP
#include "gimp-painter-visibility.h"
#include "filter-edge.hpp"
#include "filter-gauss.hpp"
#include "filter-raster.hpp"

namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Exact legacy kernels over independent worker-owned, straight RGBA8 storage.
 * No application objects or callbacks may be captured by the scratch factory.
 * Input, output and factory rasters must have independent backing storage;
 * direct object aliasing is rejected. Each must have exactly width*height*4
 * bytes. Offset arithmetic is checked in 64 bits, including beyond 4 GiB.
 *
 * Unlike the vector APIs, output is PRIVATE WORKSPACE: cancellation (false)
 * or any exception can leave it partial. Publish it only after true AND the
 * adapter's generation/lifetime checks. I/O/allocation failures propagate.
 * Input remains untouched. No pixel or color conversion occurs here.
 *
 * Edge uses fixed 1024-pixel strips and halo reads, regardless of dimensions.
 * Gaussian uses one full spill scratch only when its vertical axis is enabled.
 * Tiled transpose makes columns contiguous before the exact vertical pass;
 * horizontal follows vertical, with the original-RGB shadow merge last.
 * Heap storage is bounded by a transpose tile or a single legacy scanline and
 * kernel state, never by raster area. Defined legacy line/radius limits apply.
 * The factory must return a non-null, fresh raster of the requested size.
 * Optional progress counts completed work; 1.0 requires a successful flush.
 */
bool filter_edge_raster (FilterRaster& input,
                         std::size_t width, std::size_t height,
                         const EdgeOptions& options,
                         std::atomic<bool>& cancel,
                         FilterRaster& output,
                         const std::shared_ptr<FilterProgress>& progress = {});

bool filter_gauss_raster (FilterRaster& input,
                          std::size_t width, std::size_t height,
                          const GaussOptions& options,
                          std::atomic<bool>& cancel,
                          FilterRaster& output,
                          const FilterRasterFactory& scratch_factory,
                          const std::shared_ptr<FilterProgress>& progress = {});
} // namespace GimpPainter
#endif
