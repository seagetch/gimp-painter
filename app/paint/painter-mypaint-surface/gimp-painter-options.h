/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_OPTIONS_H
#define GIMP_PAINTER_MYBRUSH_OPTIONS_H
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "paint/paint-types.h"
#include "paint/gimppaintoptions.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS (gimp_painter_mybrush_options_get_type ())
#define GIMP_PAINTER_MYBRUSH_OPTIONS(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, GimpPainterMybrushOptions))
#define GIMP_IS_PAINTER_MYBRUSH_OPTIONS(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS))
typedef struct _GimpPainterMybrushOptions GimpPainterMybrushOptions;
typedef struct _GimpPainterMybrushOptionsClass GimpPainterMybrushOptionsClass;
struct _GimpPainterMybrushOptions { GimpPaintOptions parent_instance; gboolean binding_failed; };
struct _GimpPainterMybrushOptionsClass { GimpPaintOptionsClass parent_class; };
GType gimp_painter_mybrush_options_get_type (void) G_GNUC_CONST;
gchar *gimp_painter_mybrush_options_dup_json (GimpPainterMybrushOptions *options, GError **error);
gboolean gimp_painter_mybrush_options_set_json (GimpPainterMybrushOptions *options, const gchar *json, GError **error);
gboolean gimp_painter_mybrush_options_set_curve (GimpPainterMybrushOptions *options, gint setting, gint input, const GimpVector2 *points, guint count, GError **error);
GArray *gimp_painter_mybrush_options_get_curve (GimpPainterMybrushOptions *options, gint setting, gint input, GError **error);
/* Applies the full draft to its selected resource; filesystem saving is a
 * separate native data-factory action. Rejects an externally changed source. */
gboolean gimp_painter_mybrush_options_commit (GimpPainterMybrushOptions *options, GError **error);
guint gimp_painter_mybrush_options_history_size (GimpPainterMybrushOptions *options);
gchar *gimp_painter_mybrush_options_history_name (GimpPainterMybrushOptions *options, guint index);
gboolean gimp_painter_mybrush_options_restore_history (GimpPainterMybrushOptions *options, guint index, GError **error);
/* Borrow no options through an untyped cast. Returns a new reference or an
 * explicit diagnostic for NULL/wrong context or an unavailable painter tool. */
GimpPainterMybrushOptions *gimp_painter_mybrush_options_ref_for_context (GimpContext *context, GError **error);
G_END_DECLS
#endif
