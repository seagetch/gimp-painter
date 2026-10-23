/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_GIO_TYPE_TRAITS_HPP
#define GIMP_PAINTER_GIO_TYPE_TRAITS_HPP
#include "gimp-painter-visibility.h"
#include <gio/gio.h>
#include "object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GFile>
{ static GType type () noexcept { return G_TYPE_FILE; } };
template<> struct TypeTraits<GFileInfo>
{ static GType type () noexcept { return G_TYPE_FILE_INFO; } };
template<> struct TypeTraits<GFileEnumerator>
{ static GType type () noexcept { return G_TYPE_FILE_ENUMERATOR; } };
template<> struct TypeTraits<GFileOutputStream>
{ static GType type () noexcept { return G_TYPE_FILE_OUTPUT_STREAM; } };
}
#endif
