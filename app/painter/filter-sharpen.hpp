/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_SHARPEN_HPP
#define GIMP_PAINTER_FILTER_SHARPEN_HPP

#include "gimp-painter-visibility.h"
#include "filter-raster.hpp"

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class FilterProgress;

/* The original plug-in-sharpen INT32 argument, not the dialog's 1..99 range.
 * Values >= 99 have the same legacy factor of one. */
struct SharpenOptions
{
  std::int32_t percent = 10;
};

/* Exact legacy RAW shadow calculation over an already-cropped rectangular
 * selection intersection. Input/output are tightly packed, straight RGBA8.
 * Each RGB channel is independent; native Gray may be replicated into RGB by
 * the caller without a color conversion. All alpha bytes and the rectangle's
 * first/last rows and columns are copied unchanged. Transparent neighbors and
 * centers participate normally: hidden RGB is filtered even when alpha is 0.
 * Final selection/component/alpha shadow merging belongs to the owner adapter,
 * not this kernel. No live FilterLayer route is enabled by this foundation.
 *
 * Supported dimensions: 2 <= width <= INT32_MAX/4, 3 <= height <= INT32_MAX.
 * Width one reaches out-of-bounds legacy row copies; legacy heights one/two
 * leave shadow rows unwritten. These cases are explicitly unsupported rather
 * than assigned invented identity results. Percent must be >= 100-INT32_MAX,
 * so the original signed 100-percent subtraction is defined. Negative values
 * within that range retain the old integer arithmetic, not a UI-range clamp.
 * Other precisions/formats are not interpreted or converted by this API.
 *
 * Input and output must have independent backing storage and exactly
 * width*height*4 bytes; direct object aliasing is rejected. Caller-owned source
 * and options must remain immutable until return. This worker-only operation
 * owns no GObject/GEGL/UI state. It uses fixed 1024-pixel strips and a one-pixel
 * halo, no scratch raster and no raster-sized or scanline-sized allocation.
 *
 * Output is PRIVATE WORKSPACE: false (cancellation) or an exception may leave
 * partial output. Publish only after true and the adapter's generation/lifetime
 * checks. Input is never written. Invalid geometry/storage/options throw
 * std::invalid_argument; storage errors propagate. Progress reaches 1 only
 * after successful flush and a final cancellation check.
 */
bool filter_sharpen_raster (FilterRaster& input,
                            std::size_t width,
                            std::size_t height,
                            const SharpenOptions& options,
                            std::atomic<bool>& cancel,
                            FilterRaster& output,
                            const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
