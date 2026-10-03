/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_CONTEXT_HPP
#define GIMP_PAINTER_FILTER_CONTEXT_HPP

#include <cstddef>
#include <cstdint>

namespace GimpPainter {

/* Scalar snapshots only. These types never retain a GObject, GEGL object,
 * selection buffer or callback. Bounds are half-open image coordinates. */
struct FilterSelectionBounds
{
  bool empty = true;             // globally empty means unrestricted
  std::int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
};

struct FilterSelectionRegion
{
  bool selected = false;         // distinct from an outside, empty intersection
  bool intersects = false;
  std::int32_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
};

/* Bounded owner-side discovery when the image selection's cached bounds are
 * unknown. Feed contiguous chunks of the complete image mask. No synchronous
 * whole-mask query or full-size allocation is needed. The owner is responsible
 * for a stable snapshot/revision and calls reset after a selection edit. */
class FilterSelectionScan
{
public:
  bool reset (std::int32_t width, std::int32_t height) noexcept;
  bool append (std::uint64_t offset, const std::uint8_t *coverage,
               std::size_t count) noexcept;
  bool finish (FilterSelectionBounds& bounds) const noexcept;
  std::uint64_t consumed () const noexcept { return consumed_; }
private:
  FilterSelectionBounds bounds_;
  std::int32_t width_ = 0, height_ = 0;
  std::uint64_t consumed_ = 0;
};

/* Old gimp_item_mask_bounds coordinates, with a separate intersect result.
 * Wide intermediates avoid old signed-overflow hazards at offset boundaries.
 * Returns false for nonpositive dimensions or inconsistent nonempty bounds. */
bool filter_selection_region (const FilterSelectionBounds& selection,
                              std::int32_t offset_x, std::int32_t offset_y,
                              std::int32_t width, std::int32_t height,
                              FilterSelectionRegion& region) noexcept;

/* Old REPLACE_INTEN byte merge, including integer rounding, alpha-zero hidden
 * colors, and inactive-alpha color ratio. Alpha-bearing RGBA and YA are the
 * live FilterLayer formats; RGB/Y are accepted for independent native probes.
 * active_components bit i controls byte i. Target alpha lock is applied by the
 * owner when capturing that mask; ancestor locks are deliberately irrelevant.
 * A null selection means all 255. Opacity is an exact byte (shadow merge: 255).
 *
 * Allocation-free and noexcept; output may equal original or shadow exactly.
 * Other overlaps, especially with the selection, are unsupported. Invalid
 * arguments return false before writing. Caller retains all memory ownership.
 */
bool filter_replace_inten_row (const std::uint8_t *original,
                              const std::uint8_t *shadow,
                              const std::uint8_t *selection,
                              std::uint8_t       *output,
                              std::size_t        pixels,
                              unsigned           channels,
                              unsigned           active_components,
                              std::uint8_t       opacity = 255) noexcept;

} // namespace GimpPainter
#endif
