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

/* The model membership reference is gone at items-changed, but remove()
 * retains one temporary reference until notification returns.  The second
 * reference must be the DnD snapshot: it bridges remove() returning and the
 * next destination insertion.  Final teardown below also rejects leaks. */
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
      g_assert_cmpuint (G_OBJECT (state->source)->ref_count, ==, 2);
      state->observed_owner_gap = TRUE;
    }
}

static void
w08017_dnd_destination_added (GimpContainer *container,
                              GimpObject    *object,
                              gpointer       data)
{
  W08017DndLifetime *state = data;
  state->destination_added++;
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
  destination_handler = g_signal_connect (right_children, "add",
                                           G_CALLBACK (w08017_dnd_destination_added), &state);
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

/* Modern generic multi-item extension coverage. The original 08.017 DnD
 * hunks describe the singleton contract; this regression must not be used
 * to accept image-specific layer COPY/Undo behavior or those siblings.
 * Keep ordinary fatal diagnostics enabled: a NULL destination diagnostic
 * must fail this test rather than be expected or suppressed. */
static void
w08017_dnd_multi_into (GtkTreeViewDropPosition position)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Multi source");
  GimpToolGroup *right = w08017_dnd_group (root, "Multi destination");
  GimpContainer *left_children = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *right_children = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *a = w08017_dnd_tool (left_children, "A");
  GimpToolInfo *b = w08017_dnd_tool (left_children, "B");
  GimpToolInfo *anchor = w08017_dnd_tool (right_children, "Existing child");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  GimpContainerTreeViewClass *klass = GIMP_CONTAINER_TREE_VIEW_GET_CLASS (view);
  GtkTreePath *path = w08017_dnd_path (view, GIMP_VIEWABLE (right));
  GtkTreeViewDropPosition result_position = position;
  GdkDragAction action = 0;
  GList *items = NULL;

  /* Borrowed model objects in source order. No extra object references may
   * mask the production remove/insert lifetime route. */
  items = g_list_append (items, a);
  items = g_list_append (items, b);
  g_assert_cmpuint (g_list_length (items), ==, 2);
  g_assert_true (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                      GIMP_VIEWABLE (right), path, position,
                                      &result_position, &action));
  g_assert_cmpint (result_position, ==, position);
  g_assert_cmpint (action, ==, GDK_ACTION_MOVE);
  gtk_tree_path_free (path);

  /* Invoke the real generic native vfunc exactly once for both sources. */
  klass->drop_viewables (view, items, GIMP_VIEWABLE (right), position);

  /* The current generic implementation mutates list links by reversing.
   * Free from the actual head regardless of whether a later production
   * repair preserves or reverses the caller's list. Caller-list ownership
   * is a separate issue and is deliberately not asserted by this case. */
  g_list_free (g_list_first (items));

  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (a)) == GIMP_VIEWABLE (right));
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (b)) == GIMP_VIEWABLE (right));
  g_assert_cmpint (gimp_container_get_n_children (left_children), ==, 0);
  g_assert_cmpint (gimp_container_get_n_children (right_children), ==, 3);
  g_assert_true (gimp_container_get_child_by_index (right_children, 0) == GIMP_OBJECT (a));
  g_assert_true (gimp_container_get_child_by_index (right_children, 1) == GIMP_OBJECT (b));
  g_assert_true (gimp_container_get_child_by_index (right_children, 2) == GIMP_OBJECT (anchor));
  g_assert_cmpint (gimp_container_get_n_children (root), ==, 2);
  g_assert_true (gimp_container_get_child_by_index (root, 0) == GIMP_OBJECT (left));
  g_assert_true (gimp_container_get_child_by_index (root, 1) == GIMP_OBJECT (right));
  w08017_dnd_assert_model (view, root, NULL);

  w08017_dnd_close_view (view);
  g_object_unref (root);
}

static void
w08017_dnd_multi_into_before (void)
{
  w08017_dnd_multi_into (GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
}

static void
w08017_dnd_multi_into_after (void)
{
  w08017_dnd_multi_into (GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}

/* Modern multi-source regression extensions.  The INTO reproducers above
 * remain separate from caller-list ownership and before/after ordering. */
static void
w08017_dnd_multi_call (GimpContainerTreeView   *view,
                       GList                   *items,
                       GimpViewable            *destination,
                       GtkTreeViewDropPosition  position)
{
  GimpContainerTreeViewClass *klass = GIMP_CONTAINER_TREE_VIEW_GET_CLASS (view);
  GtkTreePath *path = w08017_dnd_path (view, destination);
  GtkTreeViewDropPosition result_position = position;
  GdkDragAction action = 0;

  g_assert_true (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                      destination, path, position,
                                      &result_position, &action));
  g_assert_cmpint (result_position, ==, position);
  g_assert_cmpint (action, ==, GDK_ACTION_MOVE);
  gtk_tree_path_free (path);
  klass->drop_viewables (view, items, destination, position);
}

static void
w08017_dnd_assert_order (GimpContainer *container,
                         GimpObject   **expected,
                         guint          length)
{
  g_assert_cmpint (gimp_container_get_n_children (container), ==, length);
  for (guint i = 0; i < length; i++)
    g_assert_true (gimp_container_get_child_by_index (container, i) == expected[i]);
}

static void
w08017_dnd_multi_order (gboolean                same_parent,
                        gboolean                sources_before,
                        GtkTreeViewDropPosition position)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Order source");
  GimpToolGroup *right = w08017_dnd_group (root, "Order destination");
  GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *destination = same_parent ? source :
    gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *anchor = NULL;
  GimpToolInfo *a;
  GimpToolInfo *gap;
  GimpToolInfo *b;
  GimpToolInfo *tail;
  GimpContainerTreeView *view;
  GimpObject *expected[5];
  GList *items = NULL;
  guint n = 0;

  if (same_parent && ! sources_before)
    anchor = w08017_dnd_tool (destination, "Anchor");
  a = w08017_dnd_tool (source, "A");
  gap = w08017_dnd_tool (source, "Unselected gap");
  b = w08017_dnd_tool (source, "B");
  if (! anchor)
    anchor = w08017_dnd_tool (destination, "Anchor");
  tail = w08017_dnd_tool (destination, "Unselected tail");
  view = w08017_dnd_generic_view (root);

  items = g_list_append (items, a);
  items = g_list_append (items, b);
  w08017_dnd_multi_call (view, items, GIMP_VIEWABLE (anchor), position);
  /* Isolate placement from topology: the topology test below deliberately
   * frees the original head and asserts every link; this case does not. */
  g_list_free (g_list_first (items));

  if (same_parent && sources_before)
    expected[n++] = GIMP_OBJECT (gap);
  if (position == GTK_TREE_VIEW_DROP_AFTER)
    expected[n++] = GIMP_OBJECT (anchor);
  expected[n++] = GIMP_OBJECT (a);
  expected[n++] = GIMP_OBJECT (b);
  if (position == GTK_TREE_VIEW_DROP_BEFORE)
    expected[n++] = GIMP_OBJECT (anchor);
  if (same_parent && ! sources_before)
    expected[n++] = GIMP_OBJECT (gap);
  expected[n++] = GIMP_OBJECT (tail);
  w08017_dnd_assert_order (destination, expected, n);
  if (! same_parent)
    {
      GimpObject *remaining[] = { GIMP_OBJECT (gap) };
      w08017_dnd_assert_order (source, remaining, G_N_ELEMENTS (remaining));
    }
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (a)) ==
                 GIMP_VIEWABLE (same_parent ? left : right));
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (b)) ==
                 GIMP_VIEWABLE (same_parent ? left : right));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
}

static void
w08017_dnd_multi_same_parent_before (void)
{
  w08017_dnd_multi_order (TRUE, TRUE, GTK_TREE_VIEW_DROP_BEFORE);
  w08017_dnd_multi_order (TRUE, FALSE, GTK_TREE_VIEW_DROP_BEFORE);
}

static void
w08017_dnd_multi_same_parent_after (void)
{
  w08017_dnd_multi_order (TRUE, TRUE, GTK_TREE_VIEW_DROP_AFTER);
  w08017_dnd_multi_order (TRUE, FALSE, GTK_TREE_VIEW_DROP_AFTER);
}

static void
w08017_dnd_multi_cross_parent_before (void)
{
  w08017_dnd_multi_order (FALSE, TRUE, GTK_TREE_VIEW_DROP_BEFORE);
}

static void
w08017_dnd_multi_cross_parent_after (void)
{
  w08017_dnd_multi_order (FALSE, TRUE, GTK_TREE_VIEW_DROP_AFTER);
}

static void
w08017_dnd_multi_caller_topology (void)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Topology source");
  GimpToolGroup *right = w08017_dnd_group (root, "Topology destination");
  GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *destination = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *a = w08017_dnd_tool (source, "A");
  GimpToolInfo *b = w08017_dnd_tool (source, "B");
  GimpToolInfo *anchor = w08017_dnd_tool (destination, "Anchor");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  GList *items = NULL;
  GList *original_head;
  GList *original_tail;
  GimpObject *expected[] = { GIMP_OBJECT (anchor), GIMP_OBJECT (a), GIMP_OBJECT (b) };

  items = g_list_append (items, a);
  items = g_list_append (items, b);
  original_head = items;
  original_tail = items->next;
  /* AFTER already preserves object order in the preimage.  Thus this is an
   * independent list-node ownership failure, not the BEFORE/INTO defect. */
  w08017_dnd_multi_call (view, items, GIMP_VIEWABLE (anchor),
                         GTK_TREE_VIEW_DROP_AFTER);
  g_assert_null (original_head->prev);
  g_assert_true (original_head->next == original_tail);
  g_assert_true (original_head->data == a);
  g_assert_true (original_tail->prev == original_head);
  g_assert_null (original_tail->next);
  g_assert_true (original_tail->data == b);
  g_assert_cmpuint (g_list_length (original_head), ==, 2);
  g_list_free (original_head); /* Exactly the real caller's ownership route. */
  w08017_dnd_assert_order (destination, expected, G_N_ELEMENTS (expected));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
}

static void
w08017_dnd_multi_selected_anchor (GtkTreeViewDropPosition position)
{
  for (guint selected = 0; selected < 2; selected++)
    {
      GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
      GimpToolInfo *lead = w08017_dnd_tool (root, "Lead");
      GimpToolInfo *a = w08017_dnd_tool (root, "A");
      GimpToolInfo *gap = w08017_dnd_tool (root, "Gap");
      GimpToolInfo *b = w08017_dnd_tool (root, "B");
      GimpToolInfo *tail = w08017_dnd_tool (root, "Tail");
      GimpContainerTreeView *view = w08017_dnd_generic_view (root);
      GimpObject *expected[5];
      GList *items = NULL;
      guint n = 0;

      items = g_list_append (items, a);
      items = g_list_append (items, b);
      w08017_dnd_multi_call (view, items, GIMP_VIEWABLE (selected ? b : a), position);
      g_list_free (g_list_first (items));
      expected[n++] = GIMP_OBJECT (lead);
      if (selected)
        expected[n++] = GIMP_OBJECT (gap);
      expected[n++] = GIMP_OBJECT (a);
      expected[n++] = GIMP_OBJECT (b);
      if (! selected)
        expected[n++] = GIMP_OBJECT (gap);
      expected[n++] = GIMP_OBJECT (tail);
      w08017_dnd_assert_order (root, expected, n);
      w08017_dnd_assert_model (view, root, NULL);
      w08017_dnd_close_view (view);
      g_object_unref (root);
    }
}

static void
w08017_dnd_multi_selected_anchor_before (void)
{
  w08017_dnd_multi_selected_anchor (GTK_TREE_VIEW_DROP_BEFORE);
}

static void
w08017_dnd_multi_selected_anchor_after (void)
{
  w08017_dnd_multi_selected_anchor (GTK_TREE_VIEW_DROP_AFTER);
}

static void
w08017_dnd_multi_source_changed (GListModel *model,
                                guint       position,
                                guint       removed,
                                guint       added,
                                gpointer    data)
{
  W08017DndLifetime *states = data;
  guint owner_gaps = 0;

  if (! removed)
    return;
  g_assert_cmpuint (removed, ==, 1);
  for (guint i = 0; i < 2; i++)
    {
      g_assert_false (states[i].finalized);
      if (! gimp_viewable_get_parent (states[i].source))
        {
          g_assert_cmpuint (G_OBJECT (states[i].source)->ref_count, ==, 2);
          states[i].source_removed++;
          states[i].observed_owner_gap = TRUE;
          owner_gaps++;
        }
    }
  g_assert_cmpuint (owner_gaps, ==, 1);
}

static void
w08017_dnd_multi_transfer_lifetime (void)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Lifetime source");
  GimpToolGroup *right = w08017_dnd_group (root, "Lifetime destination");
  GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *destination = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *a = w08017_dnd_tool (source, "A");
  GimpToolInfo *b = w08017_dnd_tool (source, "B");
  GimpToolInfo *anchor = w08017_dnd_tool (destination, "Anchor");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  W08017DndLifetime states[] = {
    { GIMP_VIEWABLE (a), FALSE, 0, 0, FALSE },
    { GIMP_VIEWABLE (b), FALSE, 0, 0, FALSE }
  };
  GimpObject *expected[] = { GIMP_OBJECT (anchor), GIMP_OBJECT (a), GIMP_OBJECT (b) };
  GList *items = NULL;
  gulong handler;

  for (guint i = 0; i < 2; i++)
    {
      g_object_weak_ref (G_OBJECT (states[i].source), w08017_dnd_finalized, &states[i]);
      g_assert_cmpuint (G_OBJECT (states[i].source)->ref_count, ==, 1);
      items = g_list_append (items, states[i].source);
    }
  handler = g_signal_connect (source, "items-changed",
                              G_CALLBACK (w08017_dnd_multi_source_changed), states);
  w08017_dnd_multi_call (view, items, GIMP_VIEWABLE (anchor), GTK_TREE_VIEW_DROP_AFTER);
  g_signal_handler_disconnect (source, handler);
  g_list_free (g_list_first (items));
  for (guint i = 0; i < 2; i++)
    {
      g_assert_false (states[i].finalized);
      g_assert_true (states[i].observed_owner_gap);
      g_assert_cmpuint (states[i].source_removed, ==, 1);
      g_assert_cmpuint (G_OBJECT (states[i].source)->ref_count, ==, 1);
    }
  w08017_dnd_assert_order (destination, expected, G_N_ELEMENTS (expected));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
  g_assert_true (states[0].finalized);
  g_assert_true (states[1].finalized);
}

static void
w08017_dnd_multi_reject_unowned_ancestor (void)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpContainer *foreign = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolInfo *a = w08017_dnd_tool (root, "A");
  GimpToolInfo *unowned = w08017_dnd_tool (foreign, "Outside this root");
  GimpToolGroup *group = w08017_dnd_group (root, "Parent");
  GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (group));
  GimpToolInfo *child = w08017_dnd_tool (children, "Child");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  GimpContainerTreeViewClass *klass = GIMP_CONTAINER_TREE_VIEW_GET_CLASS (view);
  GtkTreePath *path = w08017_dnd_path (view, GIMP_VIEWABLE (child));
  GtkTreeViewDropPosition result_position = GTK_TREE_VIEW_DROP_BEFORE;
  GdkDragAction action = 0;
  GList *items = NULL;

  items = g_list_append (items, a);
  items = g_list_append (items, unowned);
  g_assert_false (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                       NULL, NULL, GTK_TREE_VIEW_DROP_BEFORE,
                                       &result_position, &action));
  g_assert_false (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                       GIMP_VIEWABLE (child), path,
                                       GTK_TREE_VIEW_DROP_BEFORE,
                                       &result_position, &action));
  g_assert_null (items->prev);
  g_assert_true (items->data == a && items->next->data == unowned);
  g_list_free (items);

  items = g_list_append (NULL, a);
  items = g_list_append (items, group);
  g_assert_false (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                       GIMP_VIEWABLE (child), path,
                                       GTK_TREE_VIEW_DROP_BEFORE,
                                       &result_position, &action));
  g_list_free (items);
  gtk_tree_path_free (path);
  g_assert_cmpint (gimp_container_get_n_children (root), ==, 2);
  g_assert_cmpint (gimp_container_get_n_children (foreign), ==, 1);
  g_assert_true (gimp_container_have (foreign, GIMP_OBJECT (unowned)));
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (child)) == GIMP_VIEWABLE (group));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
  g_object_unref (foreign);
}

/* Separate safety/cancellation probes.  No extra destination references in
 * the fixture may hide loss of its last external/model owner. */
typedef struct
{
  GimpContainer *root;
  GimpToolGroup *destination; /* Borrowed; weak notification tracks death. */
  gboolean after_insert;
  gboolean fired;
  gboolean finalized;
} W08017DndDestinationLoss;

static void
w08017_dnd_destination_finalized (gpointer data,
                                  GObject *object)
{
  W08017DndDestinationLoss *state = data;
  state->finalized = TRUE;
}

static void
w08017_dnd_remove_destination_owner (GListModel *model,
                                     guint       position,
                                     guint       removed,
                                     guint       added,
                                     gpointer    data)
{
  W08017DndDestinationLoss *state = data;

  if (! state->fired && (state->after_insert ? added > 0 : removed > 0))
    {
      state->fired = TRUE;
      g_assert_true (gimp_container_remove (state->root,
                                            GIMP_OBJECT (state->destination)));
    }
}

static void
w08017_dnd_multi_destination_owner_loss (gboolean after_insert,
                                           gboolean into)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Owner source");
  GimpToolGroup *right = w08017_dnd_group (root, "Owner destination");
  GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *destination = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *a = w08017_dnd_tool (source, "A");
  GimpToolInfo *b = w08017_dnd_tool (source, "B");
  GimpToolInfo *anchor = w08017_dnd_tool (destination, "Existing destination child");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  W08017DndDestinationLoss state = { root, right, after_insert, FALSE, FALSE };
  GimpObject *expected[] = { GIMP_OBJECT (a), GIMP_OBJECT (b) };
  GList *items = NULL;
  gulong handler;

  g_assert_cmpuint (G_OBJECT (right)->ref_count, ==, 1);
  g_object_weak_ref (G_OBJECT (right), w08017_dnd_destination_finalized, &state);
  handler = g_signal_connect (after_insert ? destination : source, "items-changed",
                              G_CALLBACK (w08017_dnd_remove_destination_owner), &state);
  items = g_list_append (items, a);
  items = g_list_append (items, b);
  w08017_dnd_multi_call (view, items, into ? GIMP_VIEWABLE (right) : GIMP_VIEWABLE (anchor),
                         into ? GTK_TREE_VIEW_DROP_INTO_OR_AFTER : GTK_TREE_VIEW_DROP_AFTER);

  /* A disappeared destination cancels the current transfer.  Its moved
   * source is restored; remaining sources are untouched.  This is the
   * explicit candidate cancellation policy, not an existing baseline pass. */
  g_assert_true (state.fired);
  g_assert_true (state.finalized);
  if (! after_insert)
    g_signal_handler_disconnect (source, handler);
  /* Destination signal connection died with its container in the other case. */
  g_assert_null (items->prev);
  g_assert_true (items->data == a && items->next->data == b);
  g_list_free (items);
  w08017_dnd_assert_order (source, expected, G_N_ELEMENTS (expected));
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (a)) == GIMP_VIEWABLE (left));
  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (b)) == GIMP_VIEWABLE (left));
  g_assert_cmpint (gimp_container_get_n_children (root), ==, 1);
  g_assert_true (gimp_container_get_child_by_index (root, 0) == GIMP_OBJECT (left));
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
}

static void
w08017_dnd_multi_destination_lost_on_remove (void)
{
  w08017_dnd_multi_destination_owner_loss (FALSE, FALSE);
  w08017_dnd_multi_destination_owner_loss (FALSE, TRUE);
}

static void
w08017_dnd_multi_destination_lost_on_insert (void)
{
  w08017_dnd_multi_destination_owner_loss (TRUE, FALSE);
  w08017_dnd_multi_destination_owner_loss (TRUE, TRUE);
}

/* Admission is separate from placement and ownership: ToolGroup children
 * accept ToolInfo, even though the root admits the broader ToolItem type. */
static void
w08017_dnd_multi_reject_incompatible_group (GtkTreeViewDropPosition position)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Source group");
  GimpToolGroup *right = w08017_dnd_group (root, "Destination group");
  GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *destination = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpToolInfo *tool = w08017_dnd_tool (source, "Compatible tool");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  GimpContainerTreeViewClass *klass = GIMP_CONTAINER_TREE_VIEW_GET_CLASS (view);
  GtkTreePath *path = w08017_dnd_path (view, GIMP_VIEWABLE (right));
  GtkTreeViewDropPosition result_position = position;
  GdkDragAction action = 0;
  GList *items = g_list_append (NULL, tool);
  GimpImage *image;
  GimpLayer *foreign_layer;

  g_assert_cmpuint (gimp_container_get_child_type (root), ==, GIMP_TYPE_TOOL_ITEM);
  g_assert_cmpuint (gimp_container_get_child_type (destination), ==, GIMP_TYPE_TOOL_INFO);
  g_assert_true (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                      GIMP_VIEWABLE (right), path, position,
                                      &result_position, &action));
  g_assert_cmpint (action, ==, GDK_ACTION_MOVE);
  g_assert_cmpint (result_position, ==, position);

  items = g_list_append (items, left);
  /* The first source fits.  The second is a valid root ToolItem, but cannot
   * be inserted into the narrower ToolInfo destination container. */
  g_assert_false (klass->drop_possible (view, GIMP_DND_TYPE_TOOL_ITEM, items,
                                       GIMP_VIEWABLE (right), path, position,
                                       &result_position, &action));
  g_assert_null (items->prev);
  g_assert_true (items->data == tool && items->next->data == left);
  g_list_free (items);

  /* Foreign root types still request COPY, which derived item views can
   * convert.  This checks admission only; generic drop_viewables has no
   * foreign-type conversion contract. */
  image = new_image ();
  foreign_layer = layer (image, NULL, "Foreign type COPY control");
  items = g_list_append (NULL, foreign_layer);
  action = 0;
  g_assert_true (klass->drop_possible (view, GIMP_DND_TYPE_LAYER_LIST, items,
                                      GIMP_VIEWABLE (right), path, position,
                                      &result_position, &action));
  g_assert_cmpint (action, ==, GDK_ACTION_COPY);
  g_assert_cmpint (result_position, ==, position);
  g_list_free (items);
  g_object_unref (image);
  gtk_tree_path_free (path);

  g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (tool)) == GIMP_VIEWABLE (left));
  g_assert_cmpint (gimp_container_get_n_children (root), ==, 2);
  g_assert_cmpint (gimp_container_get_n_children (source), ==, 1);
  g_assert_cmpint (gimp_container_get_n_children (destination), ==, 0);
  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_close_view (view);
  g_object_unref (root);
}

static void
w08017_dnd_multi_reject_group_into_before (void)
{
  w08017_dnd_multi_reject_incompatible_group (GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);
}

static void
w08017_dnd_multi_reject_group_into_after (void)
{
  w08017_dnd_multi_reject_incompatible_group (GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}

/* Reentrant native source admissions are independent of ordering, target
 * lifetime, and MOVE admission.  In particular, a root owner sets no parent.
 */
typedef enum
{
  W08017_ADOPT_PARENT,
  W08017_ADOPT_ROOT,
  W08017_ADOPT_FOREIGN_ROOT,
  W08017_ADOPT_SAME_DESTINATION,
  W08017_ADOPT_WEAK
} W08017DndAdoptKind;

typedef enum
{
  W08017_ADOPT_ON_SOURCE_REMOVE,
  W08017_ADOPT_ON_DESTINATION_INSERT,
  W08017_ADOPT_ON_ROLLBACK_REMOVE
} W08017DndAdoptPhase;

typedef struct
{
  GimpContainer *root;
  GimpContainer *source;
  GimpContainer *destination;
  GimpContainer *new_owner;
  GimpToolGroup *destination_group; /* Borrowed, never pinned by the probe. */
  GimpObject *chosen;
  W08017DndAdoptPhase phase;
  gboolean later;
  gboolean lose_destination;
  gboolean fired;
  gboolean destination_removed;
  gboolean destination_finalized;
  guint source_added;
  guint source_removed;
  guint destination_added;
  guint destination_removed_items;
} W08017DndAdoption;

static void
w08017_dnd_adoption_destination_finalized (gpointer data,
                                           GObject *object)
{
  W08017DndAdoption *state = data;
  state->destination_finalized = TRUE;
}

static void
w08017_dnd_adoption_remove_destination (W08017DndAdoption *state)
{
  if (! state->destination_removed)
    {
      state->destination_removed = TRUE;
      g_assert_true (gimp_container_remove (state->root,
                                            GIMP_OBJECT (state->destination_group)));
    }
}

static void
w08017_dnd_adoption_choose (W08017DndAdoption *state)
{
  state->fired = TRUE; /* Nested native notifications must not fire twice. */
  if (state->later)
    g_assert_true (gimp_container_remove (state->source, state->chosen));
  else if (state->phase == W08017_ADOPT_ON_DESTINATION_INSERT)
    g_assert_true (gimp_container_remove (state->destination, state->chosen));

  g_assert_null (gimp_viewable_get_parent (GIMP_VIEWABLE (state->chosen)));
  w08017_dnd_append (state->new_owner, state->chosen);
  g_assert_true (gimp_container_have (state->new_owner, state->chosen));
  if (state->lose_destination)
    w08017_dnd_adoption_remove_destination (state);
}

/* Count native admissions separately from GListModel splices: an insert
 * followed by reorder may publish a coherent replacement including unchanged
 * rows.  Replay every splice against borrowed identity snapshots, so counts
 * are not merely ignored and the probes do not add object owners. */
typedef struct
{
  GPtrArray *items;
} W08017DndModelProbe;

static void
w08017_dnd_model_changed (GListModel *model,
                         guint       position,
                         guint       removed,
                         guint       added,
                         gpointer    data)
{
  W08017DndModelProbe *probe = data;
  g_assert_cmpuint (position, <=, probe->items->len);
  g_assert_cmpuint (removed, <=, probe->items->len - position);
  g_ptr_array_remove_range (probe->items, position, removed);
  for (guint i = 0; i < added; i++)
    {
      GObject *item = g_list_model_get_item (model, position + i);
      g_assert_nonnull (item);
      g_ptr_array_insert (probe->items, position + i, item);
      g_object_unref (item);
    }
  g_assert_cmpuint (probe->items->len, ==, g_list_model_get_n_items (model));
  for (guint i = 0; i < probe->items->len; i++)
    {
      GObject *item = g_list_model_get_item (model, i);
      g_assert_true (item == g_ptr_array_index (probe->items, i));
      g_object_unref (item);
    }
}

static void
w08017_dnd_count_membership (GimpContainer *container,
                           GimpObject    *object,
                           gpointer       data)
{
  guint *count = data;
  (*count)++;
}

static void
w08017_dnd_adoption_source_changed (GListModel *model,
                                     guint       position,
                                     guint       removed,
                                     guint       added,
                                     gpointer    data)
{
  W08017DndAdoption *state = data;

  if (! state->fired && removed && state->phase == W08017_ADOPT_ON_SOURCE_REMOVE)
    w08017_dnd_adoption_choose (state);
}

static void
w08017_dnd_adoption_destination_changed (GListModel *model,
                                          guint       position,
                                          guint       removed,
                                          guint       added,
                                          gpointer    data)
{
  W08017DndAdoption *state = data;

  if (! state->fired && added && state->phase == W08017_ADOPT_ON_DESTINATION_INSERT)
    w08017_dnd_adoption_choose (state);
  else if (! state->fired && state->phase == W08017_ADOPT_ON_ROLLBACK_REMOVE)
    {
      if (added)
        w08017_dnd_adoption_remove_destination (state);
      else if (removed && state->destination_removed)
        w08017_dnd_adoption_choose (state);
    }
}

static void
w08017_dnd_adoption_case (W08017DndAdoptKind       kind,
                           W08017DndAdoptPhase      phase,
                           gboolean                later,
                           gboolean                lose_destination,
                           GtkTreeViewDropPosition position)
{
  GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpContainer *foreign = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpContainer *weak = gimp_list_new_weak (GIMP_TYPE_TOOL_ITEM, TRUE);
  GimpToolGroup *left = w08017_dnd_group (root, "Source owner");
  GimpToolGroup *right = w08017_dnd_group (root, "Intended owner");
  GimpToolGroup *third = w08017_dnd_group (root, "Newer owner");
  GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
  GimpContainer *destination = gimp_viewable_get_children (GIMP_VIEWABLE (right));
  GimpContainer *third_children = gimp_viewable_get_children (GIMP_VIEWABLE (third));
  GimpToolInfo *a = w08017_dnd_tool (source, "A");
  GimpToolInfo *b = w08017_dnd_tool (source, "B");
  GimpToolInfo *anchor = w08017_dnd_tool (destination, "Anchor");
  GimpContainerTreeView *view = w08017_dnd_generic_view (root);
  GimpContainerTreeView *foreign_view = w08017_dnd_generic_view (foreign);
  W08017DndAdoption state = { 0 };
  W08017DndLifetime lifetime[2] = {
    { GIMP_VIEWABLE (a), FALSE, 0, 0, FALSE },
    { GIMP_VIEWABLE (b), FALSE, 0, 0, FALSE }
  };
  GList *items = g_list_append (g_list_append (NULL, a), b);
  GList *second = items->next;
  GimpObject *remaining[] = { GIMP_OBJECT (later ? a : b) };
  GimpObject *chosen = GIMP_OBJECT (later ? b : a);
  gboolean into = position == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
                  position == GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
  gulong source_handler;
  gulong destination_handler;
  W08017DndModelProbe probes[2] = { { NULL }, { NULL } };
  GimpContainer *models[] = { source, destination };

  state.root = root;
  state.source = source;
  state.destination = destination;
  state.destination_group = right;
  state.chosen = chosen;
  state.phase = phase;
  state.later = later;
  state.lose_destination = lose_destination;
  switch (kind)
    {
    case W08017_ADOPT_PARENT:           state.new_owner = third_children; break;
    case W08017_ADOPT_ROOT:             state.new_owner = root; break;
    case W08017_ADOPT_FOREIGN_ROOT:     state.new_owner = foreign; break;
    case W08017_ADOPT_SAME_DESTINATION: state.new_owner = destination; break;
    case W08017_ADOPT_WEAK:             state.new_owner = weak; break;
    }
  for (guint i = 0; i < 2; i++)
    g_object_weak_ref (G_OBJECT (lifetime[i].source), w08017_dnd_finalized, &lifetime[i]);
  g_object_weak_ref (G_OBJECT (right), w08017_dnd_adoption_destination_finalized, &state);
  for (guint i = 0; i < 2; i++)
    {
      probes[i].items = g_ptr_array_new ();
      w08017_dnd_model_changed (G_LIST_MODEL (models[i]), 0, 0,
                               g_list_model_get_n_items (G_LIST_MODEL (models[i])),
                               &probes[i]);
      /* Validate before the reentrant adoption observer mutates the model. */
      g_signal_connect (models[i], "items-changed",
                        G_CALLBACK (w08017_dnd_model_changed), &probes[i]);
    }
  g_signal_connect (source, "add", G_CALLBACK (w08017_dnd_count_membership), &state.source_added);
  g_signal_connect (source, "remove", G_CALLBACK (w08017_dnd_count_membership), &state.source_removed);
  g_signal_connect (destination, "add", G_CALLBACK (w08017_dnd_count_membership), &state.destination_added);
  g_signal_connect (destination, "remove", G_CALLBACK (w08017_dnd_count_membership), &state.destination_removed_items);
  source_handler = g_signal_connect (source, "items-changed",
                                      G_CALLBACK (w08017_dnd_adoption_source_changed), &state);
  destination_handler = g_signal_connect (destination, "items-changed",
                                           G_CALLBACK (w08017_dnd_adoption_destination_changed), &state);

  w08017_dnd_multi_call (view, items, into ? GIMP_VIEWABLE (right) : GIMP_VIEWABLE (anchor),
                         position);

  g_assert_true (state.fired);
  g_assert_cmpint (state.destination_removed, ==, lose_destination);
  g_assert_cmpint (state.destination_finalized, ==, lose_destination);
  g_assert_false (lifetime[0].finalized);
  g_assert_false (lifetime[1].finalized);
  /* The original caller nodes and every link/data field are unchanged. */
  g_assert_null (items->prev);
  g_assert_true (items->data == a && items->next == second);
  g_assert_true (second->prev == items && second->data == b);
  g_assert_null (second->next);
  g_list_free (items);
  g_signal_handler_disconnect (source, source_handler);
  if (! state.destination_finalized)
    g_signal_handler_disconnect (destination, destination_handler);

  g_assert_true (gimp_container_have (state.new_owner, chosen));
  if (kind == W08017_ADOPT_WEAK)
    {
      /* A weak observer-list admission must not cancel an ordinary MOVE. */
      GimpObject *expected_before[] = { GIMP_OBJECT (a), GIMP_OBJECT (b), GIMP_OBJECT (anchor) };
      GimpObject *expected_after[] = { GIMP_OBJECT (anchor), GIMP_OBJECT (a), GIMP_OBJECT (b) };
      w08017_dnd_assert_order (destination,
                               position == GTK_TREE_VIEW_DROP_AFTER ? expected_after : expected_before, 3);
      g_assert_cmpint (gimp_container_get_n_children (source), ==, 0);
      g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (a)) == GIMP_VIEWABLE (right));
      g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (b)) == GIMP_VIEWABLE (right));
      g_assert_cmpuint (state.destination_added, ==, 2);
    }
  else
    {
      w08017_dnd_assert_order (source, remaining, G_N_ELEMENTS (remaining));
      g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (remaining[0])) == GIMP_VIEWABLE (left));
      g_assert_false (gimp_container_have (source, chosen));
      g_assert_cmpuint (state.source_added, ==, later ? 1 : 0);
      g_assert_cmpuint (state.source_removed, ==, later ? 2 : 1);
      if (kind == W08017_ADOPT_PARENT)
        g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (chosen)) == GIMP_VIEWABLE (third));
      else if (kind == W08017_ADOPT_SAME_DESTINATION)
        g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (chosen)) == GIMP_VIEWABLE (right));
      else
        g_assert_null (gimp_viewable_get_parent (GIMP_VIEWABLE (chosen)));
      g_assert_cmpint (gimp_container_have (root, chosen), ==, kind == W08017_ADOPT_ROOT);
      g_assert_cmpint (gimp_container_have (foreign, chosen), ==, kind == W08017_ADOPT_FOREIGN_ROOT);
      g_assert_cmpint (gimp_container_have (third_children, chosen), ==, kind == W08017_ADOPT_PARENT);
      if (! lose_destination)
        {
          g_assert_cmpint (gimp_container_have (destination, chosen), ==,
                           kind == W08017_ADOPT_SAME_DESTINATION);
          g_assert_false (gimp_container_have (destination, remaining[0]));
          g_assert_cmpint (gimp_container_get_n_children (destination), ==,
                           kind == W08017_ADOPT_SAME_DESTINATION ? 2 : 1);
          g_assert_true (gimp_container_get_child_by_index (destination, 0) == GIMP_OBJECT (anchor));
          if (kind == W08017_ADOPT_SAME_DESTINATION)
            g_assert_true (gimp_container_get_child_by_index (destination, 1) == chosen);
        }
      g_assert_cmpuint (state.destination_added, ==,
                        kind == W08017_ADOPT_SAME_DESTINATION ? 2 :
                        phase == W08017_ADOPT_ON_SOURCE_REMOVE ? 0 : 1);
    }

  w08017_dnd_assert_model (view, root, NULL);
  w08017_dnd_assert_model (foreign_view, foreign, NULL);
  w08017_dnd_close_view (view);
  w08017_dnd_close_view (foreign_view);
  g_object_unref (weak);
  g_object_unref (root);
  g_object_unref (foreign);
  g_assert_true (lifetime[0].finalized);
  g_assert_true (lifetime[1].finalized);
  g_assert_true (state.destination_finalized);
  for (guint i = 0; i < 2; i++)
    g_ptr_array_unref (probes[i].items);
}

static void
w08017_dnd_adoption_positions (W08017DndAdoptKind  kind,
                                W08017DndAdoptPhase phase,
                                gboolean           later,
                                gboolean           lose_destination)
{
  const GtkTreeViewDropPosition positions[] = {
    GTK_TREE_VIEW_DROP_BEFORE, GTK_TREE_VIEW_DROP_AFTER,
    GTK_TREE_VIEW_DROP_INTO_OR_BEFORE, GTK_TREE_VIEW_DROP_INTO_OR_AFTER
  };
  for (guint i = 0; i < G_N_ELEMENTS (positions); i++)
    w08017_dnd_adoption_case (kind, phase, later, lose_destination, positions[i]);
}

#define W08017_ADOPTION_CASE(name, kind, phase, later, lost) \
static void name (void) \
{ w08017_dnd_adoption_positions (kind, phase, later, lost); }

W08017_ADOPTION_CASE (w08017_dnd_relocate_parent_valid, W08017_ADOPT_PARENT, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, FALSE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_parent_lost, W08017_ADOPT_PARENT, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, TRUE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_root_valid, W08017_ADOPT_ROOT, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, FALSE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_root_lost, W08017_ADOPT_ROOT, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, TRUE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_foreign_root_valid, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, FALSE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_foreign_root_lost, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, TRUE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_later_foreign_valid, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_SOURCE_REMOVE, TRUE, FALSE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_later_foreign_lost, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_SOURCE_REMOVE, TRUE, TRUE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_on_insert_valid, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_DESTINATION_INSERT, FALSE, FALSE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_on_insert_lost, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_DESTINATION_INSERT, FALSE, TRUE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_on_rollback, W08017_ADOPT_FOREIGN_ROOT, W08017_ADOPT_ON_ROLLBACK_REMOVE, FALSE, TRUE)
W08017_ADOPTION_CASE (w08017_dnd_relocate_same_destination, W08017_ADOPT_SAME_DESTINATION, W08017_ADOPT_ON_DESTINATION_INSERT, FALSE, FALSE)
W08017_ADOPTION_CASE (w08017_dnd_weak_admission_keeps_move, W08017_ADOPT_WEAK, W08017_ADOPT_ON_SOURCE_REMOVE, FALSE, FALSE)

#undef W08017_ADOPTION_CASE

/* A later source can be adopted while the previous source's retained parent
 * is released, after that iteration computed its cancellation flag. */
typedef struct
{
  GimpContainer *root;
  GimpToolGroup *old_parent;
  GimpContainer *new_owner;
  GimpObject *later;
  guint callbacks;
  gboolean finalized;
} W08017DndCleanupAdoption;

static void
w08017_dnd_cleanup_parent_finalized (gpointer data,
                                      GObject *object)
{
  W08017DndCleanupAdoption *state = data;
  state->finalized = TRUE;
}

static void
w08017_dnd_cleanup_source_changed (GListModel *model,
                                    guint       position,
                                    guint       removed,
                                    guint       added,
                                    gpointer    data)
{
  W08017DndCleanupAdoption *state = data;
  if (! removed)
    return;

  state->callbacks++;
  if (state->callbacks == 1)
    {
      /* The native drop alone keeps this parent and its child container
       * alive until the first source has completed its transfer. */
      g_assert_true (gimp_container_remove (state->root, GIMP_OBJECT (state->old_parent)));
    }
  else
    {
      /* Container dispose clears the remaining B, then emits items-changed
       * on thaw.  Its newer admission happens during the old drop's cleanup. */
      g_assert_cmpuint (state->callbacks, ==, 2);
      g_assert_false (gimp_container_have (GIMP_CONTAINER (model), state->later));
      g_assert_null (gimp_viewable_get_parent (GIMP_VIEWABLE (state->later)));
      w08017_dnd_append (state->new_owner, state->later);
    }
}

static void
w08017_dnd_relocate_later_on_cleanup (void)
{
  const GtkTreeViewDropPosition positions[] = {
    GTK_TREE_VIEW_DROP_BEFORE, GTK_TREE_VIEW_DROP_AFTER,
    GTK_TREE_VIEW_DROP_INTO_OR_BEFORE, GTK_TREE_VIEW_DROP_INTO_OR_AFTER
  };
  for (guint i = 0; i < G_N_ELEMENTS (positions); i++)
    {
      GimpContainer *root = gimp_list_new (GIMP_TYPE_TOOL_ITEM, TRUE);
      GimpToolGroup *left = w08017_dnd_group (root, "Expiring source parent");
      GimpToolGroup *right = w08017_dnd_group (root, "Intended destination");
      GimpToolGroup *third = w08017_dnd_group (root, "Newer later-source parent");
      GimpContainer *source = gimp_viewable_get_children (GIMP_VIEWABLE (left));
      GimpContainer *destination = gimp_viewable_get_children (GIMP_VIEWABLE (right));
      GimpContainer *new_owner = gimp_viewable_get_children (GIMP_VIEWABLE (third));
      GimpToolInfo *a = w08017_dnd_tool (source, "A");
      GimpToolInfo *b = w08017_dnd_tool (source, "B");
      GimpToolInfo *anchor = w08017_dnd_tool (destination, "Anchor");
      GimpContainerTreeView *view = w08017_dnd_generic_view (root);
      W08017DndCleanupAdoption state = { root, left, new_owner, GIMP_OBJECT (b), 0, FALSE };
      W08017DndLifetime lifetime[2] = {
        { GIMP_VIEWABLE (a), FALSE, 0, 0, FALSE },
        { GIMP_VIEWABLE (b), FALSE, 0, 0, FALSE }
      };
      GList *items = g_list_append (g_list_append (NULL, a), b);
      GList *second = items->next;
      gboolean into = positions[i] == GTK_TREE_VIEW_DROP_INTO_OR_BEFORE ||
                      positions[i] == GTK_TREE_VIEW_DROP_INTO_OR_AFTER;
      GimpObject *before[] = { GIMP_OBJECT (a), GIMP_OBJECT (anchor) };
      GimpObject *after[] = { GIMP_OBJECT (anchor), GIMP_OBJECT (a) };
      GimpObject *adopted[] = { GIMP_OBJECT (b) };

      for (guint j = 0; j < 2; j++)
        g_object_weak_ref (G_OBJECT (lifetime[j].source), w08017_dnd_finalized, &lifetime[j]);
      g_object_weak_ref (G_OBJECT (left), w08017_dnd_cleanup_parent_finalized, &state);
      g_signal_connect (source, "items-changed",
                        G_CALLBACK (w08017_dnd_cleanup_source_changed), &state);
      w08017_dnd_multi_call (view, items, into ? GIMP_VIEWABLE (right) : GIMP_VIEWABLE (anchor),
                             positions[i]);
      g_assert_cmpuint (state.callbacks, ==, 2);
      g_assert_true (state.finalized);
      g_assert_false (lifetime[0].finalized);
      g_assert_false (lifetime[1].finalized);
      g_assert_null (items->prev);
      g_assert_true (items->data == a && items->next == second);
      g_assert_true (second->prev == items && second->data == b);
      g_assert_null (second->next);
      g_list_free (items);
      w08017_dnd_assert_order (destination, positions[i] == GTK_TREE_VIEW_DROP_AFTER ? after : before, 2);
      w08017_dnd_assert_order (new_owner, adopted, 1);
      g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (a)) == GIMP_VIEWABLE (right));
      g_assert_true (gimp_viewable_get_parent (GIMP_VIEWABLE (b)) == GIMP_VIEWABLE (third));
      w08017_dnd_assert_model (view, root, NULL);
      w08017_dnd_close_view (view);
      g_object_unref (root);
      g_assert_true (lifetime[0].finalized);
      g_assert_true (lifetime[1].finalized);
    }
}
