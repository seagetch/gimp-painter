/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_LIFETIME_HPP
#define GIMP_PAINTER_FILTER_LIFETIME_HPP
#include "gimp-painter-visibility.h"
#include <cstddef>
namespace GimpPainter GIMP_PAINTER_PRIVATE {
/* Process-independent resource lifetime. Neither a GObject store nor an owner
 * callback. Tokens are acquired before launch and released by the worker only
 * after private resources/children have been cleaned up. */
class FilterLifetime {
public:
  FilterLifetime () noexcept;
  ~FilterLifetime () noexcept;
  FilterLifetime (const FilterLifetime&) = delete;
  FilterLifetime& operator= (const FilterLifetime&) = delete;
  static void stop_all () noexcept;
  static bool stopping () noexcept;
  static std::size_t pending () noexcept;
};
}
#endif
