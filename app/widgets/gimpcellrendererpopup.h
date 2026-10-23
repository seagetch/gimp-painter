/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CELL_RENDERER_POPUP_H
#define GIMP_CELL_RENDERER_POPUP_H
#include <gtk/gtk.h>
G_BEGIN_DECLS
#define GIMP_TYPE_CELL_RENDERER_POPUP (gimp_cell_renderer_popup_get_type ())
#define GIMP_CELL_RENDERER_POPUP(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GIMP_TYPE_CELL_RENDERER_POPUP, GimpCellRendererPopup))
#define GIMP_IS_CELL_RENDERER_POPUP(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GIMP_TYPE_CELL_RENDERER_POPUP))
typedef struct _GimpCellRendererPopup GimpCellRendererPopup;
typedef struct _GimpCellRendererPopupClass GimpCellRendererPopupClass;
struct _GimpCellRendererPopup { GtkCellRendererPixbuf parent_instance; };
struct _GimpCellRendererPopupClass {
  GtkCellRendererPixbufClass parent_class;
  void (*parent_clicked) (GimpCellRendererPopup *, GObject *, const gchar *, GdkRectangle *);
};
GType             gimp_cell_renderer_popup_get_type (void) G_GNUC_CONST;
GtkCellRenderer * gimp_cell_renderer_popup_new (void);
/* One native owning tree per renderer. This does not register another slot.
 * Tree destroy explicitly closes the renderer store even if a caller retains it. */
gboolean          gimp_cell_renderer_popup_bind_tree (GimpCellRendererPopup *cell,
                                                       GtkWidget *tree);
void              gimp_cell_renderer_popup_clicked (GimpCellRendererPopup *cell,
                                                     GObject *widget,
                                                     const gchar *path,
                                                     GdkRectangle *area);
/* Native event handlers snapshot this before selecting a row; a callback
 * that opens/closes another popup supersedes the pending activation. */
guint64           gimp_cell_renderer_popup_get_generation (GimpCellRendererPopup *cell);
/* Borrowed; NULL after popdown, image change, removal or close. */
GtkWidget *       gimp_cell_renderer_popup_get_popup (GimpCellRendererPopup *cell);
G_END_DECLS
#endif
