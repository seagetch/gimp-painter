/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_PROVENANCE_H
#define GIMP_PAINTER_PROVENANCE_H
#include <glib-object.h>
G_BEGIN_DECLS
/* Fixed, typed value fields. No arbitrary key/type association is accepted.
 * Getters return an independent reference/copy; they never create a component.
 * All calls belong to the native object's main thread. */
typedef enum {
  GIMP_PAINTER_PROVENANCE_PROPERTIES,
  GIMP_PAINTER_PROVENANCE_EXTENSION,
  GIMP_PAINTER_PROVENANCE_HEADER,
  GIMP_PAINTER_PROVENANCE_EXTERNAL,
  GIMP_PAINTER_PROVENANCE_ORIGINAL,
  GIMP_PAINTER_PROVENANCE_N_BYTES
} GimpPainterProvenanceBytes;
typedef enum {
  GIMP_PAINTER_PROVENANCE_NAME,
  GIMP_PAINTER_PROVENANCE_TYPE,
  GIMP_PAINTER_PROVENANCE_N_TEXT
} GimpPainterProvenanceText;
GBytes   *gimp_painter_provenance_ref_bytes (GObject *, GimpPainterProvenanceBytes);
gboolean  gimp_painter_provenance_set_bytes (GObject *, GimpPainterProvenanceBytes, GBytes *);
gchar    *gimp_painter_provenance_dup_text (GObject *, GimpPainterProvenanceText);
gboolean  gimp_painter_provenance_set_text (GObject *, GimpPainterProvenanceText, const gchar *);
/* Arrays are snapshots of GBytes references, never shared mutable containers. */
GPtrArray *gimp_painter_provenance_ref_records (GObject *);
gboolean  gimp_painter_provenance_set_records (GObject *, GPtrArray *);
GVariant *gimp_painter_provenance_ref_definition (GObject *);
gboolean  gimp_painter_provenance_set_definition (GObject *, GVariant *);
gboolean  gimp_painter_provenance_has_definition (GObject *);
gboolean  gimp_painter_provenance_get_offset (GObject *, guint64 *);
gboolean  gimp_painter_provenance_set_offset (GObject *, guint64);
gint      gimp_painter_provenance_get_dialect (GObject *);
gboolean  gimp_painter_provenance_set_dialect (GObject *, gint);
guint32   gimp_painter_provenance_get_save_id (GObject *);
gboolean  gimp_painter_provenance_set_save_id (GObject *, guint32);
/* Copies only immutable import provenance. No whole-file snapshot, current
 * definition, dialect, original offset or last-save ID is transferred. */
void      gimp_painter_copy_provenance (GObject *source, GObject *target);
G_END_DECLS
#endif
