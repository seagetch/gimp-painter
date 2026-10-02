/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_LAYER_TILES_H
#define GIMP_PAINTER_LAYER_TILES_H
#include "gimpimageeditor.h"
G_BEGIN_DECLS
#define GIMP_TYPE_PAINTER_LAYER_TILES (gimp_painter_layer_tiles_get_type ())
#define GIMP_PAINTER_LAYER_TILES(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GIMP_TYPE_PAINTER_LAYER_TILES, GimpPainterLayerTiles))
#define GIMP_IS_PAINTER_LAYER_TILES(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GIMP_TYPE_PAINTER_LAYER_TILES))
typedef struct _GimpPainterLayerTiles GimpPainterLayerTiles;
typedef struct _GimpPainterLayerTilesClass GimpPainterLayerTilesClass;
struct _GimpPainterLayerTiles { GimpImageEditor parent_instance; };
struct _GimpPainterLayerTilesClass { GimpImageEditorClass parent_class; };
GType gimp_painter_layer_tiles_get_type (void) G_GNUC_CONST;
GtkWidget *gimp_painter_layer_tiles_new (GimpContext *context, GimpImage *image);
/* The image is independent of the global active image. NULL disconnects it. */
void gimp_painter_layer_tiles_set_image (GimpPainterLayerTiles *tiles, GimpImage *image);
void gimp_painter_layer_tiles_set_display (GimpPainterLayerTiles *tiles, GimpDisplay *display);
void gimp_painter_layer_tiles_cancel_interaction (GimpPainterLayerTiles *tiles);
GtkWidget *gimp_painter_layer_tiles_popup (GimpPainterLayerTiles *tiles, gboolean creation);
/* Uses the same checked path as drag-and-drop, including group/cycle checks. */
gboolean gimp_painter_layer_tiles_move (GimpPainterLayerTiles *tiles, GimpLayer *layer,
                                       GimpLayer *target, gboolean into, gboolean after);
gboolean gimp_painter_layer_tiles_long_press_pending (GimpPainterLayerTiles *tiles);
G_END_DECLS
#endif
