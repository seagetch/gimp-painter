/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Standalone first-include and include-guard probe. Keep other project
 * headers out of this translation unit so dependencies cannot be hidden. */
#include "filter-sobel.hpp"
#include "filter-sobel.hpp"
#include <type_traits>

static_assert (std::is_same<decltype (GimpPainter::SobelOptions::horizontal),
                            std::int32_t>::value, "horizontal retains INT32");
static_assert (std::is_same<decltype (GimpPainter::SobelOptions::vertical),
                            std::int32_t>::value, "vertical retains INT32");
static_assert (std::is_same<decltype (GimpPainter::SobelOptions::keep_sign),
                            std::int32_t>::value, "keep_sign retains INT32");
static_assert (std::is_enum<GimpPainter::FilterSobelShadowOutcome>::value,
               "Sobel outcome declaration is independently available");
