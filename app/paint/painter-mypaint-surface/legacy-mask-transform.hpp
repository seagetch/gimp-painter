/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_LEGACY_MASK_TRANSFORM_HPP
#define GIMP_PAINTER_LEGACY_MASK_TRANSFORM_HPP
#include "../../painter/gimp-painter-visibility.h"
#include "gegl-surface.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE { namespace MyPaint {
/* Exact generic bitmap-mask transform. Generated brushes have a distinct
 * historical transform and must not be silently treated as bitmap brushes. */
ShapeMask transform_bitmap_mask (const ShapeMask& source, double scale,
                                 double gimp_aspect_ratio, double turns,
                                 double hardness);
} }
#endif
