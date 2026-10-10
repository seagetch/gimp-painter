/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform-abi.h"
#include "platform-abi.h"
#include <stddef.h>

static gint callback_count;
static gint64
callback (GObject *owner, const GValue *value, gint integer, gdouble real,
          const gchar *text, gpointer context)
{
  g_assert_true (G_IS_OBJECT (owner));
  g_assert_true (owner == context);
  g_assert_cmpint (integer, ==, -714);
  g_assert_cmpfloat (real, ==, 0.375);
  g_assert_cmpstr (text, ==, "C/C++ callback ABI");
  g_assert_true (G_VALUE_HOLDS_INT64 (value));
  g_assert_cmpint (g_value_get_int64 (value), ==, G_GINT64_CONSTANT (0x123456789abc));
  ++callback_count;
  return -G_GINT64_CONSTANT (0x123456789abc);
}

gint painter_platform_export_control (void) { return 714; }

int
main (void)
{
  const PainterPlatformLayout c = {
    sizeof (GObject), _Alignof (GObject), offsetof (GObject, ref_count),
    sizeof (GValue), _Alignof (GValue), offsetof (GValue, data),
    sizeof (GError), _Alignof (GError), offsetof (GError, message),
    sizeof (GimpPainterError), sizeof (gpointer), sizeof (long)
  };
  PainterPlatformLayout cpp;
  GObject *owner = g_object_new (G_TYPE_OBJECT, NULL);
  GError *error = NULL;
  gint64 result = 0;
  painter_platform_cpp_layout (&cpp);
#define SAME(field) g_assert_cmpuint (c.field, ==, cpp.field)
  SAME (object_size); SAME (object_align); SAME (object_ref_count);
  SAME (value_size); SAME (value_align); SAME (value_data);
  SAME (error_size); SAME (error_align); SAME (error_message);
  SAME (enum_size); SAME (pointer_size); SAME (long_size);
#undef SAME
  g_assert_cmpuint (c.pointer_size, ==, 8);
  g_assert_true (painter_platform_roundtrip (owner, callback, owner, &result, &error));
  g_assert_no_error (error);
  g_assert_cmpint (result, ==, -G_GINT64_CONSTANT (0x123456789abc) + 1);
  g_assert_cmpint (callback_count, ==, 1);
  g_assert_true (gimp_painter_binding_close (owner, &error));
  g_assert_no_error (error);
  /* A C entry must contain the closed-store C++ exception on every platform. */
  g_assert_false (painter_platform_roundtrip (owner, callback, owner, &result, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_assert_cmpint (callback_count, ==, 1);
  g_clear_error (&error);
  g_object_unref (owner);
  g_assert_cmpint (painter_platform_export_control (), ==, 714);
  g_print ("PLATFORM_ABI_PASS pointer=%" G_GSIZE_FORMAT " long=%" G_GSIZE_FORMAT
           " object=%" G_GSIZE_FORMAT " value=%" G_GSIZE_FORMAT
           " error=%" G_GSIZE_FORMAT "\n", c.pointer_size, c.long_size,
           c.object_size, c.value_size, c.error_size);
  return 0;
}
