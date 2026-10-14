/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_LAYER_TILE_VIEW_H
#define GIMP_LAYER_TILE_VIEW_H
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets-types.h"
#include "gimppainterlayertiles.h"
G_BEGIN_DECLS
/* Operation-level source compatibility. The native GimpImageEditor parent,
 * runtime GType name and layouts replace the historical GtkBox/Interface ABI. */
typedef GimpPainterLayerTiles GimpLayerTileView;
typedef GimpPainterLayerTilesClass GimpLayerTileViewClass;
#define GIMP_TYPE_LAYER_TILE_VIEW (gimp_layer_tile_view_get_type ())
#define GIMP_LAYER_TILE_VIEW(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GIMP_TYPE_LAYER_TILE_VIEW, GimpLayerTileView))
#define GIMP_IS_LAYER_TILE_VIEW(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GIMP_TYPE_LAYER_TILE_VIEW))
#define GIMP_LAYER_TILE_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GIMP_TYPE_LAYER_TILE_VIEW, GimpLayerTileViewClass))
#define GIMP_IS_LAYER_TILE_VIEW_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GIMP_TYPE_LAYER_TILE_VIEW))
#define GIMP_LAYER_TILE_VIEW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GIMP_TYPE_LAYER_TILE_VIEW, GimpLayerTileViewClass))
GType gimp_layer_tile_view_get_type (void) G_GNUC_CONST;
/* GUI thread. Returns a floating, initially unbound widget. The writable
 * construct properties "context" and "image" retain their exact inputs;
 * g_object_get returns caller-owned references. Their assignments are
 * independent, including NULL. Context is the supplied object, not the private
 * per-canvas action context. Configure this widget through these properties;
 * direct GimpDocked context replacement is not this compatibility route.
 * Destroy is terminal, even if storage is retained. */
GimpLayerTileView *gimp_layer_tile_view_new (void);
G_END_DECLS
#endif
