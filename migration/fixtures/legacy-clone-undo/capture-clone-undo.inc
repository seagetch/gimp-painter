/* Ordinary RGB editing observations against the pinned legacy core.
 * All state is read through public APIs or declared, GType-checked Undo fields.
 * No implementation is replaced, and no ideal geometry is asserted.
 */
#include "base/tile-manager.h"
#include "core/gimpclonelayer.h"
#include "core/gimpcontainer.h"
#include "core/gimpdrawablemodundo.h"
#include "core/gimpimage-undo.h"
#include "core/gimpitemundo.h"
#include "core/gimplayermask.h"
#include "core/gimpundostack.h"

static void
capture_undo_node (const gchar *scene,
                   const gchar *step,
                   const gchar *stack_name,
                   const gchar *path,
                   GimpUndo    *undo)
{
  const gchar *item_name = "-";
  gint children = 0;
  gint i;

  if (GIMP_IS_ITEM_UNDO (undo))
    item_name = gimp_object_get_name (GIMP_ITEM_UNDO (undo)->item);
  if (GIMP_IS_UNDO_STACK (undo))
    children = gimp_container_get_n_children (GIMP_UNDO_STACK (undo)->undos);

  g_print ("UNDO_NODE case=%s step=%s stack=%s path=%s gtype=%s undo_type=%d item=%s children=%d",
           scene, step, stack_name, path, G_OBJECT_TYPE_NAME (undo),
           undo->undo_type, item_name, children);
  if (GIMP_IS_DRAWABLE_MOD_UNDO (undo))
    {
      GimpDrawableModUndo *mod = GIMP_DRAWABLE_MOD_UNDO (undo);
      g_print (" saved_wh=%d,%d saved_xy=%d,%d copy_tiles=%d image_type=%d",
               tile_manager_width (mod->tiles), tile_manager_height (mod->tiles),
               mod->offset_x, mod->offset_y, mod->copy_tiles, mod->type);
    }
  g_print ("\n");

  for (i = 0; i < children; i++)
    {
      GimpUndo *child = GIMP_UNDO (gimp_container_get_child_by_index (
                                   GIMP_UNDO_STACK (undo)->undos, i));
      gchar *child_path = g_strdup_printf ("%s/%d", path, i);
      capture_undo_node (scene, step, stack_name, child_path, child);
      g_free (child_path);
    }
}

static void
capture_resize_state (const gchar *scene,
                      const gchar *step,
                      GimpImage   *image,
                      GimpLayer   *source,
                      GimpLayer   *clone)
{
  GimpLayerMask *mask = gimp_layer_get_mask (clone);
  GimpUndoStack *undo_stack = gimp_image_get_undo_stack (image);
  GimpUndoStack *redo_stack = gimp_image_get_redo_stack (image);
  GimpUndo *top;

  g_print ("UNDO_STATE case=%s step=%s source_wh=%d,%d source_xy=%d,%d clone_wh=%d,%d clone_xy=%d,%d mask=%d",
           scene, step,
           gimp_item_get_width (GIMP_ITEM (source)),
           gimp_item_get_height (GIMP_ITEM (source)),
           gimp_item_get_offset_x (GIMP_ITEM (source)),
           gimp_item_get_offset_y (GIMP_ITEM (source)),
           gimp_item_get_width (GIMP_ITEM (clone)),
           gimp_item_get_height (GIMP_ITEM (clone)),
           gimp_item_get_offset_x (GIMP_ITEM (clone)),
           gimp_item_get_offset_y (GIMP_ITEM (clone)), mask != NULL);
  if (mask)
    g_print (" mask_wh=%d,%d mask_xy=%d,%d",
             gimp_item_get_width (GIMP_ITEM (mask)),
             gimp_item_get_height (GIMP_ITEM (mask)),
             gimp_item_get_offset_x (GIMP_ITEM (mask)),
             gimp_item_get_offset_y (GIMP_ITEM (mask)));
  g_print (" undo_depth=%d redo_depth=%d group_count=%d\n",
           gimp_undo_stack_get_depth (undo_stack),
           gimp_undo_stack_get_depth (redo_stack),
           gimp_image_get_undo_group_count (image));

  top = gimp_undo_stack_peek (undo_stack);
  if (top)
    capture_undo_node (scene, step, "undo", "0", top);
  top = gimp_undo_stack_peek (redo_stack);
  if (top)
    capture_undo_node (scene, step, "redo", "0", top);
}

static void
capture_clone_resize_undo (Gimp *gimp, gboolean with_mask)
{
  const gchar *scene = with_mask ? "white-mask" : "no-mask";
  GimpImage *image = gimp_create_image (gimp, 64, 64, GIMP_RGB, FALSE);
  GimpLayer *source = gimp_layer_new (image, 16, 16, GIMP_RGBA_IMAGE,
                                    "source", 1.0, GIMP_NORMAL_MODE);
  GimpLayer *clone;
  GimpRGB color = { 0.8, 0.2, 0.4, 1.0 };
  GError *error = NULL;
  gint cycle;

  g_assert (gimp_image_add_layer (image, source, NULL, 0, FALSE));
  gimp_drawable_fill (GIMP_DRAWABLE (source), &color, NULL);
  clone = gimp_clone_layer_new (image, source, 16, 16, "clone", 1.0,
                               GIMP_NORMAL_MODE);
  g_assert (gimp_image_add_layer (image, clone, NULL, 0, FALSE));
  gimp_item_set_offset (GIMP_ITEM (clone), 32, 30);
  if (with_mask)
    {
      GimpLayerMask *mask = gimp_layer_create_mask (clone, GIMP_ADD_WHITE_MASK, NULL);
      g_assert (mask != NULL);
      g_assert (gimp_layer_add_mask (clone, mask, FALSE, &error) == mask);
      g_assert_no_error (error);
      gimp_object_set_name (GIMP_OBJECT (mask), "clone-mask");
    }
  gimp_item_set_offset (GIMP_ITEM (source), 5, 7);
  gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, 16, 16);
  gimp_image_undo_free (image);
  capture_resize_state (scene, "baseline", image, source, clone);

  gimp_item_resize (GIMP_ITEM (source), gimp_get_user_context (gimp),
                    20, 18, -2, -3);
  capture_resize_state (scene, "resized", image, source, clone);
  for (cycle = 1; cycle <= 2; cycle++)
    {
      gchar *step = g_strdup_printf ("undo%d", cycle);
      gboolean result = gimp_image_undo (image);
      g_print ("UNDO_ACTION case=%s step=%s returned=%d\n", scene, step, result);
      capture_resize_state (scene, step, image, source, clone);
      g_free (step);
      step = g_strdup_printf ("redo%d", cycle);
      result = gimp_image_redo (image);
      g_print ("UNDO_ACTION case=%s step=%s returned=%d\n", scene, step, result);
      capture_resize_state (scene, step, image, source, clone);
      g_free (step);
    }
  g_print ("CLONE_RESIZE_UNDO_CASE_COMPLETE case=%s\n", scene);
}
