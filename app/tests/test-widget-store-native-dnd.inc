/* SPDX-License-Identifier: GPL-3.0-or-later
 * 08.017 native DnD integration fragments. Include in test-painter-canvas-ui.c
 * after its gimp/new_image/layer/drain helpers and before main().
 * Tests invoke the real native class vfuncs; no copied DnD implementation.
 * All calls are singleton drops, matching the pinned original source contract.
 */
#include "core/gimpviewable.h"
#include "core/gimplist.h"
#include "core/gimpitem.h"
#include "core/gimpdrawable.h"
#include "core/gimplayer.h"
#include "core/gimptoolitem.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolgroup.h"
#include "widgets/widgets-enums.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpitemtreeview.h"
#include "widgets/gimplayertreeview.h"
#include "menus/menus.h"

/* Small isolated native tool-model fixtures. No application tool registries,
 * active tools, user profiles or source files are modified by these tests. */
static void
w08017_dnd_append (GimpContainer *container,
                  GimpObject    *object)
{
  g_assert_true (gimp_container_insert (container, object,
                                       gimp_container_get_n_children (container)));
}

static GimpToolInfo *
w08017_dnd_tool (GimpContainer *container,
                const gchar   *name)
{
  GimpToolInfo *info = g_object_new (GIMP_TYPE_TOOL_INFO,
                                     "name", name,
                                     "icon-name", GIMP_ICON_TOOL_PAINTBRUSH,
                                     NULL);
  info->gimp = gimp; /* Same borrowed application link as normal tool infos. */
  info->label = g_strdup (name);
  info->tooltip = g_strdup (name);
  w08017_dnd_append (container, GIMP_OBJECT (info));
  g_object_unref (info); /* Only its real native container owns this child. */
  return info;
}

static GimpToolGroup *
w08017_dnd_group (GimpContainer *container,
                 const gchar   *name)
{
  GimpToolGroup *group = gimp_tool_group_new ();
  gimp_object_set_name (GIMP_OBJECT (group), name);
  w08017_dnd_append (container, GIMP_OBJECT (group));
  g_object_unref (group);
  return group;
}

static GimpContainerTreeView *
w08017_dnd_generic_view (GimpContainer *container)
{
  GtkWidget *widget = gimp_container_tree_view_new (container, NULL, 24, 0);
  g_assert_nonnull (widget);
  g_object_ref_sink (widget);
  g_object_set (widget, "reorderable", TRUE, NULL);
  return GIMP_CONTAINER_TREE_VIEW (widget);
}

static GimpContainerTreeView *
w08017_dnd_layer_view (GimpImage *image)
{
  GtkWidget *widget = gimp_item_tree_view_new (GIMP_TYPE_LAYER_TREE_VIEW,
                                               24, 0, TRUE, image,
                                               menus_get_global_menu_factory (gimp),
                                               "<Layers>", "/layers-popup");
  GimpContext *context;
  g_assert_nonnull (widget);
  g_object_ref_sink (widget);
  context = gimp_context_new (gimp, "08.017 native DnD test context",
                              gimp_get_user_context (gimp));
  gimp_context_set_image (context, image);
  gimp_container_view_set_context (GIMP_CONTAINER_VIEW (widget), context);
  g_object_unref (context);
  return GIMP_CONTAINER_TREE_VIEW (widget);
}

static void
w08017_dnd_close_view (GimpContainerTreeView *view)
{
  if (GIMP_IS_ITEM_TREE_VIEW (view))
    gimp_item_tree_view_set_image (GIMP_ITEM_TREE_VIEW (view), NULL);
  else
    gimp_container_view_set_container (GIMP_CONTAINER_VIEW (view), NULL);
  gtk_widget_destroy (GTK_WIDGET (view));
  g_object_unref (view);
  drain ();
}

typedef struct
{
  GimpViewable *needle;
  GtkTreePath *path;
} W08017DndFind;

static gboolean
w08017_dnd_find_row (GtkTreeModel *model,
                    GtkTreePath  *path,
                    GtkTreeIter  *iter,
                    gpointer      data)
{
  W08017DndFind *find = data;
  GimpViewRenderer *renderer = NULL;
  gtk_tree_model_get (model, iter,
                      GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
  if (renderer && renderer->viewable == find->needle)
    find->path = gtk_tree_path_copy (path);
  g_clear_object (&renderer);
  return find->path != NULL;
}

static GtkTreePath *
w08017_dnd_path (GimpContainerTreeView *view,
                GimpViewable          *viewable)
{
  W08017DndFind find = { viewable, NULL };
  gtk_tree_model_foreach (view->model, w08017_dnd_find_row, &find);
  g_assert_nonnull (find.path);
  return find.path;
}

static void
w08017_dnd_assert_model (GimpContainerTreeView *view,
                        GimpContainer         *container,
                        GtkTreeIter           *parent)
{
  gint count = gimp_container_get_n_children (container);
  g_assert_cmpint (gtk_tree_model_iter_n_children (view->model, parent), ==, count);
  for (gint i = 0; i < count; i++)
    {
      GtkTreeIter iter;
      GimpViewRenderer *renderer = NULL;
      GimpViewable *expected = GIMP_VIEWABLE (gimp_container_get_child_by_index (container, i));
      GimpContainer *children;
      g_assert_true (gtk_tree_model_iter_nth_child (view->model, &iter, parent, i));
      gtk_tree_model_get (view->model, &iter,
                          GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
      g_assert_nonnull (renderer);
      g_assert_true (renderer->viewable == expected);
      g_object_unref (renderer);
      children = gimp_viewable_get_children (expected);
      if (children)
        w08017_dnd_assert_model (view, children, &iter);
      else
        g_assert_cmpint (gtk_tree_model_iter_n_children (view->model, &iter), ==, 0);
    }
}

static gboolean
w08017_dnd_possible (GimpContainerTreeView  *view,
                    GimpDndType             type,
                    GimpViewable           *source,
                    GimpViewable           *destination,
                    GtkTreeViewDropPosition position,
                    GdkDragAction          *action)
{
  GimpContainerTreeViewClass *klass = GIMP_CONTAINER_TREE_VIEW_GET_CLASS (view);
  GtkTreeViewDropPosition result_position = position;
  GtkTreePath *path = w08017_dnd_path (view, destination);
  GList *items = g_list_prepend (NULL, source);
  gboolean result = klass->drop_possible (view, type, items, destination,
                                           path, position, &result_position, action);
  if (result)
    g_assert_cmpint (result_position, ==, position);
  g_list_free (items);
  gtk_tree_path_free (path);
  return result;
}

static void
w08017_dnd_drop (GimpContainerTreeView  *view,
                GimpDndType             type,
                GimpViewable           *source,
                GimpViewable           *destination,
                GtkTreeViewDropPosition position,
                GdkDragAction           expected_action)
{
  GdkDragAction action = 0;
  GList *items;
  g_assert_true (w08017_dnd_possible (view, type, source, destination,
                                     position, &action));
  g_assert_cmpint (action, ==, expected_action);
  items = g_list_prepend (NULL, source);
  GIMP_CONTAINER_TREE_VIEW_GET_CLASS (view)->drop_viewables (view, items,
                                                           destination, position);
  /* The generic implementation reverses the list in place. All tests use a
   * singleton, so this head remains valid and no multi-item claim is made. */
  g_list_free (items);
}

static void
w08017_dnd_same_parent_order (void)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolInfo *a = w08017_dnd_tool (root, "A");
  GimpToolInfo *b = w08017_dnd_tool (root, "B");
  GimpToolInfo *c = w08017_dnd_tool (root, "C");
  GimpToolInfo *d = w08017_dnd_tool (root, "D");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);

  w08017_dnd_drop (view, GIMP_DND_TYPE_TOOL_ITEM,
                   GIMP_VIEWABLE (c), GIMP_VIEWABLE (a),
                   GTK_TREE_VIEW_DROP_BEFORE, GDK_ACTION_MOVE);
  g_assert_true (gimp_container_get_child_by_index (root, 0) == GIMP_OBJECT (c));
  g_assert_true (gimp_container_get_child_by_index (root, 1) == GIMP_OBJECT (a));
  g_assert_true (gimp_container_get_child_by_index (root, 2) == GIMP_OBJECT (b));
  g_assert_true (gimp_container_get_child_by_index (root, 3) == GIMP_OBJECT (d));
  w08017_dnd_assert_model (view, root, NULL);

  w08017_dnd_drop (view, GIMP_DND_TYPE_TOOL_ITEM,
                   GIMP_VIEWABLE (c), GIMP_VIEWABLE (d),
                   GTK_TREE_VIEW_DROP_AFTER, GDK_ACTION_MOVE);
  g_assert_true (gimp_container_get_child_by_index (root, 0) == GIMP_OBJECT (a));
  g_assert_true (gimp_container_get_child_by_index (root, 1) == GIMP_OBJECT (b));
  g_assert_true (gimp_container_get_child_by_index (root, 2) == GIMP_OBJECT (d));
  g_assert_true (gimp_container_get_child_by_index (root, 3) == GIMP_OBJECT (c));
  w08017_dnd_assert_model (view, root, NULL);

  w08017_dnd_close_view (view);
  g_object_unref (root);
}

typedef struct
{
  GimpViewable *source; /* Deliberately borrowed; observer cannot mask a bug. */
  gboolean finalized;
  guint source_removed;
  guint destination_added;
  gboolean observed_owner_gap;
} W08017DndLifetime;

static void
w08017_dnd_finalized (gpointer data,
                     GObject *object)
{
  W08017DndLifetime *state = data;
  state->finalized = TRUE;
}

/* items-changed is emitted after the source container has released its strong
 * reference. Survival here therefore tests the DnD temporary ref rather than
 * merely survival inside a pre-remove signal. */
static void
w08017_dnd_source_changed (GListModel *model,
                          guint       position,
                          guint       removed,
                          guint       added,
                          gpointer    data)
{
  W08017DndLifetime *state = data;
  state->source_removed += removed;
  if (removed)
    {
      g_assert_false (state->finalized);
      g_assert_null (gimp_viewable_get_parent (state->source));
      g_assert_cmpuint (G_OBJECT (state->source)->ref_count, ==, 1);
      state->observed_owner_gap = TRUE;
    }
}

static void
w08017_dnd_destination_changed (GListModel *model,
                               guint       position,
                               guint       removed,
                               guint       added,
                               gpointer    data)
{
  W08017DndLifetime *state = data;
  state->destination_added += added;
}

static void
w08017_dnd_cross_parent_last_owner (void)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Left");
  GimpToolGroup *right = w08017_dnd_group (root, "Right");
  GimpContainer *left_children = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *right_children = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *source = w08017_dnd_tool (left_children, "Move me");
  GimpToolInfo *destination = w08017_dnd_tool (right_children, "Destination");
  GimpToolInfo *top = w08017_dnd_tool (root, "Top level");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  W08017DndLifetime state = { GIMP_VIEWABLE (source), FALSE, 0, 0, FALSE };
  gulong source_handler, destination_handler;

  g_object_weak_ref (G_OBJECT (source), w08017_dnd_finalized, &state);
  /* A renderer's viewable association is weak in the native implementation.
   * This assertion ensures the test really reaches a last-model-owner gap. */
  g_assert_cmpuint (G_OBJECT (source)->ref_count, ==, 1);
  source_handler = g_signal_connect (left_children, "items-changed",
                                      G_CALLBACK (w08017_dnd_source_changed), &state);
  destination_handler = g_signal_connect (right_children, "items-changed",
                                           G_CALLBACK (w08017_dnd_destination_changed), &state);
  w08017_dnd_drop (view, GIMP_DND_TYPE_TOOL_ITEM,
                   GIMP_VIEWABLE (source), GIMP_VIEWABLE (destination),
                   GTK_TREE_VIEW_DROP_BEFORE, GDK_ACTION_MOVE);
  g_signal_handler_disconnect (left_children, source_handler);
  g_signal_handler_disconnect (right_children, destination_handler);
  g_assert_cmpuint (state.source_removed, ==, 1);
  g_assert_cmpuint (state.destination_added, ==, 1);
  g_assert_true (state.observed_owner_gap);
  g_assert_false (state.finalized);
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (source)) == GIMP_VIEWABLE (right));
  g_assert_true (gimp_container_get_child_by_index (right_children, 0) == GIMP_OBJECT (source));
  g_assert_true (gimp_container_get_child_by_index (right_children, 1) == GIMP_OBJECT (destination));
  g_assert_false (gimp_container_have (left_children, GIMP_OBJECT (source)));
  g_assert_cmpuint (G_OBJECT (source)->ref_count, ==, 1);
  w08017_dnd_assert_model (view, root, NULL);

  /* Parent -> top-level resolves a different destination container. */
  w08017_dnd_drop (view, GIMP_DND_TYPE_TOOL_ITEM,
                   GIMP_VIEWABLE (source), GIMP_VIEWABLE (top),
                   GTK_TREE_VIEW_DROP_BEFORE, GDK_ACTION_MOVE);
  g_assert_null (gimp_viewable_get_parent (GIMP_VIEWABLE (source)));
  g_assert_true (gimp_container_have (root, GIMP_OBJECT (source)));
  g_assert_false (gimp_container_have (right_children, GIMP_OBJECT (source)));
  w08017_dnd_assert_model (view, root, NULL);

  /* Both INTO alternatives normalize to the first position of the group. */
  for (guint mode = 0; mode < 2; mode++)
    {
      w08017_dnd_drop (view, GIMP_DND_TYPE_TOOL_ITEM,
                       GIMP_VIEWABLE (source), GIMP_VIEWABLE (right),
                       mode ? GTK_TREE_VIEW_DROP_INTO_OR_AFTER : GTK_TREE_VIEW_DROP_INTO_OR_BEFORE,
                       GDK_ACTION_MOVE);
      g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (source)) == GIMP_VIEWABLE (right));
      g_assert_true (gimp_container_get_child_by_index (right_children, 0) == GIMP_OBJECT (source));
      w08017_dnd_assert_model (view, root, NULL);
      if (!mode)
        w08017_dnd_drop (view, GIMP_DND_TYPE_TOOL_ITEM,
                         GIMP_VIEWABLE (source), GIMP_VIEWABLE (top),
                         GTK_TREE_VIEW_DROP_BEFORE, GDK_ACTION_MOVE);
    }
  g_assert_false (state.finalized);
  w08017_dnd_close_view (view);
  g_assert_false (state.finalized); /* Still owned by its destination model. */
  g_object_unref (root);
  g_assert_true (state.finalized); /* No leaked temporary ref after DnD. */
}

static void
w08017_dnd_reject_self_ancestor (void)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *group = w08017_dnd_group (root, "Parent");
  GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (group));
  GimpToolInfo *child = w08017_dnd_tool (children, "Child");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  GdkDragAction action = 0;
  g_assert_false (w08017_dnd_possible (view, GIMP_DND_TYPE_TOOL_ITEM,
                                      GIMP_VIEWABLE (child), GIMP_VIEWABLE (child),
                                      GTK_TREE_VIEW_DROP_BEFORE, &action));
  g_assert_false (w08017_dnd_possible (view, GIMP_DND_TYPE_TOOL_ITEM,
                                      GIMP_VIEWABLE (group), GIMP_VIEWABLE (child),
                                      GTK_TREE_VIEW_DROP_BEFORE, &action));
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (child)) == GIMP_VIEWABLE (group));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
}

static GimpLayer *
w08017_dnd_layer_group (GimpImage *image,
                       GimpLayer *parent,
                       const char *name)
{
  GimpLayer *group = gimp_group_layer_new (image);
  gimp_object_set_name (GIMP_OBJECT (group), name);
  g_assert_true (gimp_image_add_layer (image, group, parent, -1, FALSE));
  return group;
}

static void
w08017_dnd_layer_group_move_undo (void)
{
  GimpImage *image = new_image ();
  GimpLayer *left = w08017_dnd_layer_group (image, NULL, "Left");
  GimpLayer *right = w08017_dnd_layer_group (image, NULL, "Right");
  GimpLayer *moved = w08017_dnd_layer_group (image, left, "Move group");
  GimpLayer *child = layer (image, moved, "Nested child");
  GimpLayer *anchor = layer (image, right, "Existing destination child");
  GimpContainerTreeView *view = w08017_dnd_layer_view (image);
  GimpContainer *root = gimp_image_get_layers (image);
  GList *selection = g_list_prepend (NULL, moved);
  gimp_image_set_selected_layers (image, selection);
  g_list_free (selection);

  w08017_dnd_drop (view, GIMP_DND_TYPE_LAYER_LIST,
                   GIMP_VIEWABLE (moved), GIMP_VIEWABLE (right),
                   GTK_TREE_VIEW_DROP_INTO_OR_AFTER, GDK_ACTION_MOVE);
  g_assert_true (gimp_layer_get_parent (moved) == right);
  g_assert_true (gimp_layer_get_parent (child) == moved);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (moved)) == image);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (child)) == image);
  g_assert_true (gimp_container_get_child_by_index (gimp_viewable_get_children (GIMP_VIEWABLE (right)), 0) == GIMP_OBJECT (moved));
  g_assert_true (gimp_container_get_child_by_index (gimp_viewable_get_children (GIMP_VIEWABLE (right)), 1) == GIMP_OBJECT (anchor));
  g_assert_nonnull (g_list_find (gimp_image_get_selected_layers (image), moved));
  w08017_dnd_assert_model (view, root, NULL);

  g_assert_true (gimp_image_undo (image));
  g_assert_true (gimp_layer_get_parent (moved) == left);
  g_assert_true (gimp_layer_get_parent (child) == moved);
  g_assert_true (gimp_container_get_child_by_index (gimp_viewable_get_children (GIMP_VIEWABLE (right)), 0) == GIMP_OBJECT (anchor));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (image);
}

static void
w08017_dnd_cross_image_copy (void)
{
  GimpImage *source_image = new_image ();
  GimpImage *destination_image = new_image ();
  GimpLayer *source_group = w08017_dnd_layer_group (source_image, NULL, "Source parent");
  GimpLayer *source = layer (source_image, source_group, "Source pixels");
  GimpLayer *destination_group = w08017_dnd_layer_group (destination_image, NULL, "Destination parent");
  GimpContainer *destination_children = gimp_viewable_get_children (GIMP_VIEWABLE (destination_group));
  GimpContainerTreeView *view = w08017_dnd_layer_view (destination_image);
  GeglColor *color = gegl_color_new ("#204060");
  GeglRectangle sample = { 0, 0, 1, 1 };
  guchar original_pixel[4], copied_pixel[4];
  GimpLayer *copy;

  gimp_layer_set_opacity (source, 0.37, FALSE);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), NULL, color);
  g_object_unref (color);
  w08017_dnd_drop (view, GIMP_DND_TYPE_LAYER_LIST,
                   GIMP_VIEWABLE (source), GIMP_VIEWABLE (destination_group),
                   GTK_TREE_VIEW_DROP_INTO_OR_AFTER, GDK_ACTION_COPY);
  g_assert_cmpint (gimp_container_get_n_children (destination_children), ==, 1);
  copy = GIMP_LAYER (gimp_container_get_child_by_index (destination_children, 0));
  g_assert_true (copy != source);
  g_assert_true (G_TYPE_FROM_INSTANCE (copy) == G_TYPE_FROM_INSTANCE (source));
  g_assert_true (gimp_layer_get_parent (source) == source_group);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (source)) == source_image);
  g_assert_true (gimp_layer_get_parent (copy) == destination_group);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (copy)) == destination_image);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (copy)), ==, gimp_item_get_width (GIMP_ITEM (source)));
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (copy)), ==, gimp_item_get_height (GIMP_ITEM (source)));
  g_assert_cmpfloat (gimp_layer_get_opacity (copy), ==, gimp_layer_get_opacity (source));
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), &sample, 1.0,
                   babl_format ("R'G'B'A u8"), original_pixel, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (copy)), &sample, 1.0,
                   babl_format ("R'G'B'A u8"), copied_pixel, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_cmpmem (original_pixel, sizeof original_pixel, copied_pixel, sizeof copied_pixel);
  w08017_dnd_assert_model (view, gimp_image_get_layers (destination_image), NULL);

  g_assert_true (gimp_image_undo (destination_image));
  g_assert_cmpint (gimp_container_get_n_children (destination_children), ==, 0);
  g_assert_true (gimp_layer_get_parent (source) == source_group);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (source)) == source_image);
  g_assert_cmpint (gimp_container_get_n_children (gimp_viewable_get_children (GIMP_VIEWABLE (source_group))), ==, 1);
  w08017_dnd_assert_model (view, gimp_image_get_layers (destination_image), NULL);
  w08017_dnd_close_view (view);
  g_object_unref (destination_image);
  g_object_unref (source_image);
}
