/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_EDGE_HPP
#define GIMP_PAINTER_FILTER_EDGE_HPP

#include "gimp-painter-visibility.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace GimpPainter GIMP_PAINTER_PRIVATE {

class FilterProgress;

/* Numeric values are the legacy plug-in-edge PDB arguments. */
struct EdgeOptions
{
  double amount = 2.0;
  int wrapmode = 2;              // 1: wrap, 2: smear, 3: black
  int edgemode = 0;              // Sobel, Prewitt, Gradient, Roberts,
                                // Differential, Laplace
};

/* Compatibility execution of legacy plug-in-edge over a complete drawable.
 * Input is tightly packed, straight (not premultiplied) RGBA8. The RGB bytes
 * are filtered independently; fully transparent output pixels retain their
 * original RGB, as in the legacy shadow merge. Transparent neighbors still
 * contribute their stored RGB to the kernel. Alpha is copied unchanged.
 * No color conversion or selection compositing occurs here.
 *
 * Throws std::invalid_argument for zero/overflowing dimensions, a mismatched
 * byte count, unsupported modes, or a nonfinite amount. Finite amounts below
 * 1.0 are clamped to 1.0, as in the legacy plug-in. Allocation errors propagate.
 * Returns false if cancellation is observed; otherwise publishes the complete
 * result and returns true. Output is unchanged on cancellation or exception,
 * and may alias input. Callers must keep input and options immutable until
 * return and must not concurrently access output. Only cancel and the optional
 * progress mailbox are shared with the controlling thread; no GObject or
 * borrowed application state is used. Progress is normalized work completion.
 */
bool filter_edge (const std::vector<std::uint8_t>& input,
                  std::size_t width,
                  std::size_t height,
                  const EdgeOptions& options,
                  std::atomic<bool>& cancel,
                  std::vector<std::uint8_t>& output,
                  const std::shared_ptr<FilterProgress>& progress = {});

} // namespace GimpPainter
#endif
