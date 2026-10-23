/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CORE_PAINTER_TYPE_TRAITS_HPP
#define GIMP_CORE_PAINTER_TYPE_TRAITS_HPP
#include "../painter/gimp-painter-visibility.h"
extern "C" {
#include "gimpclonelayer.h"
#include "gimpclonelayerundo.h"
#include "gimpfilterlayer.h"
#include "gimppaintermybrush.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPainterMybrush>
{ static GType type () noexcept { return GIMP_TYPE_PAINTER_MYBRUSH; } };
template<> struct TypeTraits<GimpLayer>
{ static GType type () noexcept { return GIMP_TYPE_LAYER; } };
template<> struct TypeTraits<GimpFilterLayer>
{ static GType type () noexcept { return GIMP_TYPE_FILTER_LAYER; } };
template<> struct TypeTraits<GimpCloneLayerUndo>
{ static GType type () noexcept { return GIMP_TYPE_CLONE_LAYER_UNDO; } };
template<> struct TypeTraits<GimpCloneLayer>
{ static GType type () noexcept { return GIMP_TYPE_CLONE_LAYER; } };
}
#endif
