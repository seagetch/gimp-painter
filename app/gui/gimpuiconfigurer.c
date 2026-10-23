/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpuiconfigurer.c
 * Copyright (C) 2009 Martin Nordholts <martinn@src.gnome.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "gui-types.h"

#include "config/gimpguiconfig.h"

#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontext.h"

#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpdockcontainer.h"
#include "widgets/gimpdockwindow.h"
#include "widgets/gimptoolbox.h"
#include "widgets/gimpwidgets-utils.h"

#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-appearance.h"
#include "display/gimpimagewindow.h"
#include "display/gimppaintercanvasui.h"

#include "gimpuiconfigurer.h"


enum
{
  PROP_0,
  PROP_GIMP,
  PROP_READY
};


typedef struct _PainterConversion PainterConversion;
struct _PainterConversion
{
  GList             *windows; /* temporary leases, one suspension each */
  PainterConversion *previous;
  guint64            revision;
  gboolean           single_window_mode;
  gboolean           painter_canvas_ui;
  gboolean           suspend_painter;
};

struct _GimpUIConfigurerPrivate
{
  Gimp *gimp;
  PainterConversion *painter_conversion;
  guint64 painter_conversion_revision;
  guint   painter_idle_id;
  gboolean settled_single_window_mode;
  gboolean settled_painter_canvas_ui;
  gboolean painter_dirty;
  gboolean restore_replay_pending;
  gboolean stopping;
  GimpDialogFactory *restore_factory;
};


static void              gimp_ui_configurer_queue_replay                (GimpUIConfigurer  *ui_configurer);
static void              gimp_ui_configurer_restore_notify              (GimpDialogFactory *factory,
                                                                         GParamSpec        *pspec,
                                                                         GimpUIConfigurer  *ui_configurer);
static void              gimp_ui_configurer_dispose                     (GObject           *object);
static void              gimp_ui_configurer_set_property                (GObject           *object,
                                                                         guint              property_id,
                                                                         const GValue      *value,
                                                                         GParamSpec        *pspec);
static void              gimp_ui_configurer_get_property                (GObject           *object,
                                                                         guint              property_id,
                                                                         GValue            *value,
                                                                         GParamSpec        *pspec);
static void              gimp_ui_configurer_move_docks_to_columns       (GimpUIConfigurer  *ui_configurer,
                                                                         GimpImageWindow   *uber_image_window,
                                                                         gint               reference_x);
static void              gimp_ui_configurer_move_shells                 (GimpUIConfigurer  *ui_configurer,
                                                                         GimpImageWindow   *source_image_window,
                                                                         GimpImageWindow   *target_image_window);
static void              gimp_ui_configurer_separate_docks              (GimpUIConfigurer  *ui_configurer,
                                                                         GimpImageWindow   *source_image_window);
static void              gimp_ui_configurer_move_docks_to_window        (GimpUIConfigurer  *ui_configurer,
                                                                         GimpDockColumns   *dock_columns,
                                                                         GimpAlignmentType  screen_side,
                                                                         gint               logical_width);
static void              gimp_ui_configurer_separate_shells             (GimpUIConfigurer  *ui_configurer,
                                                                         GimpImageWindow   *source_image_window);
static void              gimp_ui_configurer_configure_for_single_window (GimpUIConfigurer  *ui_configurer);
static void              gimp_ui_configurer_configure_for_multi_window  (GimpUIConfigurer  *ui_configurer);
static void              gimp_ui_configurer_reconcile_toolbar           (GimpUIConfigurer  *ui_configurer);
static GimpImageWindow * gimp_ui_configurer_get_uber_window             (GimpUIConfigurer  *ui_configurer);


G_DEFINE_TYPE_WITH_PRIVATE (GimpUIConfigurer, gimp_ui_configurer,
                            GIMP_TYPE_OBJECT)

#define parent_class gimp_ui_configurer_parent_class


static void
gimp_ui_configurer_class_init (GimpUIConfigurerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = gimp_ui_configurer_dispose;
  object_class->set_property = gimp_ui_configurer_set_property;
  object_class->get_property = gimp_ui_configurer_get_property;

  g_object_class_install_property (object_class, PROP_GIMP,
                                   g_param_spec_object ("gimp", NULL, NULL,
                                                        GIMP_TYPE_GIMP,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_READY,
                                   g_param_spec_boolean ("ready", NULL, NULL,
                                                         TRUE, G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));
}

static void
gimp_ui_configurer_init (GimpUIConfigurer *ui_configurer)
{
  ui_configurer->p = gimp_ui_configurer_get_instance_private (ui_configurer);
}

static void
gimp_ui_configurer_dispose (GObject *object)
{
  GimpUIConfigurer *ui_configurer = GIMP_UI_CONFIGURER (object);

  gimp_ui_configurer_stop (ui_configurer);
  g_clear_object (&ui_configurer->p->restore_factory);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_ui_configurer_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GimpUIConfigurer *ui_configurer = GIMP_UI_CONFIGURER (object);

  switch (property_id)
    {
    case PROP_GIMP:
      ui_configurer->p->gimp = g_value_get_object (value); /* don't ref */
      if (ui_configurer->p->gimp)
        {
          ui_configurer->p->restore_factory =
            g_object_ref (gimp_dialog_factory_get_singleton ());
          g_signal_connect_object (ui_configurer->p->restore_factory,
                                   "notify::restore-pending",
                                   G_CALLBACK (gimp_ui_configurer_restore_notify),
                                   ui_configurer, 0);
          ui_configurer->p->settled_single_window_mode =
            GIMP_GUI_CONFIG (ui_configurer->p->gimp->config)->single_window_mode;
          ui_configurer->p->settled_painter_canvas_ui =
            ui_configurer->p->gimp->config->painter_canvas_ui;
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_ui_configurer_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GimpUIConfigurer *ui_configurer = GIMP_UI_CONFIGURER (object);

  switch (property_id)
    {
    case PROP_GIMP:
      g_value_set_object (value, ui_configurer->p->gimp);
      break;

    case PROP_READY:
      g_value_set_boolean (value, gimp_ui_configurer_is_ready (ui_configurer));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}


/* A callback may replace the requested mode or close a window.  Temporary
 * references protect memory; these checks protect the live topology. */
static gboolean
gimp_ui_configurer_current (GimpUIConfigurer *ui_configurer)
{
  PainterConversion *conversion = ui_configurer->p->painter_conversion;
  GimpGuiConfig     *config = GIMP_GUI_CONFIG (ui_configurer->p->gimp->config);

  return (! ui_configurer->p->stopping &&
          ! gimp_dialog_factory_restore_stopping (ui_configurer->p->restore_factory) &&
          ! gimp_dialog_factory_restore_pending (ui_configurer->p->restore_factory) &&
          conversion &&
          conversion->revision == ui_configurer->p->painter_conversion_revision &&
          conversion->single_window_mode == config->single_window_mode &&
          conversion->painter_canvas_ui == ui_configurer->p->gimp->config->painter_canvas_ui);
}

/* Finish an already detached child's transfer, but never retire a native
 * restore target enrolled by one of that transfer's callbacks. */
static gboolean
gimp_ui_configurer_can_retire (GimpUIConfigurer *ui_configurer,
                               GtkWidget        *widget)
{
  return (! ui_configurer->p->stopping &&
          ! gimp_dialog_factory_restore_stopping (ui_configurer->p->restore_factory) &&
          ! gimp_dialog_factory_restore_owns_widget (ui_configurer->p->restore_factory,
                                                    widget));
}

static gboolean
gimp_ui_configurer_window_live (GimpUIConfigurer *ui_configurer,
                                 GimpImageWindow  *window)
{
  return (window &&
          g_list_find (ui_configurer->p->gimp->image_windows, window) &&
          ! gtk_widget_in_destruction (GTK_WIDGET (window)));
}

static gboolean
gimp_ui_configurer_dialog_live (GtkWidget *dialog)
{
  return (dialog &&
          g_list_find (gimp_dialog_factory_get_open_dialogs (gimp_dialog_factory_get_singleton ()),
                       dialog) &&
          ! gtk_widget_in_destruction (dialog));
}

static gboolean
gimp_ui_configurer_display_live (GimpUIConfigurer *ui_configurer,
                                  GimpDisplay      *display)
{
  GimpDisplayShell *shell;

  if (! display ||
      ! g_list_find (gimp_get_display_iter (ui_configurer->p->gimp), display))
    return FALSE;

  shell = gimp_display_get_shell (display);
  return (shell &&
          gimp_ui_configurer_window_live (ui_configurer,
                                           gimp_display_shell_get_window (shell)));
}

static GList *
gimp_ui_configurer_ref_windows (GimpUIConfigurer *ui_configurer)
{
  GList *windows = gimp_get_image_windows (ui_configurer->p->gimp);
  GList *iter;

  for (iter = windows; iter; iter = iter->next)
    g_object_ref (iter->data);

  return windows;
}

static void
gimp_ui_configurer_enroll_window (GimpUIConfigurer *ui_configurer,
                                   GimpImageWindow  *window)
{
  PainterConversion *conversion;

  for (conversion = ui_configurer->p->painter_conversion;
       conversion; conversion = conversion->previous)
    if (! g_list_find (conversion->windows, window))
      {
        conversion->windows = g_list_append (conversion->windows,
                                              g_object_ref (window));
        gimp_image_window_session_suspend (window);
        if (conversion->suspend_painter)
          gimp_image_window_painter_suspend (window);
      }
}

/* The host owns an empty shell.  Never mistake its action target for a shell
 * which can be moved out of the host or deleted with it. */
static GimpDisplay *
gimp_ui_configurer_empty_display (GimpUIConfigurer *ui_configurer,
                                   GimpImageWindow  *window)
{
  GimpDisplayShell *shell;

  if (! gimp_ui_configurer_window_live (ui_configurer, window) ||
      gimp_image_window_get_n_shells (window) != 1)
    return NULL;

  shell = gimp_image_window_get_shell (window, 0);
  return (shell && shell->display &&
          ! gimp_display_get_image (shell->display)) ? shell->display : NULL;
}

static GimpDisplay *
gimp_ui_configurer_selected_display (GimpUIConfigurer *ui_configurer)
{
  Gimp        *gimp = ui_configurer->p->gimp;
  GimpDisplay *display = gimp_context_get_display (gimp_get_user_context (gimp));
  GList       *iter;

  if (gimp_ui_configurer_display_live (ui_configurer, display) &&
      gimp_display_get_image (display))
    return display;

  for (iter = gimp->image_windows; iter; iter = iter->next)
    if (gimp_image_window_is_painter_toolbar (iter->data))
      {
        GimpDisplay *target = gimp_image_window_get_action_display (iter->data);

        if (gimp_ui_configurer_display_live (ui_configurer, target) &&
            gimp_display_get_image (target))
          return target;
      }

  for (iter = gimp_get_display_iter (gimp); iter; iter = iter->next)
    if (gimp_ui_configurer_display_live (ui_configurer, iter->data) &&
        gimp_display_get_image (iter->data))
      return iter->data;

  return gimp_ui_configurer_display_live (ui_configurer, display) ? display : NULL;
}

static void
gimp_ui_configurer_get_window_center_pos (GtkWindow *window,
                                          gint      *out_x,
                                          gint      *out_y)
{
  gint x, y, w, h;
  gtk_window_get_position (window, &x, &y);
  gtk_window_get_size (window, &w, &h);

  if (out_x)
    *out_x = x + w / 2;
  if (out_y)
    *out_y = y + h / 2;
}

/**
 * gimp_ui_configurer_get_relative_window_pos:
 * @window:
 * @reference_x: Stable horizontal center of the image window.
 *
 * Returns: At what side @window is relative to @reference_x. Either
 * GIMP_ALIGN_LEFT or GIMP_ALIGN_RIGHT.
 **/
static GimpAlignmentType
gimp_ui_configurer_get_relative_window_pos (GtkWindow *window,
                                            gint       reference_x)
{
  gint x;

  gimp_ui_configurer_get_window_center_pos (window, &x, NULL);

  return x < reference_x ? GIMP_ALIGN_LEFT : GIMP_ALIGN_RIGHT;
}

static void
gimp_ui_configurer_move_docks_to_columns (GimpUIConfigurer *ui_configurer,
                                          GimpImageWindow  *uber_image_window,
                                          gint              reference_x)
{
  GList *dialogs;
  GList *dialog_iter;

  dialogs = g_list_copy (gimp_dialog_factory_get_open_dialogs (gimp_dialog_factory_get_singleton ()));
  for (dialog_iter = dialogs; dialog_iter; dialog_iter = dialog_iter->next)
    g_object_ref (dialog_iter->data);

  for (dialog_iter = dialogs;
       dialog_iter && gimp_ui_configurer_current (ui_configurer) &&
       gimp_ui_configurer_window_live (ui_configurer, uber_image_window);
       dialog_iter = dialog_iter->next)
    {
      GimpDockWindow  *dock_window;
      GimpDockColumns *dock_columns;
      GList           *docks;
      GList           *dock_iter;

      if (! gimp_ui_configurer_dialog_live (dialog_iter->data) ||
          ! GIMP_IS_DOCK_WINDOW (dialog_iter->data))
        continue;

      dock_window = GIMP_DOCK_WINDOW (dialog_iter->data);
      if (gimp_ui_configurer_get_relative_window_pos (GTK_WINDOW (dock_window),
                                                      reference_x) == GIMP_ALIGN_LEFT)
        dock_columns = gimp_image_window_get_left_docks (uber_image_window);
      else
        dock_columns = gimp_image_window_get_right_docks (uber_image_window);
      g_object_ref (dock_columns);

      docks = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (dock_window));
      for (dock_iter = docks; dock_iter; dock_iter = dock_iter->next)
        g_object_ref (dock_iter->data);

      for (dock_iter = docks;
           dock_iter && gimp_ui_configurer_current (ui_configurer) &&
           gimp_ui_configurer_window_live (ui_configurer, uber_image_window) &&
           gimp_ui_configurer_dialog_live (GTK_WIDGET (dock_window));
           dock_iter = dock_iter->next)
        {
          GimpDock *dock = dock_iter->data;

          if (gimp_dock_window_from_dock (dock) != dock_window)
            continue;

          /* The snapshot lease survives parent-set callbacks.  Finish the
           * transfer even when a callback supersedes the requested mode. */
          gimp_dock_window_remove_dock (dock_window, dock);
          if (gimp_ui_configurer_window_live (ui_configurer, uber_image_window) &&
              ! gtk_widget_get_parent (GTK_WIDGET (dock)))
            gimp_dock_columns_add_dock (dock_columns, dock, -1);
          else if (gimp_ui_configurer_dialog_live (GTK_WIDGET (dock_window)) &&
                   ! gtk_widget_get_parent (GTK_WIDGET (dock)))
            gimp_dock_window_add_dock (dock_window, dock, -1);
        }
      g_list_free_full (docks, g_object_unref);
      g_object_unref (dock_columns);

      /* A lease does not mean the source is still live.  Removing its last
       * dock normally destroys a non-toolbox dock window synchronously. */
      if (gimp_ui_configurer_dialog_live (GTK_WIDGET (dock_window)))
        {
          docks = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (dock_window));
          if (! docks &&
              gimp_ui_configurer_can_retire (ui_configurer, GTK_WIDGET (dock_window)))
            {
              gimp_dialog_factory_remove_dialog (gimp_dialog_factory_get_singleton (),
                                                 GTK_WIDGET (dock_window));
              gtk_widget_destroy (GTK_WIDGET (dock_window));
            }
          g_list_free (docks);
        }
    }

  g_list_free_full (dialogs, g_object_unref);
}

/**
 * gimp_ui_configurer_move_shells:
 * @ui_configurer:
 * @source_image_window:
 * @target_image_window:
 *
 * Move all display shells from one image window to the another.
 **/
static void
gimp_ui_configurer_move_shells (GimpUIConfigurer  *ui_configurer,
                                GimpImageWindow   *source_image_window,
                                GimpImageWindow   *target_image_window)
{
  while (gimp_ui_configurer_current (ui_configurer) &&
         gimp_ui_configurer_window_live (ui_configurer, source_image_window) &&
         gimp_ui_configurer_window_live (ui_configurer, target_image_window) &&
         gimp_image_window_get_n_shells (source_image_window) > 0)
    {
      GimpDisplayShell *shell;

      shell = gimp_image_window_get_shell (source_image_window, 0);

      g_object_ref (shell);
      gimp_image_window_remove_shell (source_image_window, shell);
      /* Nested mode requests are deferred until this transfer is complete. */
      if (gimp_ui_configurer_window_live (ui_configurer, target_image_window) &&
          shell->display && gimp_display_get_shell (shell->display) == shell &&
          ! gtk_widget_get_parent (GTK_WIDGET (shell)))
        gimp_image_window_add_shell (target_image_window, shell);
      else if (gimp_ui_configurer_window_live (ui_configurer, source_image_window) &&
               shell->display && gimp_display_get_shell (shell->display) == shell &&
               ! gtk_widget_get_parent (GTK_WIDGET (shell)))
        gimp_image_window_add_shell (source_image_window, shell);
      g_object_unref (shell);
    }
}

/**
 * gimp_ui_configurer_separate_docks:
 * @ui_configurer:
 * @image_window:
 *
 * Move out the docks from the image window.
 **/
static void
gimp_ui_configurer_separate_docks (GimpUIConfigurer  *ui_configurer,
                                   GimpImageWindow   *image_window)
{
  GimpDockColumns *left_docks  = NULL;
  GimpDockColumns *right_docks = NULL;
  gint             right_width;

  left_docks  = gimp_image_window_get_left_docks (image_window);
  right_docks = gimp_image_window_get_right_docks (image_window);

  right_width = gimp_image_window_get_right_docks_width (image_window);
  gimp_ui_configurer_move_docks_to_window (ui_configurer, left_docks,
                                           GIMP_ALIGN_LEFT, -1);
  if (gimp_ui_configurer_current (ui_configurer) &&
      gimp_ui_configurer_window_live (ui_configurer, image_window))
    gimp_ui_configurer_move_docks_to_window (ui_configurer, right_docks,
                                             GIMP_ALIGN_RIGHT, right_width);
}

/**
 * gimp_ui_configurer_move_docks_to_window:
 * @dock_columns:
 * @screen_side: At what side of the screen the dock window should be put.
 *
 * Moves docks in @dock_columns into a new #GimpDockWindow and
 * position it on the screen in a non-overlapping manner.
 */
static void
gimp_ui_configurer_move_docks_to_window (GimpUIConfigurer  *ui_configurer,
                                         GimpDockColumns   *dock_columns,
                                         GimpAlignmentType  screen_side,
                                         gint               logical_width)
{
  GdkMonitor    *monitor;
  GdkRectangle   monitor_rect;
  GdkGravity     gravity;
  GList         *docks;
  GList         *iter;
  gboolean       contains_toolbox = FALSE;
  GtkWidget     *dock_window = NULL;
  GimpImageWindow *source_window;
  GtkAllocation  original_size;
  gint           x, y;

  docks = g_list_copy (gimp_dock_columns_get_docks (dock_columns));
  if (! docks)
    return;

  source_window = GIMP_IMAGE_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (dock_columns),
                                                              GIMP_TYPE_IMAGE_WINDOW));
  if (! gimp_ui_configurer_window_live (ui_configurer, source_window))
    {
      g_list_free (docks);
      return;
    }
  g_object_ref (source_window);
  g_object_ref (dock_columns);
  for (iter = docks; iter; iter = iter->next)
    g_object_ref (iter->data);

  monitor = gimp_widget_get_monitor (GTK_WIDGET (dock_columns));

  gdk_monitor_get_workarea (monitor, &monitor_rect);

  /* Remember the size so we can set the new dock window to the same
   * size
   */
  gtk_widget_get_allocation (GTK_WIDGET (dock_columns), &original_size);
  /* A restored child may still carry its overlay allocation until GTK's
   * next layout.  The session's logical native width survives that gap. */
  if (logical_width > 0)
    original_size.width = logical_width;

  /* Do we need a toolbox window? */
  for (iter = docks; iter; iter = g_list_next (iter))
    {
      GimpDock *dock = GIMP_DOCK (iter->data);

      if (GIMP_IS_TOOLBOX (dock))
        {
          contains_toolbox = TRUE;
          break;
        }
    }

  /* Create a dock window to put the dock in. Checking for
   * GIMP_IS_TOOLBOX() is kind of ugly but not a disaster. We need
   * the dock window correctly configured if we create it for the
   * toolbox
   */
  dock_window =
    gimp_dialog_factory_dialog_new (gimp_dialog_factory_get_singleton (),
                                    monitor,
                                    NULL /*ui_manager*/,
                                    GTK_WIDGET (dock_columns),
                                    (contains_toolbox ?
                                     "gimp-toolbox-window" :
                                     "gimp-dock-window"),
                                    -1 /*view_size*/,
                                    FALSE /*present*/);

  if (! gimp_ui_configurer_dialog_live (dock_window))
    {
      dock_window = NULL;
      goto done;
    }
  g_object_ref (dock_window);

  if (! gimp_ui_configurer_current (ui_configurer) ||
      ! gimp_ui_configurer_window_live (ui_configurer, source_window))
    goto finish_transfer;

  /* Position the window before showing it. */
  if (screen_side == GIMP_ALIGN_LEFT)
    {
      gravity = GDK_GRAVITY_NORTH_WEST;

      x = monitor_rect.x;
      y = monitor_rect.y;
    }
  else if (screen_side == GIMP_ALIGN_RIGHT)
    {
      gravity = GDK_GRAVITY_NORTH_EAST;

      x = monitor_rect.x + monitor_rect.width - original_size.width;
      y = monitor_rect.y;
    }
  else
    {
      gimp_assert_not_reached ();
    }

  gtk_window_set_gravity (GTK_WINDOW (dock_window), gravity);
  if (! gimp_ui_configurer_dialog_live (dock_window))
    goto done;
  if (! gimp_ui_configurer_current (ui_configurer))
    goto finish_transfer;
  gtk_window_move (GTK_WINDOW (dock_window), x, y);
  if (! gimp_ui_configurer_dialog_live (dock_window))
    goto done;

  if (! gimp_ui_configurer_current (ui_configurer))
    goto finish_transfer;

  /* An explicit logical width must use the same resize contract as
   * gimp_session_info_apply_geometry(): dock-window style updates reset
   * default sizes. Preserve the native default-size path when no logical
   * width was supplied (for example, the left docks).
   */
  if (logical_width > 0)
    gtk_window_resize (GTK_WINDOW (dock_window),
                       original_size.width,
                       original_size.height);
  else
    gtk_window_set_default_size (GTK_WINDOW (dock_window),
                                 original_size.width,
                                 original_size.height);

  if (! gimp_ui_configurer_dialog_live (dock_window))
    goto done;
  if (! gimp_ui_configurer_current (ui_configurer))
    goto finish_transfer;

  /* Map the empty destination before inserting previously realized docks.
   * GTK 3 clears overlay-scrollbar indicators on unrealize, but their
   * adjustment callbacks remain connected until realization.  Inserting
   * into a mapped parent remaps the native subtree before its first new
   * allocation, instead of allocating stale indicators during window show. */
  gtk_widget_show (dock_window);

  for (iter = docks;
       iter && gimp_ui_configurer_current (ui_configurer) &&
       gimp_ui_configurer_window_live (ui_configurer, source_window) &&
       gimp_ui_configurer_dialog_live (dock_window);
       iter = iter->next)
    {
      GimpDock *dock = iter->data;

      if (! g_list_find (gimp_dock_columns_get_docks (dock_columns), dock))
        continue;

      gimp_dock_columns_remove_dock (dock_columns, dock);
      if (gimp_ui_configurer_dialog_live (dock_window) &&
          ! gtk_widget_get_parent (GTK_WIDGET (dock)))
        gimp_dock_window_add_dock (GIMP_DOCK_WINDOW (dock_window), dock, -1);
      else if (gimp_ui_configurer_window_live (ui_configurer, source_window) &&
               ! gtk_widget_get_parent (GTK_WIDGET (dock)))
        gimp_dock_columns_add_dock (dock_columns, dock, -1);
    }

finish_transfer:
  if (! gimp_ui_configurer_dialog_live (dock_window))
    goto done;
  {
    GList *moved = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (dock_window));
    gboolean empty = moved == NULL;

    g_list_free (moved);
    if (empty && gimp_ui_configurer_can_retire (ui_configurer, dock_window))
      {
        /* Construction or show can emit a newer mode request.  Do not leave
         * its unpopulated dock window behind when the transfer was skipped. */
        gimp_dialog_factory_remove_dialog (gimp_dialog_factory_get_singleton (), dock_window);
        gtk_widget_destroy (dock_window);
        goto done;
      }
  }

done:
  g_clear_object (&dock_window);
  g_object_unref (dock_columns);
  g_object_unref (source_window);
  g_list_free_full (docks, g_object_unref);
}

/**
 * gimp_ui_configurer_separate_shells:
 * @ui_configurer:
 * @source_image_window:
 *
 * Create one image window per display shell and move it there.
 **/
static void
gimp_ui_configurer_separate_shells (GimpUIConfigurer *ui_configurer,
                                    GimpImageWindow  *source_image_window)
{
  GimpDisplayShell *active_shell = gimp_image_window_get_active_shell (source_image_window);
  GimpImageWindow  *active_window = source_image_window;

  if (active_shell)
    g_object_ref (active_shell);

  /* The last display shell remains in its window. */
  while (gimp_ui_configurer_current (ui_configurer) &&
         gimp_ui_configurer_window_live (ui_configurer, source_image_window) &&
         gimp_image_window_get_n_shells (source_image_window) > 1)
    {
      GimpImageWindow  *new_image_window;
      GimpDisplayShell *shell;

      new_image_window = gimp_image_window_new (ui_configurer->p->gimp,
                                                NULL,
                                                gimp_dialog_factory_get_singleton (),
                                                gimp_widget_get_monitor (GTK_WIDGET (source_image_window)));
      /* Enroll before the first shell can reconcile its Painter overlays. */
      gimp_ui_configurer_enroll_window (ui_configurer, new_image_window);
      if (! gimp_ui_configurer_current (ui_configurer) ||
          ! gimp_ui_configurer_window_live (ui_configurer, source_image_window))
        {
          if (gimp_ui_configurer_window_live (ui_configurer, new_image_window) &&
              gimp_ui_configurer_can_retire (ui_configurer, GTK_WIDGET (new_image_window)))
            gimp_image_window_destroy (new_image_window);
          break;
        }

      shell = gimp_image_window_get_shell (source_image_window, 1);
      if (! shell)
        {
          if (gimp_ui_configurer_can_retire (ui_configurer, GTK_WIDGET (new_image_window)))
            gimp_image_window_destroy (new_image_window);
          break;
        }

      if (shell == active_shell)
        active_window = new_image_window;

      g_object_ref (shell);
      gimp_image_window_remove_shell (source_image_window, shell);
      if (gimp_ui_configurer_window_live (ui_configurer, new_image_window) &&
          shell->display && gimp_display_get_shell (shell->display) == shell &&
          ! gtk_widget_get_parent (GTK_WIDGET (shell)))
        gimp_image_window_add_shell (new_image_window, shell);
      else if (gimp_ui_configurer_window_live (ui_configurer, source_image_window) &&
               shell->display && gimp_display_get_shell (shell->display) == shell &&
               ! gtk_widget_get_parent (GTK_WIDGET (shell)))
        gimp_image_window_add_shell (source_image_window, shell);
      g_object_unref (shell);

      if (gimp_ui_configurer_window_live (ui_configurer, new_image_window))
        {
          if (gimp_image_window_get_n_shells (new_image_window) == 0 &&
              gimp_ui_configurer_can_retire (ui_configurer, GTK_WIDGET (new_image_window)))
            gimp_image_window_destroy (new_image_window);
          else if (gimp_ui_configurer_current (ui_configurer))
            {
              gtk_widget_set_size_request (GTK_WIDGET (new_image_window), 640, 480);
              gtk_widget_show (GTK_WIDGET (new_image_window));
            }
        }
    }

  if (gimp_ui_configurer_current (ui_configurer) &&
      gimp_ui_configurer_window_live (ui_configurer, active_window))
    gtk_window_present (GTK_WINDOW (active_window));
  g_clear_object (&active_shell);
}

/**
 * gimp_ui_configurer_configure_for_single_window:
 * @ui_configurer:
 *
 * Move docks and display shells into a single window.
 **/
static void
gimp_ui_configurer_configure_for_single_window (GimpUIConfigurer *ui_configurer)
{
  Gimp            *gimp = ui_configurer->p->gimp;
  GList           *windows = gimp_ui_configurer_ref_windows (ui_configurer);
  GList           *iter;
  GimpImageWindow *uber_image_window;
  GimpImageWindow *reference_window;
  GimpDisplay     *reference_display;
  GList           *displays = NULL;
  gboolean         have_image = FALSE;
  gint             reference_x;

  /* Reuse the host itself so its empty-window session and docks survive. */
  uber_image_window = gimp_ui_configurer_get_uber_window (ui_configurer);
  if (! uber_image_window)
    goto done;
  g_object_ref (uber_image_window);

  /* The toolbar host's independent geometry is not the image-layout origin.
   * For that destination, snapshot the selected image's center before role
   * changes or dock moves can resize either window.  Ordinary native merges
   * retain the destination window as their relative-side reference. */
  reference_window = uber_image_window;
  if (gimp_image_window_is_painter_toolbar (uber_image_window))
    {
      reference_display = gimp_ui_configurer_selected_display (ui_configurer);
      if (reference_display && gimp_display_get_image (reference_display))
        reference_window = gimp_display_shell_get_window (gimp_display_get_shell (reference_display));
    }
  gimp_ui_configurer_get_window_center_pos (GTK_WINDOW (reference_window),
                                            &reference_x, NULL);

  gimp_image_window_set_painter_toolbar (uber_image_window, FALSE);
  if (! gimp_ui_configurer_current (ui_configurer) ||
      ! gimp_ui_configurer_window_live (ui_configurer, uber_image_window))
    goto unref_window;

  /* An interrupted conversion can leave docks in a source image window.
   * Use the existing separation path before destroying any source window. */
  for (iter = windows; iter && gimp_ui_configurer_current (ui_configurer);
       iter = iter->next)
    if (iter->data != uber_image_window &&
        gimp_ui_configurer_window_live (ui_configurer, iter->data))
      gimp_ui_configurer_separate_docks (ui_configurer, iter->data);

  if (gimp_ui_configurer_current (ui_configurer) &&
      gimp_ui_configurer_window_live (ui_configurer, uber_image_window))
    gimp_ui_configurer_move_docks_to_columns (ui_configurer, uber_image_window,
                                              reference_x);

  for (iter = windows; iter && gimp_ui_configurer_current (ui_configurer);
       iter = iter->next)
    {
      GimpImageWindow *image_window = iter->data;

      if (image_window == uber_image_window ||
          ! gimp_ui_configurer_window_live (ui_configurer, image_window) ||
          ! gimp_ui_configurer_window_live (ui_configurer, uber_image_window))
        continue;

      gimp_image_window_set_painter_toolbar (image_window, FALSE);
      gimp_ui_configurer_move_shells (ui_configurer, image_window, uber_image_window);
      if (gimp_ui_configurer_window_live (ui_configurer, image_window) &&
          gimp_image_window_get_n_shells (image_window) == 0 &&
          gimp_ui_configurer_can_retire (ui_configurer, GTK_WIDGET (image_window)))
        gimp_image_window_destroy (image_window);
    }

  if (! gimp_ui_configurer_current (ui_configurer) ||
      ! gimp_ui_configurer_window_live (ui_configurer, uber_image_window))
    goto unref_window;

  /* Only delete placeholders after real shells have arrived.  With zero
   * images retain an ordinary usable empty display instead. */
  displays = g_list_copy (gimp_get_display_iter (gimp));
  for (iter = displays; iter; iter = iter->next)
    {
      GimpDisplay *display = iter->data;

      g_object_ref (display);
      if (gimp_ui_configurer_display_live (ui_configurer, display) &&
          gimp_display_get_image (display) &&
          gimp_display_shell_get_window (gimp_display_get_shell (display)) == uber_image_window)
        have_image = TRUE;
    }

  for (iter = displays; iter && gimp_ui_configurer_current (ui_configurer);
       iter = iter->next)
    {
      GimpDisplay *display = iter->data;

      if (gimp_ui_configurer_display_live (ui_configurer, display) &&
          ! gimp_display_get_image (display) &&
          gimp_display_shell_get_window (gimp_display_get_shell (display)) == uber_image_window &&
          (have_image || gimp_image_window_get_n_shells (uber_image_window) > 1))
        gimp_display_close (display);
    }
  g_list_free_full (displays, g_object_unref);

unref_window:
  g_object_unref (uber_image_window);
done:
  g_list_free_full (windows, g_object_unref);
}

/**
 * gimp_ui_configurer_configure_for_multi_window:
 * @ui_configurer:
 *
 * Moves all display shells into their own image window, then reconciles the
 * opt-in host.  An existing ordinary empty window is reused before creating
 * an extra display, independently for each Gimp instance.
 **/
static void
gimp_ui_configurer_configure_for_multi_window (GimpUIConfigurer *ui_configurer)
{
  GList *windows = gimp_ui_configurer_ref_windows (ui_configurer);
  GList *iter;

  for (iter = windows; iter && gimp_ui_configurer_current (ui_configurer);
       iter = iter->next)
    {
      GimpImageWindow *image_window = iter->data;

      if (! gimp_ui_configurer_window_live (ui_configurer, image_window))
        continue;
      gimp_ui_configurer_separate_docks (ui_configurer, image_window);
      if (gimp_ui_configurer_current (ui_configurer) &&
          gimp_ui_configurer_window_live (ui_configurer, image_window))
        gimp_ui_configurer_separate_shells (ui_configurer, image_window);
    }

  g_list_free_full (windows, g_object_unref);
  if (gimp_ui_configurer_current (ui_configurer))
    gimp_ui_configurer_reconcile_toolbar (ui_configurer);
}

static void
gimp_ui_configurer_reconcile_toolbar (GimpUIConfigurer *ui_configurer)
{
  Gimp            *gimp = ui_configurer->p->gimp;
  GimpGuiConfig   *config = GIMP_GUI_CONFIG (gimp->config);
  gboolean         enabled = gimp->config->painter_canvas_ui && ! config->single_window_mode;
  gboolean         have_image = FALSE;
  GList           *windows = gimp_ui_configurer_ref_windows (ui_configurer);
  GList           *iter;
  GimpImageWindow *host = NULL;
  GimpImageWindow *empty_window = NULL;

  for (iter = gimp_get_display_iter (gimp); iter; iter = iter->next)
    if (gimp_display_get_image (iter->data))
      {
        have_image = TRUE;
        break;
      }

  for (iter = windows; iter; iter = iter->next)
    {
      GimpImageWindow *window = iter->data;

      if (gimp_ui_configurer_empty_display (ui_configurer, window))
        {
          if (! empty_window)
            empty_window = window;
          if (! host && gimp_image_window_is_painter_toolbar (window))
            host = window;
        }
    }

  if (enabled && ! host)
    host = empty_window;

  if (enabled && ! host && gimp_ui_configurer_current (ui_configurer))
    {
      GimpDisplay *selected = gimp_ui_configurer_selected_display (ui_configurer);
      GimpDisplay *display;

      if (selected)
        g_object_ref (selected);
      display = gimp_create_display (gimp, NULL, gimp_unit_pixel (), 1.0, NULL);
      /* Creation has no image/borrowed columns yet.  Join every bracket before
       * assigning the role or allowing the new presenter to reconcile. */
      if (display && gimp_ui_configurer_display_live (ui_configurer, display))
        {
          host = gimp_display_shell_get_window (gimp_display_get_shell (display));
          gimp_ui_configurer_enroll_window (ui_configurer, host);
          windows = g_list_append (windows, g_object_ref (host));
        }
      /* gui_display_create selects the placeholder as a side effect. */
      if (gimp_ui_configurer_current (ui_configurer) &&
          gimp_ui_configurer_display_live (ui_configurer, selected))
        gimp_context_set_display (gimp_get_user_context (gimp), selected);
      g_clear_object (&selected);
    }

  if (! gimp_ui_configurer_current (ui_configurer))
    goto done;

  for (iter = windows; iter && gimp_ui_configurer_current (ui_configurer);
       iter = iter->next)
    {
      GimpImageWindow *window = iter->data;
      GimpDisplay     *empty;
      gboolean         was_host;

      if (! gimp_ui_configurer_window_live (ui_configurer, window))
        continue;
      was_host = gimp_image_window_is_painter_toolbar (window);
      empty = gimp_ui_configurer_empty_display (ui_configurer, window);
      if (enabled && window == host && empty)
        gimp_image_window_set_painter_toolbar (window, TRUE);
      else
        {
          gimp_image_window_set_painter_toolbar (window, FALSE);
          /* Docks were separated before this step.  A disabled lone host is
           * demoted; no empty placeholder survives beside real images, even
           * if an interrupted single-window pass already cleared its role. */
          if (gimp_ui_configurer_current (ui_configurer) &&
              gimp_ui_configurer_window_live (ui_configurer, window) &&
              (enabled || have_image || (was_host && window != host)))
            {
              empty = gimp_ui_configurer_empty_display (ui_configurer, window);
              if (empty)
                gimp_display_close (empty);
            }
        }
    }

done:
  g_list_free_full (windows, g_object_unref);
}

static GimpImageWindow *
gimp_ui_configurer_get_uber_window (GimpUIConfigurer *ui_configurer)
{
  Gimp  *gimp = ui_configurer->p->gimp;
  GList *iter;

  for (iter = gimp->image_windows; iter; iter = iter->next)
    if (gimp_ui_configurer_window_live (ui_configurer, iter->data) &&
        gimp_image_window_is_painter_toolbar (iter->data))
      return iter->data;

  for (iter = gimp_get_display_iter (gimp); iter; iter = iter->next)
    if (gimp_ui_configurer_display_live (ui_configurer, iter->data))
      return gimp_display_shell_get_window (gimp_display_get_shell (iter->data));

  return NULL;
}

/**
 * gimp_ui_configurer_update_appearance:
 * @ui_configurer:
 *
 * Updates the appearance of all shells in all image windows, so they
 * do whatever they deem necessary to fit the new UI mode mode.
 **/
static void
gimp_ui_configurer_update_appearance (GimpUIConfigurer *ui_configurer)
{
  GList *windows = gimp_ui_configurer_ref_windows (ui_configurer);
  GList *list;

  for (list = windows; list && gimp_ui_configurer_current (ui_configurer);
       list = list->next)
    {
      GimpImageWindow *window = list->data;
      GList          *shells = NULL;
      GList          *iter;
      gint            i;

      if (! gimp_ui_configurer_window_live (ui_configurer, window))
        continue;
      for (i = 0; i < gimp_image_window_get_n_shells (window); i++)
        shells = g_list_prepend (shells,
                                g_object_ref (gimp_image_window_get_shell (window, i)));

      for (iter = shells; iter && gimp_ui_configurer_current (ui_configurer);
           iter = iter->next)
        {
          GimpDisplayShell *shell = iter->data;

          if (gimp_ui_configurer_window_live (ui_configurer, window) &&
              shell->display &&
              gimp_display_get_shell (shell->display) == shell &&
              gimp_display_shell_get_window (shell) == window)
            gimp_display_shell_appearance_update (shell);
        }
      g_list_free_full (shells, g_object_unref);
    }

  g_list_free_full (windows, g_object_unref);
}

static gboolean
gimp_ui_configurer_painter_idle (gpointer data)
{
  GimpUIConfigurer *ui_configurer = g_weak_ref_get (data);

  if (ui_configurer)
    {
      ui_configurer->p->painter_idle_id = 0;
      gimp_ui_configurer_configure (ui_configurer,
                                    GIMP_GUI_CONFIG (ui_configurer->p->gimp->config)->single_window_mode);
      g_object_unref (ui_configurer);
    }

  return G_SOURCE_REMOVE;
}

static void
gimp_ui_configurer_painter_idle_free (gpointer data)
{
  g_weak_ref_clear (data);
  g_free (data);
}

static void
gimp_ui_configurer_queue_replay (GimpUIConfigurer *ui_configurer)
{
  GWeakRef *weak;

  if (ui_configurer->p->stopping || ui_configurer->p->painter_idle_id ||
      ui_configurer->p->painter_conversion ||
      gimp_dialog_factory_restore_stopping (ui_configurer->p->restore_factory) ||
      gimp_dialog_factory_restore_pending (ui_configurer->p->restore_factory))
    return;

  weak = g_new0 (GWeakRef, 1);
  g_weak_ref_init (weak, ui_configurer);
  ui_configurer->p->painter_idle_id =
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                     gimp_ui_configurer_painter_idle, weak,
                     gimp_ui_configurer_painter_idle_free);
}

static void
gimp_ui_configurer_restore_notify (GimpDialogFactory *factory,
                                   GParamSpec        *pspec,
                                   GimpUIConfigurer  *ui_configurer)
{
  if (ui_configurer->p->stopping ||
      gimp_dialog_factory_restore_stopping (factory))
    return;

  if (gimp_dialog_factory_restore_pending (factory))
    {
      /* Even unchanged startup needs an owner sync: an initially unrealized
       * empty image window may not yet have registered its native entry. */
      ui_configurer->p->restore_replay_pending = TRUE;
      if (ui_configurer->p->painter_conversion)
        ++ui_configurer->p->painter_conversion_revision;
    }
  else if (ui_configurer->p->restore_replay_pending ||
           ui_configurer->p->painter_dirty)
    {
      /* Never convert from the native source's cleanup stack. */
      gimp_ui_configurer_queue_replay (ui_configurer);
    }

  g_object_notify (G_OBJECT (ui_configurer), "ready");
}

gboolean
gimp_ui_configurer_is_ready (GimpUIConfigurer *ui_configurer)
{
  g_return_val_if_fail (GIMP_IS_UI_CONFIGURER (ui_configurer), FALSE);

  return (! ui_configurer->p->stopping &&
          ui_configurer->p->restore_factory &&
          ! gimp_dialog_factory_restore_stopping (ui_configurer->p->restore_factory) &&
          ! gimp_dialog_factory_restore_pending (ui_configurer->p->restore_factory) &&
          ! ui_configurer->p->painter_conversion &&
          ! ui_configurer->p->painter_dirty &&
          ! ui_configurer->p->restore_replay_pending &&
          ! ui_configurer->p->painter_idle_id);
}

void
gimp_ui_configurer_stop (GimpUIConfigurer *ui_configurer)
{
  g_return_if_fail (GIMP_IS_UI_CONFIGURER (ui_configurer));

  ui_configurer->p->stopping = TRUE;
  if (ui_configurer->p->painter_idle_id)
    {
      g_source_remove (ui_configurer->p->painter_idle_id);
      ui_configurer->p->painter_idle_id = 0;
    }
  if (ui_configurer->p->restore_factory)
    g_signal_handlers_disconnect_by_func (ui_configurer->p->restore_factory,
                                          gimp_ui_configurer_restore_notify,
                                          ui_configurer);
}

/**
 * gimp_ui_configurer_configure:
 * @ui_configurer:
 * @single_window_mode:
 *
 * Configure the UI.  Nested requests supersede the current revision, but do
 * not move a shell while an outer transfer has it temporarily parentless.
 **/
void
gimp_ui_configurer_configure (GimpUIConfigurer *ui_configurer,
                              gboolean          single_window_mode)
{
  Gimp        *gimp;
  GimpDisplay *selected;
  gboolean     superseded = FALSE;
  gboolean     session_replay;
  guint        pass;

  g_return_if_fail (GIMP_IS_UI_CONFIGURER (ui_configurer));

  if (ui_configurer->p->stopping ||
      gimp_dialog_factory_restore_stopping (ui_configurer->p->restore_factory))
    return;

  if (gimp_dialog_factory_restore_pending (ui_configurer->p->restore_factory))
    {
      ui_configurer->p->restore_replay_pending = TRUE;
      if (ui_configurer->p->painter_conversion)
        ++ui_configurer->p->painter_conversion_revision;
      g_object_notify (G_OBJECT (ui_configurer), "ready");
      return;
    }

  if (ui_configurer->p->painter_conversion)
    {
      PainterConversion *conversion = ui_configurer->p->painter_conversion;

      /* Construction and action-state synchronization can re-notify an
       * unchanged mode.  Only changed intent supersedes an atomic transfer;
       * invalidating on every notify prevents that new window ever settling. */
      if (single_window_mode != conversion->single_window_mode ||
          ui_configurer->p->gimp->config->painter_canvas_ui != conversion->painter_canvas_ui)
        ++ui_configurer->p->painter_conversion_revision;
      return;
    }

  session_replay = ui_configurer->p->restore_replay_pending;
  ui_configurer->p->restore_replay_pending = FALSE;
  if (ui_configurer->p->painter_idle_id)
    {
      g_source_remove (ui_configurer->p->painter_idle_id);
      ui_configurer->p->painter_idle_id = 0;
    }

  /* Session restoration fills newly created dock windows in a later idle.
   * An unchanged action-state notification must not convert that incomplete
   * topology.  A superseded conversion remains dirty until replay settles,
   * even when callbacks have returned to the previously settled flags. */
  if (! session_replay && ! ui_configurer->p->painter_dirty &&
      single_window_mode == ui_configurer->p->settled_single_window_mode &&
      ui_configurer->p->gimp->config->painter_canvas_ui ==
      ui_configurer->p->settled_painter_canvas_ui)
    {
      g_object_notify (G_OBJECT (ui_configurer), "ready");
      return;
    }

  /* Painter-only changes in settled single-window mode are presentation
   * changes, owned by each image window's config notification.  There is no
   * toolbar-host lifecycle here, and merging unrelated floating docks would
   * change the native layout.  Stay outside the conversion bracket so a
   * reentrant presentation cancellation does not become a topology replay. */
  if (! session_replay && ! ui_configurer->p->painter_dirty &&
      single_window_mode && ui_configurer->p->settled_single_window_mode)
    {
      ui_configurer->p->settled_painter_canvas_ui =
        ui_configurer->p->gimp->config->painter_canvas_ui;
      g_object_notify (G_OBJECT (ui_configurer), "ready");
      return;
    }

  ++ui_configurer->p->painter_conversion_revision;
  ui_configurer->p->restore_replay_pending = TRUE;

  g_object_ref (ui_configurer);
  g_object_notify (G_OBJECT (ui_configurer), "ready");
  gimp = g_object_ref (ui_configurer->p->gimp);
  selected = gimp_ui_configurer_selected_display (ui_configurer);
  if (selected)
    g_object_ref (selected);

  /* Settle an ordinary nested request before returning.  Callbacks that
   * continually replace intent get one weak idle rather than unbounded
   * recursion, without keeping Gimp or a destroyed image window alive. */
  for (pass = 0; pass < 2; pass++)
    {
      PainterConversion conversion;
      GList            *initial;
      GList            *iter;

      gboolean          topology = (ui_configurer->p->painter_dirty ||
                                    single_window_mode != ui_configurer->p->settled_single_window_mode ||
                                    (! single_window_mode &&
                                     gimp->config->painter_canvas_ui !=
                                     ui_configurer->p->settled_painter_canvas_ui));

      if (topology)
        ui_configurer->p->painter_dirty = TRUE;
      conversion.suspend_painter = topology;
      conversion.windows = gimp_ui_configurer_ref_windows (ui_configurer);
      conversion.previous = ui_configurer->p->painter_conversion;
      conversion.revision = ui_configurer->p->painter_conversion_revision;
      conversion.single_window_mode = single_window_mode;
      conversion.painter_canvas_ui = gimp->config->painter_canvas_ui;
      ui_configurer->p->painter_conversion = &conversion;

      /* A fixed initial list avoids enrolling a callback-created window twice. */
      initial = g_list_copy (conversion.windows);
      /* Hold every owner before the first callback. A later mode notify
       * cannot claim the singleton while peer windows still await merging. */
      for (iter = initial; iter; iter = iter->next)
        gimp_image_window_session_suspend (iter->data);
      if (topology)
        for (iter = initial; iter; iter = iter->next)
          gimp_image_window_painter_suspend (iter->data);
      g_list_free (initial);
      if (topology && gimp_ui_configurer_current (ui_configurer))
        {
          if (single_window_mode)
            gimp_ui_configurer_configure_for_single_window (ui_configurer);
          else
            gimp_ui_configurer_configure_for_multi_window (ui_configurer);
        }

      if (topology && gimp_ui_configurer_current (ui_configurer))
        {
          GimpDisplayShell *shell;

          if (! gimp_ui_configurer_display_live (ui_configurer, selected))
            {
              g_clear_object (&selected);
              selected = gimp_ui_configurer_selected_display (ui_configurer);
              if (selected)
                g_object_ref (selected);
            }
          if (selected)
            gimp_context_set_display (gimp_get_user_context (gimp), selected);
          if (selected && gimp_ui_configurer_current (ui_configurer) &&
              gimp_ui_configurer_display_live (ui_configurer, selected))
            {
              shell = gimp_display_get_shell (selected);
              gimp_image_window_set_active_shell (gimp_display_shell_get_window (shell), shell);
            }
        }

      if (topology && gimp_ui_configurer_current (ui_configurer))
        gimp_ui_configurer_update_appearance (ui_configurer);

      /* Keep the bracket installed during resume: presenters can emit new
       * mode requests too, and those must not recurse into half-resumed UI. */
      if (topology)
        for (iter = conversion.windows; iter; iter = iter->next)
          gimp_image_window_painter_resume (iter->data);

      /* Recompute from the surviving live windows, under the same revision
       * barrier. Native filling and GTK callbacks may replace intent again. */
      for (iter = conversion.windows;
           iter && gimp_ui_configurer_current (ui_configurer);
           iter = iter->next)
        if (gimp_ui_configurer_window_live (ui_configurer, iter->data) &&
            ! gimp_image_window_session_sync (iter->data))
          ++ui_configurer->p->painter_conversion_revision;
      for (iter = conversion.windows; iter; iter = iter->next)
        if (! gimp_image_window_session_resume (iter->data) &&
            gimp_ui_configurer_window_live (ui_configurer, iter->data))
          ++ui_configurer->p->painter_conversion_revision;
      g_list_free_full (conversion.windows, g_object_unref);
      superseded = ! gimp_ui_configurer_current (ui_configurer);
      ui_configurer->p->painter_conversion = conversion.previous;
      if (! superseded)
        {
          ui_configurer->p->settled_single_window_mode = conversion.single_window_mode;
          ui_configurer->p->settled_painter_canvas_ui = conversion.painter_canvas_ui;
          ui_configurer->p->painter_dirty = FALSE;
          ui_configurer->p->restore_replay_pending = FALSE;
          break;
        }

      if (ui_configurer->p->stopping ||
          gimp_dialog_factory_restore_stopping (ui_configurer->p->restore_factory))
        break;
      if (gimp_dialog_factory_restore_pending (ui_configurer->p->restore_factory))
        {
          ui_configurer->p->restore_replay_pending = TRUE;
          break;
        }
      single_window_mode = GIMP_GUI_CONFIG (gimp->config)->single_window_mode;
    }

  if (superseded)
    {
      ui_configurer->p->restore_replay_pending = TRUE;
      gimp_ui_configurer_queue_replay (ui_configurer);
    }

  g_object_notify (G_OBJECT (ui_configurer), "ready");

  g_clear_object (&selected);
  g_object_unref (gimp);
  g_object_unref (ui_configurer);
}
