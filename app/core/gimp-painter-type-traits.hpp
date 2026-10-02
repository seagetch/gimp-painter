/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CORE_PAINTER_TYPE_TRAITS_HPP
#define GIMP_CORE_PAINTER_TYPE_TRAITS_HPP
extern "C" {
#include "gimpclonelayer.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter {
template<> struct TypeTraits<GimpLayer>
{ static GType type () noexcept { return GIMP_TYPE_LAYER; } };
template<> struct TypeTraits<GimpCloneLayer>
{ static GType type () noexcept { return GIMP_TYPE_CLONE_LAYER; } };
}
#endif
