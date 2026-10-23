/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_PLATFORM_ABI_H
#define PAINTER_PLATFORM_ABI_H

#include "gimp-painter-binding.h"

/* Only this test control is deliberately exported; the application bridge
 * remains internal. PE uses its normal explicit-export model. */
#ifdef _WIN32
#define PAINTER_PLATFORM_EXPORT __declspec(dllexport)
#else
#define PAINTER_PLATFORM_EXPORT __attribute__((visibility("default")))
#endif

G_BEGIN_DECLS
typedef gint64 (*PainterPlatformCallback) (GObject *, const GValue *, gint,
                                          gdouble, const gchar *, gpointer);
typedef struct
{
  gsize object_size, object_align, object_ref_count;
  gsize value_size, value_align, value_data;
  gsize error_size, error_align, error_message;
  gsize enum_size, pointer_size, long_size;
} PainterPlatformLayout;

void painter_platform_cpp_layout (PainterPlatformLayout *layout);
gboolean painter_platform_roundtrip (GObject *, PainterPlatformCallback, gpointer,
                                     gint64 *, GError **);
PAINTER_PLATFORM_EXPORT gint painter_platform_export_control (void);
G_END_DECLS
#endif
