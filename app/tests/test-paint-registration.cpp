/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "core/gimppaintinfo.h"
void gimp_paint_register (Gimp *, GimpPaintInfo *);
}
#include "paint/gimp-paint.h"
#include "paint/gimp-paint.h"
#include "painter/object-ref.hpp"
namespace GimpPainter {
template<> struct TypeTraits<GimpPaintInfo>
{ static GType type () noexcept { return GIMP_TYPE_PAINT_INFO; } };
}
extern "C" void paint_registration_cpp (Gimp *gimp, GimpPaintInfo *paint_info);
extern "C" void paint_registration_cpp (Gimp *gimp, GimpPaintInfo *paint_info)
{
  auto owner = GimpPainter::ObjectRef<GimpPaintInfo>::adopt (paint_info);
  gimp_paint_register (gimp, owner.release ());
  g_assert_null (owner.get ());
}
