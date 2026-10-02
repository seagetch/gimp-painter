/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_MYBRUSH_EDITOR_H
#define GIMP_PAINTER_MYBRUSH_EDITOR_H
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets-types.h"
#include "gimpeditor.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_MYBRUSH_EDITOR (gimp_painter_mybrush_editor_get_type ())
#define GIMP_PAINTER_MYBRUSH_EDITOR(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GIMP_TYPE_PAINTER_MYBRUSH_EDITOR, GimpPainterMybrushEditor))
#define GIMP_IS_PAINTER_MYBRUSH_EDITOR(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GIMP_TYPE_PAINTER_MYBRUSH_EDITOR))
typedef struct _GimpPainterMybrushEditor GimpPainterMybrushEditor;
typedef struct _GimpPainterMybrushEditorClass GimpPainterMybrushEditorClass;
struct _GimpPainterMybrushEditor { GimpEditor parent_instance; gboolean binding_failed; };
struct _GimpPainterMybrushEditorClass { GimpEditorClass parent_class; };
GType gimp_painter_mybrush_editor_get_type (void) G_GNUC_CONST;
/* All presentations own a reference to the canonical tool model, obtained
 * through ref_for_context. NULL, wrong and closed contexts return an error. */
GtkWidget *gimp_painter_mybrush_editor_new (GimpContext *context, gboolean horizontal, GError **error);
GtkWidget *gimp_painter_mybrush_editor_popup (GimpContext *context, GtkWidget *parent, GError **error);
gboolean gimp_painter_mybrush_editor_set_context (GimpPainterMybrushEditor *editor, GimpContext *context, GError **error);
/* A NULL name saves the selected writable resource; a name saves the entire
 * draft as a new native-factory resource. Save As also preserves conflicts. */
gboolean gimp_painter_mybrush_editor_save (GimpPainterMybrushEditor *editor, const gchar *name, GError **error);
gboolean gimp_painter_mybrush_editor_rename (GimpPainterMybrushEditor *editor, const gchar *name, GError **error);
gboolean gimp_painter_mybrush_editor_delete (GimpPainterMybrushEditor *editor, GError **error);
void gimp_painter_mybrush_editor_request_preview (GimpPainterMybrushEditor *editor);
GdkPixbuf *gimp_painter_mybrush_editor_ref_preview (GimpPainterMybrushEditor *editor);
guint64 gimp_painter_mybrush_editor_preview_revision (GimpPainterMybrushEditor *editor);
gboolean gimp_painter_mybrush_editor_preview_pending (GimpPainterMybrushEditor *editor);
G_END_DECLS
#endif
