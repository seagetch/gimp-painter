/* SPDX-License-Identifier: GPL-3.0-or-later
 * Byte equations derived from pinned GIMP Color Exchange (afa43fae).
 * Private pure kernel; native owner/ROI/shadow merge admission is separate.
 */
#ifndef GIMP_PAINTER_FILTER_COLOR_EXCHANGE_PRIVATE_HPP
#define GIMP_PAINTER_FILTER_COLOR_EXCHANGE_PRIVATE_HPP
#include "gimp-painter-visibility.h"
#include <array>
#include <cstdint>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
struct FilterColorExchangeOptions {
  std::array<std::uint8_t,3> from{};
  std::array<std::uint8_t,3> to{};
  std::array<std::uint8_t,3> threshold{};
};
/* Old INT8 arguments are guint8, not signed char. Each channel's inclusive
 * distance test participates in one RGB gate. Old matching output adds the
 * ABSOLUTE distance to 'to' (not the signed difference), saturating at255.
 * Alpha, including zero/hidden RGB, is independent and copied unchanged.
 * RGB callers may promote alpha255; Gray/indexed/real formats are not admitted
 * by this scalar kernel. No allocation, callbacks, persistence or GEGL. */
inline std::array<std::uint8_t,4>
filter_color_exchange_pixel(std::array<std::uint8_t,4> pixel,
                            const FilterColorExchangeOptions& options) noexcept
{
  std::array<unsigned,3> distance{};
  for(unsigned c=0;c<3;++c) {
    const int difference=int(pixel[c])-int(options.from[c]);
    distance[c]=unsigned(difference<0?-difference:difference);
    if(distance[c]>options.threshold[c])return pixel;
  }
  for(unsigned c=0;c<3;++c) {
    const unsigned value=unsigned(options.to[c])+distance[c];
    pixel[c]=std::uint8_t(value>255?255:value);
  }
  return pixel;
}
}
#endif
