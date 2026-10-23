/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_EDGE_SHADOW_HPP
#define GIMP_PAINTER_FILTER_EDGE_SHADOW_HPP

#include "filter-context.hpp"
#include "filter-edge.hpp"
#include "filter-raster.hpp"
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

/* These outcomes describe raw ROI writes only, never owner merge disposition.
 * The old Edge empty mask_bounds path still flushes and merges. In particular,
 * no_writes MUST NOT be interpreted as plug-in-sobel's no_merge outcome. */
enum class FilterEdgeShadowOutcome { cancelled, writes, no_writes };

/* Allocation-free scalar preflight, before any raster access. Full dimensions
 * are positive INT32; packed RGBA8 extent fits signed 64-bit storage; ordered
 * drawable-local ROI endpoints agree with intersects and selected. Unselected
 * means the full drawable. ROI area fits INT32, preserving edge.c's signed
 * max_progress product. Modes are 0..5, borders 1..3, amount finite. */
void filter_edge_shadow_validate (std::size_t width, std::size_t height,
                                  const FilterSelectionRegion& region,
                                  const EdgeOptions& options);

/* Worker-only independent raw Edge foundation; no live owner is connected.
 * Input and output are independent tightly packed straight RGBA8 rasters,
 * exactly width*height*4 bytes. Direct aliases are rejected; the caller must
 * exclude indirect backing-storage aliases and keep input, region and options
 * immutable until return. Gray/GrayA promotion replicates native Y into RGB;
 * RGB/Gray without alpha is promoted with alpha255, without color conversion.
 *
 * The complete private output is explicitly initialized to zero, then only
 * ROI pixels are written by the Edge calculation. All six algorithms use the
 * full DRAWABLE halo in both axes, including neighbors outside the ROI, with
 * wrap/smear/black border behavior. Hidden RGB is transformed at alpha zero;
 * source alpha is copied. No selection weight, component mask or alpha lock
 * is applied. A later owner must establish the genuine merge disposition.
 *
 * Finite amounts below 1 clamp to 1. For the defined legacy double-to-gint
 * domain (clamped amount <= INT32_MAX/2040), results match the old truncation
 * then byte-clamp operation. Larger finite amounts use the existing safe
 * byte saturation extension, with no claimed equivalence to undefined old C.
 * Existing filter_edge/filter_edge_raster/normalized-real APIs are unchanged.
 *
 * Scratch is fixed: three (1024+2)*4-byte halo arrays, a 1024*4-byte result
 * strip, a 16384-byte zero buffer and a 9-byte channel kernel (32801 explicit
 * byte-array bytes, plus scalar/frame overhead). Reads <=4104,
 * filtered writes <=4096, zero writes <=16384; no scanline-sized allocation.
 * Cancellation is checked before/after every read/write/flush and within pixel
 * loops. Exceptions propagate; cancellation or exceptions can leave partial
 * PRIVATE output, which must never be published. Progress is monotonic and
 * reaches one only after flush and a final cancellation check. Reset progress
 * before another call. The initialized empty ROI is flushed and returns
 * no_writes, making no assertion about legacy owner merge/no-merge semantics.
 */
FilterEdgeShadowOutcome filter_edge_shadow_raster (
  FilterRaster& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, const EdgeOptions& options,
  std::atomic<bool>& cancel, FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Transactional convenience API. Allocates private full output; publishes only
 * on successful writes/no_writes. Output stays unchanged on failure and may
 * equal input. This wrapper's full-vector admission is the caller's concern. */
FilterEdgeShadowOutcome filter_edge_shadow_vector (
  const std::vector<std::uint8_t>& input, std::size_t width, std::size_t height,
  const FilterSelectionRegion& region, const EdgeOptions& options,
  std::atomic<bool>& cancel, std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
