/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 29.010/toolbar-ref-balance: observe the toolbar created by the real Canvas.
 * No measured widget is retained by the test. Weak-pointer clearing alone is
 * insufficient: GtkWidget destruction may dispose an object with references
 * still outstanding. The private qdata witness also requires finalization.
 */
#include "core/gimp-gui.h"
#include "core/gimptoolgroup.h"
#include "display/gimpimagewindow.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpoverlayframe.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpwindowstrategy.h"

typedef struct
{
  gpointer widget;
  guint    destroyed;
  guint    finalized;
} ToolbarLifetimeWitness;

typedef struct
{
  GimpToolInfo *tool;
  gpointer     native;
  GPtrArray   *widgets;
  GPtrArray   *parents;
} ToolbarLifetimeNative;

typedef struct
{
  GimpToolGroup *group;
  gchar         *active;
  gboolean       expanded;
} ToolbarLifetimeGroup;

static void
toolbar_lifetime_destroyed (GtkWidget *widget,
                            gpointer   data)
{
  ToolbarLifetimeWitness *witness = data;

  (void) widget;
  witness->destroyed++;
}

static void
toolbar_lifetime_finalized (gpointer data)
{
  ToolbarLifetimeWitness *witness = data;

  witness->finalized++;
}

static void
toolbar_lifetime_watch (GtkWidget              *widget,
                        ToolbarLifetimeWitness *witness)
{
  GQuark key = g_quark_from_static_string ("gimp-test-toolbar-finalization-witness");

  g_assert_nonnull (widget);
  g_assert_null (witness->widget);
  g_assert_cmpuint (witness->destroyed, ==, 0);
  g_assert_cmpuint (witness->finalized, ==, 0);
  g_assert_null (g_object_get_qdata (G_OBJECT (widget), key));
  witness->widget = widget;
  g_object_add_weak_pointer (G_OBJECT (widget), &witness->widget);
  g_signal_connect (widget, "destroy", G_CALLBACK (toolbar_lifetime_destroyed), witness);
  /* This private key is never replaced or cleared by the test. Its destroy
   * notifier therefore runs only when the observed object is finalized. */
  g_object_set_qdata_full (G_OBJECT (widget), key, witness, toolbar_lifetime_finalized);
}

static void
toolbar_lifetime_assert_alive (ToolbarLifetimeWitness *witness)
{
  g_assert_nonnull (witness->widget);
  g_assert_cmpuint (witness->destroyed, ==, 0);
  g_assert_cmpuint (witness->finalized, ==, 0);
}

static void
toolbar_lifetime_assert_finalized (ToolbarLifetimeWitness *witness)
{
  g_assert_null (witness->widget);
  g_assert_cmpuint (witness->destroyed, ==, 1);
  g_assert_cmpuint (witness->finalized, ==, 1);
}

/* Positive control for the lifetime observer: destroying an actual toolbar
 * while one explicit owner remains must not be reported as finalization. */
static void
toolbar_lifetime_observer_control (void)
{
  ToolbarLifetimeWitness witness = { 0 };
  GtkWidget *toolbar = gimp_tools_create_painter_toolbar_gui
    (info ("gimp-move-tool")->tool_options);

  g_assert_nonnull (toolbar);
  g_object_ref_sink (toolbar);
  toolbar_lifetime_watch (toolbar, &witness);
  gtk_widget_destroy (toolbar);
  drain ();
  g_assert_cmpuint (witness.destroyed, ==, 1);
  g_assert_cmpuint (witness.finalized, ==, 0);
  g_object_unref (toolbar);
  drain ();
  toolbar_lifetime_assert_finalized (&witness);
}

static void
toolbar_lifetime_set_boolean (const gchar *property,
                              gboolean     value)
{
  gboolean previous;

  g_object_get (gimp->config, property, &previous, NULL);
  /* In particular, do not re-notify single-window-mode with no displays. */
  if (previous != value)
    g_object_set (gimp->config, property, value, NULL);
}

static void
toolbar_lifetime_snapshot_native (ToolbarLifetimeNative *snapshot)
{
  GtkWidget *native = gimp_tools_get_tool_options_gui (snapshot->tool->tool_options);

  g_assert_nonnull (gtk_widget_get_parent (native));
  snapshot->native = native;
  g_object_add_weak_pointer (G_OBJECT (native), &snapshot->native);
  snapshot->widgets = g_ptr_array_new ();
  snapshot->parents = g_ptr_array_new ();
  gather (native, snapshot->widgets);
  for (guint i = 0; i < snapshot->widgets->len; i++)
    g_ptr_array_add (snapshot->parents,
                     gtk_widget_get_parent (g_ptr_array_index (snapshot->widgets, i)));
}

static void
toolbar_lifetime_assert_native (ToolbarLifetimeNative *snapshots,
                                guint                  count,
                                GtkWidget             *dockable,
                                GtkWidget             *dockable_parent,
                                GtkWidget             *editor)
{
  GimpToolInfo *selected = gimp_context_get_tool (gimp_get_user_context (gimp));

  g_assert_nonnull (dockable);
  g_assert_true (gtk_widget_get_parent (dockable) == dockable_parent);
  g_assert_true (gtk_bin_get_child (GTK_BIN (dockable)) == editor);
  g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (dockable)));
  g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (editor)) ==
                 selected->tool_options);
  for (guint i = 0; i < count; i++)
    {
      ToolbarLifetimeNative *snapshot = &snapshots[i];
      GPtrArray *current = g_ptr_array_new ();

      g_assert_nonnull (snapshot->native);
      g_assert_true (gimp_tools_get_tool_options_gui (snapshot->tool->tool_options) ==
                     snapshot->native);
      g_assert_true (gtk_widget_get_ancestor (snapshot->native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == editor);
      g_assert_true (gtk_widget_is_ancestor (snapshot->native, dockable));
      g_assert_false (gimp_painter_compact_options_get (snapshot->native));
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (snapshot->native)),
                       ==, GTK_ORIENTATION_VERTICAL);
      gather (snapshot->native, current);
      g_assert_cmpuint (current->len, ==, snapshot->widgets->len);
      for (guint n = 0; n < snapshot->widgets->len; n++)
        {
          GtkWidget *widget = g_ptr_array_index (snapshot->widgets, n);

          /* Establish membership before dereferencing a non-owning snapshot. */
          g_assert_true (g_ptr_array_find (current, widget, NULL));
          g_assert_true (gtk_widget_get_parent (widget) == g_ptr_array_index (snapshot->parents, n));
        }
      g_ptr_array_unref (current);
    }
}

static GtkWidget *
toolbar_lifetime_current (GimpDisplayShell *shell,
                          GimpToolInfo     *tool)
{
  const gchar *name = tool == info ("gimp-move-tool") ?
    "gimp-painter-tool-toolbar" : "gimp-painter-paint-toolbar";
  GtkWidget *toolbar = find (shell->canvas, name, GTK_TYPE_BOX);

  g_assert_nonnull (toolbar);
  g_assert_true (g_object_get_data (G_OBJECT (toolbar), "gimp-painter-toolbar-options") ==
                 tool->tool_options);
  g_assert_true (toolbar != gimp_tools_get_tool_options_gui (tool->tool_options));
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (toolbar)),
                   ==, GTK_ORIENTATION_HORIZONTAL);
  return toolbar;
}

static void
toolbar_lifetime_assert_presentation (GimpDisplayShell       *shell,
                                      GimpToolInfo           *tool,
                                      ToolbarLifetimeWitness *toolbar,
                                      ToolbarLifetimeWitness *bar,
                                      gboolean                visible)
{
  GtkWidget *frame;
  GList *children;

  toolbar_lifetime_assert_alive (toolbar);
  toolbar_lifetime_assert_alive (bar);
  g_assert_true (toolbar_lifetime_current (shell, tool) == toolbar->widget);
  g_assert_true (gtk_widget_get_parent (toolbar->widget) == bar->widget);
  g_assert_true (gtk_widget_is_ancestor (toolbar->widget, shell->canvas));
  children = gtk_container_get_children (GTK_CONTAINER (bar->widget));
  g_assert_cmpuint (g_list_length (children), ==, 1);
  g_assert_true (children->data == toolbar->widget);
  g_list_free (children);
  frame = gtk_widget_get_ancestor (toolbar->widget, GIMP_TYPE_OVERLAY_FRAME);
  g_assert_nonnull (frame);
  g_assert_cmpint (gtk_widget_get_visible (frame), ==, visible);
  g_assert_cmpint (gimp_painter_canvas_ui_get_visible (shell), ==, visible);
}

static void
toolbar_canvas_ref_balance (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *previous_tool = g_object_ref (gimp_context_get_tool (context));
  ToolbarLifetimeNative natives[2] = { 0 };
  ToolbarLifetimeGroup groups[3] = { 0 };
  guint n_groups = 0;
  gboolean previous_visible, previous_single;
  GimpDisplay *empty_display = gimp_get_empty_display (gimp);
  gboolean previous_empty_visible = empty_display ?
    gimp_painter_canvas_ui_get_visible (gimp_display_get_shell (empty_display)) : FALSE;
  gint previous_displays = gimp_container_get_n_children (gimp->displays);
  GimpDisplayShell *anchor;
  GimpImageWindow *image_window;
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  gpointer dockable;
  GtkWidget *existing_dockable, *dockable_parent, *editor;

  natives[0].tool = info ("gimp-move-tool");
  natives[1].tool = info ("gimp-paintbrush-tool");
  for (guint i = 0; i < 3; i++)
    {
      GimpToolInfo *tool = i < 2 ? natives[i].tool : previous_tool;
      GimpViewable *parent = gimp_viewable_get_parent (GIMP_VIEWABLE (tool));
      gboolean saved = FALSE;

      for (guint n = 0; n < n_groups; n++)
        if (GIMP_VIEWABLE (groups[n].group) == parent)
          saved = TRUE;
      if (GIMP_IS_TOOL_GROUP (parent) && ! saved)
        {
          groups[n_groups].group = g_object_ref (GIMP_TOOL_GROUP (parent));
          groups[n_groups].active = g_strdup (gimp_tool_group_get_active_tool (GIMP_TOOL_GROUP (parent)));
          groups[n_groups].expanded = gimp_viewable_get_expanded (parent);
          n_groups++;
        }
    }
  g_object_get (gimp->config, "painter-canvas-ui", &previous_visible,
                "single-window-mode", &previous_single, NULL);
  toolbar_lifetime_set_boolean ("painter-canvas-ui", FALSE);
  anchor = new_shell ();
  if (empty_display)
    g_assert_true (anchor->display == empty_display);
  /* Keep a real image window and its native dock alive while the measured
   * display is deleted and reopened. Conversion requires a live display. */
  toolbar_lifetime_set_boolean ("single-window-mode", TRUE);
  drain ();
  image_window = gimp_display_shell_get_window (anchor);
  gimp_painter_canvas_ui_set_visible (anchor, FALSE);
  existing_dockable = gimp_dialog_factory_find_widget (factory, "gimp-tool-options");
  dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (anchor)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  g_object_add_weak_pointer (G_OBJECT (dockable), &dockable);
  editor = gtk_bin_get_child (GTK_BIN (dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (editor));
  drain ();
  dockable_parent = gtk_widget_get_parent (dockable);
  g_assert_nonnull (dockable_parent);
  /* Populate both cached native GUIs before recording their real parents. */
  for (guint i = 0; i < G_N_ELEMENTS (natives); i++)
    {
      gimp_context_set_tool (context, natives[i].tool);
      drain ();
      toolbar_lifetime_snapshot_native (&natives[i]);
    }
  toolbar_lifetime_assert_native (natives, G_N_ELEMENTS (natives), dockable,
                                  dockable_parent, editor);

  for (guint generation = 0; generation < 2; generation++)
    {
      GimpDisplayShell *shell = new_shell ();
      ToolbarLifetimeWitness toolbar[4] = { 0 };
      ToolbarLifetimeWitness bar = { 0 };

      g_assert_true (gimp_display_shell_get_window (shell) == image_window);
      g_assert_cmpint (gimp_image_window_get_n_shells (image_window), >=, 2);
      /* Device initialization can restore a tool; select after new_shell. */
      gimp_context_set_tool (context, natives[0].tool);
      gimp_painter_canvas_ui_set_visible (shell, TRUE);
      drain ();
      toolbar_lifetime_watch (gtk_widget_get_parent (toolbar_lifetime_current (shell, natives[0].tool)), &bar);

      for (guint selection = 0; selection < G_N_ELEMENTS (toolbar); selection++)
        {
          GimpToolInfo *tool = natives[selection % G_N_ELEMENTS (natives)].tool;

          if (selection)
            {
              /* One switch happens while hidden. The old GUI remains owned
               * until show updates the toolbar for the new canonical tool. */
              if (selection == 2)
                gimp_painter_canvas_ui_set_visible (shell, FALSE);
              gimp_context_set_tool (context, tool);
              drain ();
              if (selection == 2)
                {
                  toolbar_lifetime_assert_alive (&toolbar[selection - 1]);
                  gimp_painter_canvas_ui_set_visible (shell, TRUE);
                  drain ();
                }
              toolbar_lifetime_assert_finalized (&toolbar[selection - 1]);
            }
          toolbar_lifetime_watch (toolbar_lifetime_current (shell, tool), &toolbar[selection]);
          for (guint cycle = 0; cycle < 8; cycle++)
            {
              /* Repeat each setter as well: idempotent hides must not gain
               * another ownership reference or allocate another toolbar. */
              gimp_painter_canvas_ui_set_visible (shell, FALSE);
              gimp_painter_canvas_ui_set_visible (shell, FALSE);
              drain ();
              toolbar_lifetime_assert_presentation (shell, tool, &toolbar[selection], &bar, FALSE);
              toolbar_lifetime_assert_native (natives, G_N_ELEMENTS (natives), dockable,
                                              dockable_parent, editor);
              gimp_painter_canvas_ui_set_visible (shell, TRUE);
              gimp_painter_canvas_ui_set_visible (shell, TRUE);
              drain ();
              toolbar_lifetime_assert_presentation (shell, tool, &toolbar[selection], &bar, TRUE);
              toolbar_lifetime_assert_native (natives, G_N_ELEMENTS (natives), dockable,
                                              dockable_parent, editor);
            }
        }
      /* Cover both normal visible close and close after hiding. Do not call
       * Canvas::close directly or release a test-owned toolbar reference. */
      if (generation)
        {
          gimp_painter_canvas_ui_set_visible (shell, FALSE);
          drain ();
          toolbar_lifetime_assert_presentation (shell, natives[1].tool, &toolbar[3], &bar, FALSE);
        }
      close_shell (shell);
      for (guint i = 0; i < G_N_ELEMENTS (toolbar); i++)
        toolbar_lifetime_assert_finalized (&toolbar[i]);
      toolbar_lifetime_assert_finalized (&bar);
      toolbar_lifetime_assert_native (natives, G_N_ELEMENTS (natives), dockable,
                                      dockable_parent, editor);
      g_test_message ("Canvas generation %u: 4 toolbars finalized after 32 hide/show cycles; closed %s",
                      generation + 1, generation ? "hidden" : "visible");
    }

  for (guint i = 0; i < G_N_ELEMENTS (natives); i++)
    {
      g_object_remove_weak_pointer (G_OBJECT (natives[i].native), &natives[i].native);
      g_ptr_array_unref (natives[i].widgets);
      g_ptr_array_unref (natives[i].parents);
    }
  if (! existing_dockable)
    {
      g_object_remove_weak_pointer (G_OBJECT (dockable), &dockable);
      gtk_widget_destroy (dockable);
      dockable = NULL;
    }
  /* Separate pre-existing floating docks while the conversion still has a
   * live image window. Deleting it first would destroy the merged docks. */
  toolbar_lifetime_set_boolean ("single-window-mode", previous_single);
  drain ();
  if (empty_display)
    {
      GimpImage *image = gimp_display_get_image (anchor->display);

      /* new_shell() filled the application's existing empty display. Return
       * that shell to its original state; only its fixture image is ours. */
      g_assert_true (anchor->display == empty_display);
      g_assert_nonnull (image);
      gimp_display_empty (anchor->display);
      g_object_unref (image);
      drain ();
      g_assert_true (gimp_display_get_shell (empty_display) == anchor);
      g_assert_null (gimp_display_get_image (empty_display));
    }
  else
    close_shell (anchor);
  toolbar_lifetime_set_boolean ("painter-canvas-ui", previous_visible);
  if (empty_display)
    gimp_painter_canvas_ui_set_visible (anchor, previous_empty_visible);
  gimp_context_set_tool (context, previous_tool);
  for (guint i = 0; i < n_groups; i++)
    gimp_tool_group_set_active_tool (groups[i].group, groups[i].active);
  for (guint i = 0; i < n_groups; i++)
    {
      gimp_viewable_set_expanded (GIMP_VIEWABLE (groups[i].group), groups[i].expanded);
      g_free (groups[i].active);
      g_object_unref (groups[i].group);
    }
  g_object_unref (previous_tool);
  drain ();
  g_assert_cmpint (gimp_container_get_n_children (gimp->displays), ==, previous_displays);
  if (empty_display)
    g_assert_cmpint (gimp_painter_canvas_ui_get_visible (anchor), ==, previous_empty_visible);
  if (existing_dockable)
    {
      g_assert_nonnull (dockable);
      g_assert_true (gimp_dialog_factory_find_widget (factory, "gimp-tool-options") == dockable);
      g_object_remove_weak_pointer (G_OBJECT (dockable), &dockable);
    }
}
