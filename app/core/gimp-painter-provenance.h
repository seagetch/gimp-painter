/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PROVENANCE_H
#define GIMP_PAINTER_PROVENANCE_H
#include <glib-object.h>
/* Immutable import provenance, never current model/definition state. This
 * lower-level helper has no dependency on a file-format module or C++ store. */
static inline void
gimp_painter_copy_provenance (GObject *source, GObject *target)
{
  const gchar *bytes_keys[] = { "gimp-painter-xcf-property-records",
                                "gimp-painter-xcf-extension",
                                "gimp-painter-xcf-object-header" };
  const gchar *text_keys[] = { "gimp-painter-xcf-original-name",
                               "gimp-painter-xcf-original-type" };
  guint i;
  GPtrArray *records;
  for (i = 0; i < G_N_ELEMENTS (bytes_keys); ++i)
    {
      GBytes *bytes = (GBytes *) g_object_get_data (source, bytes_keys[i]);
      if (bytes) g_object_set_data_full (target, bytes_keys[i], g_bytes_ref (bytes), (GDestroyNotify) g_bytes_unref);
    }
  for (i = 0; i < G_N_ELEMENTS (text_keys); ++i)
    {
      const gchar *text = (const gchar *) g_object_get_data (source, text_keys[i]);
      if (text) g_object_set_data_full (target, text_keys[i], g_strdup (text), g_free);
    }
  records = (GPtrArray *) g_object_get_data (source, "gimp-painter-xcf-unknown-records");
  if (records) g_object_set_data_full (target, "gimp-painter-xcf-unknown-records",
                                      g_ptr_array_ref (records), (GDestroyNotify) g_ptr_array_unref);
}
#endif
