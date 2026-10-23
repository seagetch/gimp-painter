/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "core/gimptoolinfo.h"
void gimp_tools_register (Gimp *, GimpToolInfo *);
}
#include "tools/gimp-tools.h"
#include "tools/gimp-tools.h"
#include "painter/object-ref.hpp"
namespace GimpPainter {
template<> struct TypeTraits<GimpToolInfo>
{ static GType type () noexcept { return GIMP_TYPE_TOOL_INFO; } };
}
extern "C" void tool_registration_cpp (Gimp *gimp, GimpToolInfo *tool_info);
extern "C" void tool_registration_cpp (Gimp *gimp, GimpToolInfo *tool_info)
{
  auto owner = GimpPainter::ObjectRef<GimpToolInfo>::adopt (tool_info);
  gimp_tools_register (gimp, owner.release ());
  g_assert_null (owner.get ());
}
