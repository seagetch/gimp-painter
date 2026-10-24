/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_COLORIFY_KERNEL_HPP
#define GIMP_PAINTER_FILTER_COLORIFY_KERNEL_HPP

#include <array>
#include <cmath>
#include <cfenv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace GimpPainter {
/* Explicit private native value, not an old boxed GimpRGB/XCF wire decoder.
 * Alpha is retained as metadata but the historical operation ignores it. */
struct FilterColorifyColor { double r, g, b, a; };

class FilterColorifyKernel final
{
  using Channels = std::array<std::array<std::uint8_t, 256>, 3>;
  struct Tables { Channels luminance, tint; };
  const FilterColorifyColor color_;
  const Tables tables_;

  static Tables make_tables (const FilterColorifyColor& color)
  {
    for (double component : {color.r, color.g, color.b, color.a})
      if (!std::isfinite (component) || component < 0.0 || component > 1.0)
        throw std::invalid_argument ("Colorify requires finite RGBA components in [0,1]");
    if (std::fegetround () != FE_TONEAREST)
      throw std::invalid_argument ("Colorify requires round-to-nearest floating arithmetic");
    Tables tables {};
    const double coefficients[] = {0.2126, 0.7152, 0.0722};
    const double tint[] = {color.r, color.g, color.b};
    for (std::size_t channel = 0; channel < 3; ++channel)
      for (std::size_t value = 0; value < 256; ++value)
        {
          /* Two separate integer truncations are the pinned byte contract.
           * Combining luminance before truncation changes even white pixels. */
          tables.luminance[channel][value] = static_cast<std::uint8_t> (
            static_cast<int> (value * coefficients[channel]));
          tables.tint[channel][value] = static_cast<std::uint8_t> (
            static_cast<int> (value * tint[channel]));
        }
    return tables;
  }
public:
  explicit FilterColorifyKernel (FilterColorifyColor color)
    : color_ (color), tables_ (make_tables (color)) {}

  const FilterColorifyColor& color () const noexcept { return color_; }

  std::uint8_t tint_component (std::size_t channel, std::size_t luminance) const
  {
    if (channel >= 3 || luminance >= 256)
      throw std::out_of_range ("Invalid Colorify lookup index");
    return tables_.tint[channel][luminance];
  }

  /* One RGB/RGBA pixel. All checks and reads precede publication; exact or
   * partially overlapping aliases are safe; alpha is preserved and hidden RGB
   * is processed normally. */
  void pixel (const std::uint8_t *input, std::uint8_t *output,
              std::size_t channels) const
  {
    if ((channels != 3 && channels != 4) || !input || !output)
      throw std::invalid_argument ("Colorify requires an RGB/RGBA pixel");
    const unsigned luminance = tables_.luminance[0][input[0]] +
                               tables_.luminance[1][input[1]] +
                               tables_.luminance[2][input[2]];
    std::uint8_t result[4] = {tables_.tint[0][luminance],
                              tables_.tint[1][luminance],
                              tables_.tint[2][luminance], 0};
    if (channels == 4) result[3] = input[3];
    std::memcpy (output, result, channels);
  }
};
}
#endif
