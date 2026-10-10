/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cstddef>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"
#include "config/config-types.h"
#include "config/gimpgeglconfig.h"
#include "config/gimprc.h"
#include "core/core-types.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
void gimp_test_filter_cpp_config (Gimp *application);
gsize gimp_test_filter_pending_jobs (void);
void gimp_test_filter_cpp_layout (gsize size, gsize offset, GimpFilterLayer *layer);
GBytes *gimp_test_filter_cpp_gauss_reference (const guint8 *native, gsize width, gsize height, guint channels, gint method);
}
#include "core/gimpfilterlayer-handle.hpp"
#include "painter/filter-gauss.hpp"
#include "painter/filter-lifetime.hpp"
#include "config/gimppainterfilterconfig.hpp"
#include <atomic>
#include <algorithm>
#include <vector>
extern "C" gsize gimp_test_filter_pending_jobs (void)
{ return GimpPainter::FilterLifetime::pending (); }
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

extern "C" void gimp_test_filter_cpp_config (Gimp *application)
{
  auto *first = G_OBJECT (g_object_new (GIMP_TYPE_RC,"gimp",application,nullptr));
  auto *second = G_OBJECT (g_object_new (GIMP_TYPE_RC,"gimp",application,nullptr));
  auto pool = GimpPainter::filter_admission_for_config (first);
  auto other = GimpPainter::filter_admission_for_config (second);
  g_assert_true (pool != other);
  g_assert_cmpuint (pool->limits ().spill_bytes, ==, UINT64_C(8)*1024*1024*1024);
  const guint64 boundaries[] = {0, UINT64_C(8)*1024*1024*1024,
                               UINT64_C(9)*1024*1024*1024+123,
                               GIMP_MAX_MEMSIZE, GIMP_MAX_MEMSIZE+1,
                               (UINT64_C(1)<<53)+1, G_MAXUINT64};
  for (guint64 boundary : boundaries)
    {
      g_object_set (first,"painter-filter-spill-size",boundary,nullptr);
      guint64 value = 0;
      g_object_get (first,"painter-filter-spill-size",&value,nullptr);
      g_assert_cmpuint (value, ==, boundary);
      g_assert_cmpuint (pool->limits ().spill_bytes, ==, boundary);
      gchar *saved = gimp_config_serialize_to_string (GIMP_CONFIG (first),nullptr);
      GError *parse_error = nullptr;
      g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (second),saved,-1,nullptr,&parse_error));
      g_assert_no_error (parse_error); g_free (saved);
      g_object_get (second,"painter-filter-spill-size",&value,nullptr);
      g_assert_cmpuint (value, ==, boundary);
      g_assert_cmpuint (other->limits ().spill_bytes, ==, boundary);
    }
  const guint64 bytes = UINT64_C(9)*1024*1024*1024+123;
  g_object_set (first,"painter-filter-spill-size",bytes,nullptr);
  gchar *text = gimp_config_serialize_to_string (GIMP_CONFIG (first),nullptr);
  GError *error = nullptr;
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (second),text,-1,nullptr,&error));
  g_assert_no_error (error); g_free (text);
  g_assert_cmpuint (other->limits ().spill_bytes, ==, bytes);
  auto ticket = pool->request (1,bytes); auto lease = ticket.try_acquire ();
  g_object_set (first,"painter-filter-spill-size",guint64(0),nullptr);
  g_assert_cmpuint (pool->active_spill_bytes (), ==, bytes);
  g_assert_cmpuint (other->limits ().spill_bytes, ==, bytes);
  g_object_run_dispose (first); g_object_run_dispose (first);
  g_assert_true (pool->closed ()); g_assert_false (other->closed ());
  unsigned caught=0; try { pool->request (1); } catch (const std::runtime_error&) { ++caught; }
  g_assert_cmpuint (caught, ==, 1); lease.close ();
  g_assert_cmpuint (pool->active_spill_bytes (), ==, 0);
  g_object_unref (first); g_object_unref (second); g_assert_true (other->closed ());
}
