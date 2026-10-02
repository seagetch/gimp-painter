/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __PAINTER_LAYER_DIALOG_H__
#define __PAINTER_LAYER_DIALOG_H__

GtkWidget * painter_clone_layer_dialog_new  (GimpImage   *image,
                                             GimpLayer   *layer,
                                             GimpContext *context,
                                             GtkWidget   *parent);
GtkWidget * painter_filter_layer_dialog_new (GimpImage   *image,
                                             GimpLayer   *layer,
                                             GimpContext *context,
                                             GtkWidget   *parent);

#endif
