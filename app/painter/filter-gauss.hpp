/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_GAUSS_HPP
#define GIMP_PAINTER_FILTER_GAUSS_HPP

#include "gimp-painter-visibility.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class FilterProgress;

/* Values are the legacy plug-in-gauss PDB arguments, not GEGL sigma. */
struct GaussOptions
{
  double horizontal = 5.0;
  double vertical = 5.0;
  int method = 1;                // 0: IIR, 1: integer RLE
};

/* Whole-drawable legacy plug-in-gauss compatibility for tightly packed,
 * straight RGBA8. Vertical precedes horizontal; each pass premultiplies and
 * separates alpha using legacy byte rounding. The final shadow merge retains
 * input RGB wherever output alpha is zero. No selection/color conversion.
 * Either radius <= 1 selects RLE for BOTH axes, even if method is IIR.
 * A nonpositive radius disables that axis; at least one must be positive.
 * Below -1, the old region arithmetic also removes border rows/columns from
 * processing. A nonempty region clears excluded alpha (retaining original RGB);
 * an empty region leaves the original pixels unchanged. Checked old-coordinate
 * overflow is rejected. These are full, unselected RGBA drawable semantics.
 *
 * Throws std::invalid_argument for invalid dimensions/byte count, nonfinite
 * radii, unsupported methods, or calculations outside defined legacy limits
 * (32-bit byte/index/integer arithmetic or finite IIR intermediates). Allocation
 * exceptions propagate. Undefined legacy overflow is rejected, not emulated.
 * Returns false on cancellation; publishes output only on complete success.
 * Output stays unchanged on cancellation or exception and may alias input.
 * Only cancel and the optional progress mailbox are shared; callers must keep
 * input/options immutable and must not concurrently access output. No
 * GObject/GEGL or borrowed state is used.
 */
bool filter_gauss (const std::vector<std::uint8_t>& input,
                   std::size_t width,
                   std::size_t height,
                   const GaussOptions& options,
                   std::atomic<bool>& cancel,
                   std::vector<std::uint8_t>& output,
                   const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
