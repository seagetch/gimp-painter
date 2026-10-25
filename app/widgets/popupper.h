/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_NATIVE_POPOVER_H
#define GIMP_NATIVE_POPOVER_H
#include <gtk/gtk.h>
G_BEGIN_DECLS
#define GIMP_TYPE_POPOVER (gimp_popover_get_type ())
#define GIMP_POPOVER(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_POPOVER, GimpPopover))
#define GIMP_POPOVER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_POPOVER, GimpPopoverClass))
#define GIMP_IS_POPOVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_POPOVER))
#define GIMP_POPOVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_POPOVER, GimpPopoverClass))
typedef struct _GimpPopover GimpPopover;
typedef struct _GimpPopoverClass GimpPopoverClass;
struct _GimpPopover { GtkWindow parent_instance; };
struct _GimpPopoverClass {
  GtkWindowClass parent_class;
  void (*cancel) (GimpPopover *popover);
  void (*confirm) (GimpPopover *popover);
};
GType gimp_popover_get_type (void) G_GNUC_CONST;
/* GTK owner-thread only. Original popup-window factory, not GtkPopover.
 * Returns GTK-managed transfer-none top-level (or NULL); retain for an owned
 * reference and destroy to close. A successful frame insertion consumes the
 * child's floating reference as ordinary GTK containment does. Pre-insertion
 * failures do not consume the view. Synchronous GTK callbacks after insertion
 * may already have destroyed/reparented it: FALSE/NULL is not a rollback promise.
 * NULL/non-widget/toplevel/already-parented/currently-destroying views reject.
 * Old Interface/decorator APIs and GTK2 class-layout ABI are not restored. */
GtkWidget *gimp_popover_new (GtkWidget *view);
/* Common layer properties use the old area-aware GimpPopover2 placement.
 * anchor must be mapped. area is in anchor widget coordinates (or NULL for
 * its full allocation). Placement is fixed at opening; only later size growth
 * is constrained to the selected monitor, without tracking the anchor. */
void gimp_popover_show_at (GtkWidget *popup, GtkWidget *anchor, const GdkRectangle *area);
/* MyPaint toolbar uses the historical GimpPopup event contract: outside
 * cancellation is not consumed. Distinct from legacy GimpPopover2. */
GType gimp_painter_popup_get_type (void) G_GNUC_CONST;
GtkWidget *gimp_painter_popup_new (GtkWidget *view);
void gimp_painter_popup_show_over (GtkWidget *popup, GtkWidget *anchor);
G_END_DECLS
#endif
