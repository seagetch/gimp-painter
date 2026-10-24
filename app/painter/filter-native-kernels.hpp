/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_NATIVE_KERNELS_HPP
#define GIMP_PAINTER_FILTER_NATIVE_KERNELS_HPP
#include "gimp-painter-visibility.h"
#include "filter-raster-kernels.hpp"
#include "filter-colorify-kernel.hpp"
#include "filter-video-kernel.hpp"
#include <vector>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
enum class FilterPoint { value_invert, max_rgb, threshold_alpha };
/* Explicit point mappings. Bytes retain old whole-drawable shadow semantics;
 * real samples are the normalized-double modern extension, never byte-oracle
 * claims. A real pixel is four native-TRC, straight doubles packed by memcpy. */
bool filter_point_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                           FilterPoint, int argument, bool real, std::atomic<bool>&,
                           const std::shared_ptr<FilterProgress>& progress = {});
/* Immutable validated Colorify LUT snapshot; private tightly packed RGBA8.
 * RGB inputs are promoted with alpha255 by the owner. Hidden RGB is processed;
 * source alpha is unchanged. No native owner/precision admission implied. */
/* RGBA8 cropped transport. Kernel carries immutable drawable-local origin;
 * every row/chunk uses its global raster offset to preserve pattern phase. */
bool filter_video_raw_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
  const FilterVideoKernel&, std::atomic<bool>&,
  const std::shared_ptr<FilterProgress>& progress = {});
bool filter_colorify_raw_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                                  const FilterColorifyKernel&, std::atomic<bool>&,
                                  const std::shared_ptr<FilterProgress>& progress = {});
/* Legacy Max RGB callback output before REPLACE_INTEN merging. RGB is
 * transformed even when alpha is zero; alpha is copied unchanged. Positive
 * mode selects maxima, zero/negative selects minima, with all ties retained.
 * Private independent tightly packed RGBA8 rasters only; no owner merge. */
bool filter_max_rgb_raw_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                                std::int32_t mode, std::atomic<bool>&,
                                const std::shared_ptr<FilterProgress>& progress = {});
/* Legacy vinvert_render_row output before REPLACE_INTEN merging. Each RGB
 * byte c becomes ((255-M)*c + M/2)/M for M=max(R,G,B)>0 with unequal channels;
 * black/achromatic RGB becomes 255-M. Alpha is copied even at alpha zero,
 * where hidden RGB is still transformed. Private independent tightly packed
 * RGBA8 transport only; the caller promotes RGB with alpha 255. No Gray or
 * indexed admission, owner merge, or normalized-real API is implied. */
bool filter_value_invert_raw_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                                     std::atomic<bool>&,
                                     const std::shared_ptr<FilterProgress>& progress = {});
/* Literal old Red Eye raw callback, nonlinear U8 RGB/RGBA only. Alpha and
 * hidden RGB follow old scalar arithmetic; no owner merge or real extension.
 * The declared INT32 PDB range includes undefined old signed arithmetic:
 * reject values outside the universally defined interval before any I/O. */
bool filter_red_eye_threshold_valid (std::int32_t) noexcept;
bool filter_red_eye_raw_raster (FilterRaster&, FilterRaster&, std::size_t, std::size_t,
                                 std::int32_t threshold, std::atomic<bool>&,
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
