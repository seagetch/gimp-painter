/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpsinglewindowstrategy.c
 * Copyright (C) 2011 Martin Nordholts <martinn@src.gnome.org>
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

#include <string.h>

#include <gegl.h>
#include <gtk/gtk.h>

#include "display-types.h"

#include "core/gimp.h"
#include "config/gimpcoreconfig.h"

#include "menus/menus.h"

#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpmenudock.h"
#include "widgets/gimpwindowstrategy.h"

#include "gimpimagewindow.h"
#include "gimpsinglewindowstrategy.h"


static void        gimp_single_window_strategy_window_strategy_iface_init (GimpWindowStrategyInterface *iface);
static GtkWidget * gimp_single_window_strategy_show_dockable_dialog       (GimpWindowStrategy          *strategy,
                                                                           Gimp                        *gimp,
                                                                           GimpDialogFactory           *factory,
                                                                           GdkMonitor                  *monitor,
                                                                           const gchar                 *identifiers);


G_DEFINE_TYPE_WITH_CODE (GimpSingleWindowStrategy, gimp_single_window_strategy, GIMP_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GIMP_TYPE_WINDOW_STRATEGY,
                                                gimp_single_window_strategy_window_strategy_iface_init))

#define parent_class gimp_single_window_strategy_parent_class


static void
gimp_single_window_strategy_class_init (GimpSingleWindowStrategyClass *klass)
{
}

static void
gimp_single_window_strategy_init (GimpSingleWindowStrategy *strategy)
{
}

static void
gimp_single_window_strategy_window_strategy_iface_init (GimpWindowStrategyInterface *iface)
{
  iface->show_dockable_dialog = gimp_single_window_strategy_show_dockable_dialog;
}

static void
painter_open_target_destroyed (GtkWidget *widget, gpointer data)
{
  (void) widget;
  *((gboolean *) data) = TRUE;
}

static GtkWidget *
gimp_single_window_strategy_show_dockable_dialog (GimpWindowStrategy *strategy,
                                                  Gimp               *gimp,
                                                  GimpDialogFactory  *factory,
                                                  GdkMonitor         *monitor,
                                                  const gchar        *identifiers)
{
  GList           *windows = gimp_get_image_windows (gimp);
  GtkWidget       *widget  = NULL;
  GimpImageWindow *window;
  gboolean         window_destroyed = FALSE;
  gulong           window_handler;

  g_return_val_if_fail (windows != NULL, NULL);

  /* In single-window mode, there should only be one window... */
  window = g_object_ref (GIMP_IMAGE_WINDOW (windows->data));
  window_handler = g_signal_connect (window, "destroy",
                                      G_CALLBACK (painter_open_target_destroyed),
                                      &window_destroyed);

  if (strcmp ("gimp-toolbox", identifiers) == 0)
    {
      /* Only allow one toolbox... */
      if (! gimp_image_window_has_toolbox (window))
        {
          GimpDockColumns *columns;
          GimpUIManager   *ui_manager = menus_get_image_manager_singleton (gimp);

          widget = gimp_dialog_factory_dialog_new (factory, monitor,
                                                   ui_manager,
                                                   GTK_WIDGET (window),
                                                   "gimp-toolbox",
                                                   -1 /*view_size*/,
                                                   FALSE /*present*/);
          gtk_widget_show (widget);

          columns = gimp_image_window_get_left_docks (window);
          gimp_dock_columns_add_dock (columns,
                                      GIMP_DOCK (widget),
                                      -1 /*index*/);
        }
      else
        {
          widget = gimp_dialog_factory_find_widget (factory, "gimp-toolbox");
        }
    }
  else if (gimp_dialog_factory_find_widget (factory, identifiers))
    {
      /* Existing profiles can keep a book in the now-hidden left toolbox.
       * An explicit Open action transfers that whole book (including siblings)
       * to the on-canvas collection; detached floating docks are not touched. */
      GtkWidget *existing = g_object_ref (gimp_dialog_factory_find_widget (factory, identifiers));
      gboolean widget_destroyed = FALSE;
      gulong widget_handler = g_signal_connect (existing, "destroy",
                                                G_CALLBACK (painter_open_target_destroyed),
                                                &widget_destroyed);
      widget = existing;
      if (gimp->config->painter_canvas_ui && GIMP_IS_DOCKABLE (widget) &&
          gtk_widget_is_ancestor (widget, GTK_WIDGET (gimp_image_window_get_left_docks (window))))
        {
          GimpDockbook *book = gimp_dockable_get_dockbook (GIMP_DOCKABLE (widget));
          GimpDock *old_dock = book ? gimp_dockbook_get_dock (book) : NULL;
          GimpDockColumns *right = gimp_image_window_get_right_docks (window);
          GList *docks = gimp_dock_columns_get_docks (right);
          if (book && old_dock)
            {
              GimpDock *target = docks ? GIMP_DOCK (docks->data) :
                                        GIMP_DOCK (gimp_menu_dock_new ());
              gboolean new_target = docks == NULL;
              gboolean book_destroyed = FALSE, target_destroyed = FALSE;
              gboolean right_destroyed = FALSE;
              gulong book_handler, target_handler, right_handler;
              g_object_ref (right);
              right_handler = g_signal_connect (right, "destroy",
                                                 G_CALLBACK (painter_open_target_destroyed),
                                                 &right_destroyed);
              g_object_ref (book);
              if (new_target)
                g_object_ref_sink (target);
              else
                g_object_ref (target);
              book_handler = g_signal_connect (book, "destroy", G_CALLBACK (painter_open_target_destroyed), &book_destroyed);
              target_handler = g_signal_connect (target, "destroy", G_CALLBACK (painter_open_target_destroyed), &target_destroyed);
              /* A restored profile may have no right-hand docks yet. Create
               * a destination for the entire book, not another Tool Options
               * instance or a second, empty placeholder book. */
              if (new_target)
                gimp_dock_columns_add_dock (right, target, -1);
              if (! window_destroyed && ! right_destroyed &&
                  ! book_destroyed && ! target_destroyed &&
                  gtk_widget_is_ancestor (GTK_WIDGET (target), GTK_WIDGET (right)) &&
                  gimp_dockbook_get_dock (book) == old_dock)
                {
                  gimp_dock_remove_book (old_dock, book);
                  if (! window_destroyed && ! right_destroyed &&
                      ! book_destroyed && ! target_destroyed &&
                      gtk_widget_is_ancestor (GTK_WIDGET (target), GTK_WIDGET (right)) &&
                      ! gtk_widget_get_parent (GTK_WIDGET (book)))
                    {
                      gimp_dock_add_book (target, book, -1);
                      if (! window_destroyed && ! right_destroyed &&
                          ! book_destroyed && ! target_destroyed &&
                          gtk_widget_is_ancestor (GTK_WIDGET (target), GTK_WIDGET (right)) &&
                          gimp_dockbook_get_dock (book) == target)
                        gtk_widget_show (GTK_WIDGET (target));
                    }
                }
              if (g_signal_handler_is_connected (book, book_handler))
                g_signal_handler_disconnect (book, book_handler);
              if (g_signal_handler_is_connected (target, target_handler))
                g_signal_handler_disconnect (target, target_handler);
              if (g_signal_handler_is_connected (right, right_handler))
                g_signal_handler_disconnect (right, right_handler);
              g_object_unref (target);
              g_object_unref (book);
              g_object_unref (right);
            }
        }
      widget = NULL;
      if (! window_destroyed && ! widget_destroyed)
        widget = gimp_dialog_factory_dialog_raise (factory, monitor,
                                                   GTK_WIDGET (window),
                                                   identifiers, -1);
      /* A reentrant receiver can detach without destroying the dockable.
       * Do not return storage kept alive only by this temporary lease. */
      if (widget_destroyed ||
          (GIMP_IS_DOCKABLE (existing) &&
           ! gimp_dockable_get_dockbook (GIMP_DOCKABLE (existing))))
        widget = NULL;
      if (g_signal_handler_is_connected (existing, widget_handler))
        g_signal_handler_disconnect (existing, widget_handler);
      g_object_unref (existing);
   }
  else
    {
      GtkWidget *dockbook;

      dockbook = gimp_image_window_get_default_dockbook (window);
      /* The usual fallback is the left toolbox, which Painter mode hides.
       * New dockables belong to the whole right-side collection instead. */
      if (gimp->config->painter_canvas_ui && dockbook &&
          ! gtk_widget_is_ancestor (dockbook,
                                    GTK_WIDGET (gimp_image_window_get_right_docks (window))))
        dockbook = NULL;

      if (! dockbook)
        {
          GimpDockColumns *dock_columns;

          /* No dock, need to add one */
          dock_columns = gimp_image_window_get_right_docks (window);
          gimp_dock_columns_prepare_dockbook (dock_columns,
                                              -1 /*index*/,
                                              &dockbook);
        }

      widget = gimp_dockbook_add_from_dialog_factory (GIMP_DOCKBOOK (dockbook),
                                                      identifiers);
    }


  if (window_destroyed)
    widget = NULL;
  if (g_signal_handler_is_connected (window, window_handler))
    g_signal_handler_disconnect (window, window_handler);
  g_object_unref (window);
  g_list_free (windows);

  return widget;
}

GimpObject *
gimp_single_window_strategy_get_singleton (void)
{
  static GimpObject *singleton = NULL;

  if (! singleton)
    singleton = g_object_new (GIMP_TYPE_SINGLE_WINDOW_STRATEGY, NULL);

  return singleton;
}
