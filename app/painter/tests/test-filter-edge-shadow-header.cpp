/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "filter-edge-shadow.hpp"
#include "filter-edge-shadow.hpp"
#include <type_traits>
static_assert (std::is_same<decltype (GimpPainter::EdgeOptions::amount), double>::value,
               "Edge amount retains binary64");
static_assert (std::is_enum<GimpPainter::FilterEdgeShadowOutcome>::value,
               "Raw Edge outcome is independently declared");
