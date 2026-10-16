/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_SHARPEN_SHADOW_HPP
#define GIMP_PAINTER_FILTER_SHARPEN_SHADOW_HPP

#include "filter-context.hpp"
#include "filter-sharpen.hpp"
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

/* A selected area wholly outside the drawable has no shadow merge. Its private
 * output is nevertheless initialized to zero, never an invented identity crop. */
enum class FilterSharpenShadowOutcome { cancelled, shadow, no_merge };

/* Scalar preflight for the owner after capturing the region. Successful
 * validation allocates nothing. Throws std::invalid_argument for bad input;
 * does not access raster or native state. */
void filter_sharpen_shadow_validate (std::size_t width, std::size_t height,
                                     const FilterSelectionRegion& region,
                                     const SharpenOptions& options);

/* Worker-only full-drawable RAW shadow. Input/output are tightly packed,
 * straight RGBA8 with exact width*height*4-byte extents and independent backing
 * storage. Direct object aliasing is rejected; callers must also exclude
 * indirect aliases. The captured region is a checked half-open drawable-local
 * rectangle from filter_selection_region(). No selection means the complete
 * drawable; a nonempty selection can have no intersection. Input, region and
 * options must remain immutable until return. No GObject/GEGL/UI state is used.
 *
 * The complete output is zeroed in at most 16-KiB cancellable writes before the
 * cropped legacy kernel runs. Reads stay inside the captured ROI, whose edges
 * are copied exactly. Pixels outside it stay zero, including their alpha, for
 * the owner's later current-selection shadow merge. Component masks, selection
 * weights and alpha lock are deliberately absent from this raw operation.
 * Intersecting tiny/undefined legacy geometries and undefined percentages are
 * rejected, as documented by filter_sharpen_raster(). Full dimensions must be
 * positive INT32 values and their RGBA8 extent must fit signed 64-bit storage.
 * A narrow ROI may lie in a full raster wider than the kernel row-width limit.
 *
 * Invalid arguments throw std::invalid_argument before I/O; storage errors
 * propagate. Cancellation and exceptions may leave partial PRIVATE output.
 * Publish only after shadow/no_merge and owner generation/lifetime checks;
 * no_merge means preserve original pixels, not merge the initialized zeros.
 * Progress covers zero-fill and kernel writes monotonically and reaches one
 * only after successful flush and the final cancellation check. Reset progress
 * before launching a new operation.
 */
FilterSharpenShadowOutcome filter_sharpen_shadow_raster (
  FilterRaster& input,
  std::size_t width,
  std::size_t height,
  const FilterSelectionRegion& region,
  const SharpenOptions& options,
  std::atomic<bool>& cancel,
  FilterRaster& output,
  const std::shared_ptr<FilterProgress>& progress = {});

/* Same contract with transactional vector publication. Output is unchanged on
 * cancellation/exception, and may be the same vector as input. On no_merge it
 * contains initialized zeros; the outcome still forbids a final shadow merge.
 */
FilterSharpenShadowOutcome filter_sharpen_shadow_vector (
  const std::vector<std::uint8_t>& input,
  std::size_t width,
  std::size_t height,
  const FilterSelectionRegion& region,
  const SharpenOptions& options,
  std::atomic<bool>& cancel,
  std::vector<std::uint8_t>& output,
  const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
