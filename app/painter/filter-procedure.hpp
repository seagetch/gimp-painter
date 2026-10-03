/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_PROCEDURE_HPP
#define GIMP_PAINTER_FILTER_PROCEDURE_HPP
#include <array>
#include <cstdint>
#include <stdexcept>
namespace GimpPainter {
/* This is the entire untrusted procedure selector. A frame contains no name,
 * executable, path, script, image identity, or GObject. Extend only after a
 * separate source/runtime compatibility audit of a literal bundled route. */
enum class FilterProcedure : std::uint32_t { blinds = 1 };
struct FilterProcedureRequest {
  FilterProcedure procedure = FilterProcedure::blinds;
  std::uint32_t width = 0, height = 0;
  std::int32_t angle = 0, segments = 1, orientation = 0, transparent = 0;
  bool gray = false;
  std::array<std::uint8_t, 4> background {{0, 0, 0, 255}};
  std::uint64_t bytes () const {
    if (procedure != FilterProcedure::blinds || !width || !height ||
        width > 524288 || height > 524288 || angle < 0 || angle > 90 ||
        segments < 1 || segments > 100)
      throw std::invalid_argument ("Unsupported private Filter procedure request");
    return std::uint64_t (width) * height * 4;
  }
};
}
#endif
