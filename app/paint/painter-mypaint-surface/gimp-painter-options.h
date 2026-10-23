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
#define GIMP_PAINTER_MYBRUSH_OPTIONS_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, GimpPainterMybrushOptionsClass))
#define GIMP_IS_PAINTER_MYBRUSH_OPTIONS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS))
#define GIMP_PAINTER_MYBRUSH_OPTIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, GimpPainterMybrushOptionsClass))
typedef struct _GimpPainterMybrushOptions GimpPainterMybrushOptions;
typedef struct _GimpPainterMybrushOptionsClass GimpPainterMybrushOptionsClass;
struct _GimpPainterMybrushOptions { GimpPaintOptions parent_instance; gboolean binding_failed; };
struct _GimpPainterMybrushOptionsClass { GimpPaintOptionsClass parent_class; };
GType gimp_painter_mybrush_options_get_type (void) G_GNUC_CONST;
gchar *gimp_painter_mybrush_options_dup_json (GimpPainterMybrushOptions *options, GError **error);
/* NULL GObject painter-settings is the config default (no override); this
 * explicit API still requires non-NULL, authoritative full-resource JSON. */
gboolean gimp_painter_mybrush_options_set_json (GimpPainterMybrushOptions *options, const gchar *json, GError **error);
/* Startup-only reseed after native reset and before persisted options restore.
 * Uses the complete currently selected resource without changing selection or
 * history, clears settings dirty/conflict and preserves the pending draft name.
 * Rejects an unavailable/closed model, inconsistent selection or active commit. */
gboolean gimp_painter_mybrush_options_prepare_restore (GimpPainterMybrushOptions *options, GError **error);
/* Transfer-full draft name, independent of the selected resource's identity.
 * UTF-8 names may be empty while editing. NULL/invalid UTF-8 is rejected.
 * Runtime properties painter-draft-name and painter-name-dirty are read-only
 * and not serialized. Name edits do not emit settings-changed or affect
 * painter-dirty. Settings Reset preserves the independent pending name. */
gchar *gimp_painter_mybrush_options_dup_draft_name (GimpPainterMybrushOptions *options, GError **error);
gboolean gimp_painter_mybrush_options_set_draft_name (GimpPainterMybrushOptions *options, const gchar *name, GError **error);
gboolean gimp_painter_mybrush_options_set_curve (GimpPainterMybrushOptions *options, gint setting, gint input, const GimpVector2 *points, guint count, GError **error);
GArray *gimp_painter_mybrush_options_get_curve (GimpPainterMybrushOptions *options, gint setting, gint input, GError **error);
/* Applies draft settings to the selected resource without renaming it or
 * clearing a pending draft name; filesystem saving is a separate native
 * data-factory action. Rejects an externally changed source. */
gboolean gimp_painter_mybrush_options_commit (GimpPainterMybrushOptions *options, GError **error);
guint gimp_painter_mybrush_options_history_size (GimpPainterMybrushOptions *options);
gchar *gimp_painter_mybrush_options_history_name (GimpPainterMybrushOptions *options, guint index);
gboolean gimp_painter_mybrush_options_restore_history (GimpPainterMybrushOptions *options, guint index, GError **error);
/* Borrow no options through an untyped cast. Returns a new reference or an
 * explicit diagnostic for NULL/wrong/closed context or an unavailable tool.
 * A normal context selects its resource into the canonical shared tool model;
 * an unselected context uses the internal standard. Dirty drafts enter history. */
GimpPainterMybrushOptions *gimp_painter_mybrush_options_ref_for_context (GimpContext *context, GError **error);
G_END_DECLS
#endif
