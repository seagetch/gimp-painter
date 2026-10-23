/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_NATIVE_KERNELS_HPP
#define GIMP_PAINTER_FILTER_NATIVE_KERNELS_HPP
#include "filter-raster-kernels.hpp"
#include <vector>
namespace GimpPainter {
enum class FilterPoint { value_invert, max_rgb, threshold_alpha };
/* Explicit point mappings. Bytes retain old whole-drawable shadow semantics;
 * real samples are the normalized-double modern extension, never byte-oracle
 * claims. A real pixel is four native-TRC, straight doubles packed by memcpy. */
bool filter_point_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                           FilterPoint, int argument, bool real, std::atomic<bool>&,
                           const std::shared_ptr<FilterProgress>& progress = {});
bool filter_edge_real_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                               const EdgeOptions&, std::atomic<bool>&,
                               const std::shared_ptr<FilterProgress>& progress = {});
bool filter_gauss_real_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                                const GaussOptions&, std::atomic<bool>&, const FilterRasterFactory&,
                                const std::shared_ptr<FilterProgress>& progress = {});
/* Reuse the exact raster kernels for small vectors without file I/O. The
 * callback owns no GObject. Input cannot be written; scratch is independent. */
using FilterNativeProcess = std::function<bool (FilterRaster&, FilterRaster&,
                                  std::atomic<bool>&, const FilterRasterFactory&)>;
bool filter_native_vector (const std::vector<std::uint8_t>&, std::atomic<bool>&,
                            std::vector<std::uint8_t>&, const FilterNativeProcess&);
}
#endif
