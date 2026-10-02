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
GBytes *gimp_test_filter_cpp_gauss_reference (const guint8 *native, gsize width, gsize height, guint channels, gint method);
}
#include "core/gimpfilterlayer-handle.hpp"
#include "painter/filter-gauss.hpp"
#include <atomic>
#include <algorithm>
#include <vector>
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

/* Generated large-image transfer reference. The arithmetic itself is already
 * checked independently against the old executable; this tests the live
 * adapter against the existing vector route, not a new old-runtime capture. */
extern "C" GBytes *gimp_test_filter_cpp_gauss_reference (const guint8 *native, gsize width, gsize height,
                                                        guint channels, gint method)
{
  std::vector<std::uint8_t> input (width * height * 4), output;
  for (gsize i = 0; i < width * height; ++i)
    {
      if (channels == 2)
        {
          input[i*4] = input[i*4+1] = input[i*4+2] = native[i*2];
          input[i*4+3] = native[i*2+1];
        }
      else std::copy_n (native+i*4,4,input.data ()+i*4);
    }
  std::atomic<bool> cancel {false};
  g_assert_true (GimpPainter::filter_gauss (input,width,height,{25.0,25.0,method},cancel,output));
  if (channels == 2)
    {
      std::vector<std::uint8_t> gray (width*height*2);
      for (gsize i = 0; i < width*height; ++i) { gray[i*2] = output[i*4]; gray[i*2+1] = output[i*4+3]; }
      return g_bytes_new (gray.data (),gray.size ());
    }
  return g_bytes_new (output.data (),output.size ());
}
