/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_H
#define GIMP_PAINTER_MYBRUSH_H
#include "gimpdata.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_MYBRUSH (gimp_painter_mybrush_get_type ())
#define GIMP_PAINTER_MYBRUSH(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_PAINTER_MYBRUSH, GimpPainterMybrush))
#define GIMP_IS_PAINTER_MYBRUSH(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_PAINTER_MYBRUSH))
#define GIMP_PAINTER_MYBRUSH_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_PAINTER_MYBRUSH, GimpPainterMybrushClass))
#define GIMP_IS_PAINTER_MYBRUSH_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_PAINTER_MYBRUSH))
#define GIMP_PAINTER_MYBRUSH_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_PAINTER_MYBRUSH, GimpPainterMybrushClass))
typedef struct _GimpPainterMybrush GimpPainterMybrush;
typedef struct _GimpPainterMybrushClass GimpPainterMybrushClass;
struct _GimpPainterMybrush { GimpData parent_instance; gboolean binding_failed; };
struct _GimpPainterMybrushClass { GimpDataClass parent_class; };
GType gimp_painter_mybrush_get_type (void) G_GNUC_CONST;
GimpData *gimp_painter_mybrush_new (GimpContext *context, const gchar *name);
GimpData *gimp_painter_mybrush_get_standard (GimpContext *context);
GList *gimp_painter_mybrush_load (GimpContext *context, GFile *file, GInputStream *input, GError **error);
gchar *gimp_painter_mybrush_dup_json (GimpPainterMybrush *brush, GError **error);
gboolean gimp_painter_mybrush_set_json (GimpPainterMybrush *brush, const gchar *json, GError **error);
gboolean gimp_painter_mybrush_set_icon (GimpPainterMybrush *brush, GdkPixbuf *icon, GError **error);
GdkPixbuf *gimp_painter_mybrush_ref_icon (GimpPainterMybrush *brush);
/* Returns diagnostics as owned text. Unknown data is never discarded. */
gchar *gimp_painter_mybrush_dup_diagnostics (GimpPainterMybrush *brush);
G_END_DECLS
#endif
