/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_LAYER_PRESET_VIEW_H__
#define __GIMP_LAYER_PRESET_VIEW_H__
#include "gimpdatafactoryview.h"
G_BEGIN_DECLS
#define GIMP_TYPE_LAYER_PRESET_VIEW (gimp_layer_preset_view_get_type ())
#define GIMP_LAYER_PRESET_VIEW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_LAYER_PRESET_VIEW, GimpLayerPresetView))
#define GIMP_IS_LAYER_PRESET_VIEW(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_LAYER_PRESET_VIEW))
typedef struct _GimpLayerPresetView GimpLayerPresetView;
typedef struct _GimpLayerPresetViewClass GimpLayerPresetViewClass;
struct _GimpLayerPresetView { GimpDataFactoryView parent_instance; gboolean ready; gboolean applying; gboolean menu_closed; };
struct _GimpLayerPresetViewClass { GimpDataFactoryViewClass parent_class; };
GType gimp_layer_preset_view_get_type (void) G_GNUC_CONST;
GtkWidget *gimp_layer_preset_view_new (GimpContext *, gint, GimpMenuFactory *);
gboolean gimp_layer_preset_view_can_apply (GimpLayerPresetView *);
void gimp_layer_preset_view_apply (GimpLayerPresetView *);
G_END_DECLS
#endif
