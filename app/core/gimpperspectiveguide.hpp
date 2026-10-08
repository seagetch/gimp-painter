/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PERSPECTIVE_GUIDE_HPP
#define GIMP_PERSPECTIVE_GUIDE_HPP
#include "../painter/gimp-painter-visibility.h"
#include "gimpperspectiveguide.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPerspectiveGuide>
{ static GType type () noexcept { return GIMP_TYPE_PERSPECTIVE_GUIDE; } };
}
#endif
