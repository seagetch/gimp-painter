/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_LEGACY_GENERATED_MASK_HPP
#define GIMP_PAINTER_LEGACY_GENERATED_MASK_HPP
#include "../../painter/gimp-painter-visibility.h"
#include "gegl-surface.hpp"
#include <utility>
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
enum class GeneratedShape { Circle, Square, Diamond };
std::pair<int,int> legacy_generated_dimensions (GeneratedShape shape,float radius,int spikes,
                                               float hardness,float aspect,float angle);
ShapeMask generate_legacy_mask (GeneratedShape shape,float radius,int spikes,
                                float hardness,float aspect,float angle);
} }
#endif
