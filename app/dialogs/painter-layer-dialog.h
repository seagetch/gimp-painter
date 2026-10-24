/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __PAINTER_LAYER_DIALOG_H__
#define __PAINTER_LAYER_DIALOG_H__

G_BEGIN_DECLS

GtkWidget * painter_clone_layer_dialog_new  (GimpImage   *image,
                                             GimpLayer   *layer,
                                             GimpContext *context,
                                             GtkWidget   *parent);
GtkWidget * painter_filter_layer_dialog_new (GimpImage   *image,
                                             GimpLayer   *layer,
                                             GimpContext *context,
                                             GtkWidget   *parent);

GtkWidget *painter_filter_layer_popup_new (GimpImage *image, GimpLayer *layer,
                                            GimpContext *context, GtkWidget *anchor,
                                            GtkWidget *layer_controls);
void painter_filter_layer_popup_show_at (GtkWidget *popup, GtkWidget *anchor,
                                         const GdkRectangle *area);

G_END_DECLS

#endif
