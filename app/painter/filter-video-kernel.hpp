/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_VIDEO_KERNEL_HPP
#define GIMP_PAINTER_FILTER_VIDEO_KERNEL_HPP
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace GimpPainter {
struct FilterVideoOptions { std::int32_t pattern, additive, rotated; };
/* Explicit immutable drawable-local origin. Neither image layer offsets nor
 * tile/crop resets may change the historical pattern phase. */
class FilterVideoKernel final
{
  const FilterVideoOptions options_;
  const std::int64_t origin_x_, origin_y_;
  static FilterVideoOptions checked (FilterVideoOptions options,
                                     std::int64_t x, std::int64_t y)
  {
    const auto maximum = std::numeric_limits<std::int32_t>::max ();
    if (options.pattern < 0 || options.pattern >= 9 || x < 0 || y < 0 ||
        x > maximum || y > maximum)
      throw std::invalid_argument ("Video requires pattern0..8 and nonnegative signed32 origin");
    return options;
  }
public:
  FilterVideoKernel (FilterVideoOptions options, std::int64_t origin_x,
                    std::int64_t origin_y)
    : options_ (checked (options, origin_x, origin_y)),
      origin_x_ (origin_x), origin_y_ (origin_y) {}
  const FilterVideoOptions& options () const noexcept { return options_; }
  std::int64_t origin_x () const noexcept { return origin_x_; }
  std::int64_t origin_y () const noexcept { return origin_y_; }
  /* Relative coordinates are added to the captured origin after overflow
   * validation. All checks and reads precede writes; partial aliases are safe.
   * Alpha is copied; hidden RGB is processed normally. */
  void pixel (std::int64_t relative_x, std::int64_t relative_y,
              const std::uint8_t *input, std::uint8_t *output,
              std::size_t channels) const
  {
    const auto maximum = std::numeric_limits<std::int32_t>::max ();
    if ((channels != 3 && channels != 4) || !input || !output ||
        relative_x < 0 || relative_y < 0 ||
        relative_x > maximum - origin_x_ || relative_y > maximum - origin_y_)
      throw std::invalid_argument ("Video requires RGB/RGBA and defined signed32 coordinates");
    static const unsigned widths[] = {2,4,1,1,2,3,6,6,5};
    static const unsigned heights[] = {6,12,3,6,12,3,6,18,15};
    /* Immutable original pattern data, independently bound by source oracle. */
    static const char * const patterns[] = {
      "010212102021",
      "001100110022002211221122110011002200220022112211",
      "012",
      "001122",
      "010102021212101020202121",
      "012201120",
      "001122001122220011220011112200112200",
      "220000222002222222222112221111111111001111000110000000000220002222222222112222111221111111111001110000000000",
      "012001112001222001200111220122000122011122012200011201122201120001120112220"
    };
    const auto x = std::uint32_t (origin_x_ + relative_x);
    const auto y = std::uint32_t (origin_y_ + relative_y);
    const auto pattern = unsigned (options_.pattern);
    const auto row = (options_.rotated ? x : y) % heights[pattern];
    const auto column = (options_.rotated ? y : x) % widths[pattern];
    const auto selected = unsigned (patterns[pattern][row * widths[pattern] + column] - '0');
    std::uint8_t result[4] = {};
    for (unsigned channel = 0; channel < 3; ++channel)
      {
        const unsigned selected_value = selected == channel ? input[channel] : 0;
        const unsigned value = selected_value + (options_.additive ? input[channel] : 0);
        result[channel] = std::uint8_t (value > 255 ? 255 : value);
      }
    if (channels == 4) result[3] = input[3];
    std::memcpy (output, result, channels);
  }
};
}
#endif
