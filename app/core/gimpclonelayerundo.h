/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_CLONE_LAYER_UNDO_H__
#define __GIMP_CLONE_LAYER_UNDO_H__
#include "gimpitemundo.h"
G_BEGIN_DECLS
#define GIMP_TYPE_CLONE_LAYER_UNDO (gimp_clone_layer_undo_get_type ())
#define GIMP_CLONE_LAYER_UNDO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_CLONE_LAYER_UNDO, GimpCloneLayerUndo))
#define GIMP_IS_CLONE_LAYER_UNDO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_CLONE_LAYER_UNDO))
typedef struct _GimpCloneLayerUndo GimpCloneLayerUndo;
typedef struct _GimpCloneLayerUndoClass GimpCloneLayerUndoClass;
struct _GimpCloneLayerUndo { GimpItemUndo parent_instance; gboolean binding_failed; };
struct _GimpCloneLayerUndoClass { GimpItemUndoClass parent_class; };
GType gimp_clone_layer_undo_get_type (void) G_GNUC_CONST;
G_END_DECLS
#endif
