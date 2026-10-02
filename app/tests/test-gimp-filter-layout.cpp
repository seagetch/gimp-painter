/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cstddef>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
void gimp_test_filter_cpp_layout (gsize size, gsize offset, GimpFilterLayer *layer);
}
#include "core/gimpfilterlayer-handle.hpp"
extern "C" void gimp_test_filter_cpp_layout (gsize size, gsize offset, GimpFilterLayer *layer)
{
  g_assert_cmpuint (size, ==, sizeof (GimpDrawable));
  g_assert_cmpuint (offset, ==, offsetof (GimpDrawable, priv));
  auto ref = GimpPainter::FilterLayerRef::retain (layer);
  ref.set_definition ("future-procedure", nullptr, nullptr);
  auto name = ref.procedure ();
  g_assert_cmpstr (name.get (), ==, "future-procedure");
  auto created = GimpPainter::FilterLayerRef::create (gimp_item_get_image (GIMP_ITEM (layer)),
                                                     2,2,"typed",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (GIMP_IS_FILTER_LAYER (created.get ()));
}
