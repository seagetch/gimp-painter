/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <json-glib/json-glib.h>
extern "C" {
#include "core/core-types.h"
#include "core/gimplayerpreset.h"
}
#include "core/gimpjsonresource.hpp"
#include "core/gimpjsonresource.hpp"
extern "C" void json_resource_handle_test (void)
{
  using namespace GimpPainter;
  auto owner=JsonResourceRef::create(nullptr,"typed");g_assert_true(bool(owner));
  g_assert_cmpuint(TypeTraits<GimpJsonResource>::type(),==,gimp_json_resource_get_type());
  auto copy=owner;owner.reset();g_assert_cmpstr(gimp_object_get_name(copy.get()),==,"typed");
  auto derived=JsonResourceRef::adopt(GIMP_JSON_RESOURCE(gimp_layer_preset_new(nullptr,"derived")));
  g_assert_true(GIMP_IS_LAYER_PRESET(derived.get()));
  auto *wrong=G_OBJECT(g_object_new(G_TYPE_OBJECT,nullptr));bool rejected=false;
  try{auto bad=JsonResourceRef::retain(reinterpret_cast<GimpJsonResource*>(wrong));}catch(const Error&){rejected=true;}
  g_assert_true(rejected);g_object_unref(wrong);
}
