/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
}
#include "core/gimpclonelayer-handle.hpp"
#include <cstddef>
#include <type_traits>
extern "C" void gimp_test_clone_cpp_layout (gsize size, gsize offset, GimpCloneLayer *layer);
extern "C" void gimp_test_clone_cpp_layout (gsize size, gsize offset, GimpCloneLayer *layer)
{
  g_assert_cmpuint (sizeof (GimpDrawable), ==, size);
  g_assert_cmpuint (offsetof (GimpDrawable, priv), ==, offset);
  auto handle = GimpPainter::CloneLayerRef::retain (layer);
  auto created = GimpPainter::CloneLayerRef::create (gimp_item_get_image (GIMP_ITEM (layer)),
    nullptr, 2, 2, "C++ factory", 1.0, GIMP_LAYER_MODE_NORMAL);
  g_assert_true (GIMP_IS_CLONE_LAYER (created.get ()));
  g_assert_false (g_object_is_floating (created.get ()));
  auto adopted = GimpPainter::CloneLayerRef::adopt (GIMP_CLONE_LAYER (g_object_ref (layer)));
  auto sunk = GimpPainter::CloneLayerRef::sink (layer);
  static_assert (std::is_same<decltype (handle.source ()), GimpPainter::ObjectRef<GimpLayer>>::value,
                 "Clone source must have typed owning Layer semantics");
  g_assert_true (adopted.get () == layer);
  g_assert_true (sunk.get () == layer);
  handle.set_source_name ("unresolved via C++ handle");
  auto name = handle.source_name ();
  g_assert_cmpstr (name.get (), ==, "unresolved via C++ handle");
  g_assert_false (static_cast<bool> (handle.source ()));
}
