/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpdock.c
 * Copyright (C) 2001-2005 Michael Natterer <mitch@gimp.org>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "menus/menus.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpmarshal.h"

#include "gimpdialogfactory.h"
#include "gimpdock.h"
#include "gimpdockable.h"
#include "gimpdockbook.h"
#include "gimpdockcolumns.h"
#include "gimpdockcontainer.h"
#include "gimpdockwindow.h"
#include "gimppanedbox.h"
#include "gimpuimanager.h"
#include "gimpwidgets-utils.h"

#include "gimptitlebaricon-pixbuf.h"

#include "gimp-intl.h"


#define DEFAULT_DOCK_FONT_SCALE  PANGO_SCALE_SMALL


enum
{
  BOOK_ADDED,
  BOOK_REMOVED,
  DESCRIPTION_INVALIDATED,
  GEOMETRY_INVALIDATED,
  LAST_SIGNAL
};


typedef struct _GimpDockTitlebar GimpDockTitlebar;
struct _GimpDockTitlebar
{
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *button;
};

struct _GimpDockPrivate
{
  GtkWidget         *temp_vbox;
  GtkWidget         *main_vbox;
  GtkWidget         *paned_vbox;

  GList             *dockbooks;

  gint               ID;
  gchar             *name;
  GimpDockTitlebar  titlebar;
  GimpDockTitlebar  shaded_titlebar;
  GtkWidget         *content_hbox;
  
  gboolean           shaded;
  gint               orig_w, orig_h;
};


static void       gimp_dock_dispose                (GObject      *object);

static void       gimp_dock_style_set              (GtkWidget    *widget,
                                                    GtkStyle     *prev_style);
static gchar    * gimp_dock_real_get_description   (GimpDock     *dock,
                                                    gboolean      complete);
static void       gimp_dock_real_book_added        (GimpDock     *dock,
                                                    GimpDockbook *dockbook);
static void       gimp_dock_real_book_removed      (GimpDock     *dock,
                                                    GimpDockbook *dockbook);
static void       gimp_dock_invalidate_description (GimpDock     *dock);
static gboolean   gimp_dock_dropped_cb             (GtkWidget    *source,
                                                    gint          insert_index,
                                                    gpointer      data);
static void       gimp_dock_orientation_notify     (GimpDock     *dock,
                                                    void         *data);

/* Gimp-painter-2.7: Functions for title bar implementation */
static void              gimp_dock_titlebar_init          (GimpDockTitlebar *titlebar, 
                                                            gboolean           shaded, 
                                                            GObject           *dock);
#if 0
static GimpDockColumns * gimp_dock_get_dock_columns       (GimpDock     *dock);
#endif
static void              gimp_dock_titlebar_set_shaded (GimpDock *dock, gboolean shaded);
static void              gimp_dock_titlebar_button_clicked (GtkWidget   *source, GimpDock *data);
static void              gimp_dock_titlebar_update_description (GimpDockTitlebar* titlebar, GimpDock *dock);

G_DEFINE_TYPE (GimpDock, gimp_dock, GTK_TYPE_BOX)

#define parent_class gimp_dock_parent_class

static guint dock_signals[LAST_SIGNAL] = { 0 };


static void
gimp_dock_class_init (GimpDockClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  dock_signals[BOOK_ADDED] =
    g_signal_new ("book-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDockClass, book_added),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_DOCKBOOK);

  dock_signals[BOOK_REMOVED] =
    g_signal_new ("book-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDockClass, book_removed),
                  NULL, NULL,
                  gimp_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GIMP_TYPE_DOCKBOOK);

  dock_signals[DESCRIPTION_INVALIDATED] =
    g_signal_new ("description-invalidated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDockClass, description_invalidated),
                  NULL, NULL,
                  gimp_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  dock_signals[GEOMETRY_INVALIDATED] =
    g_signal_new ("geometry-invalidated",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GimpDockClass, geometry_invalidated),
                  NULL, NULL,
                  gimp_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  object_class->dispose          = gimp_dock_dispose;

  widget_class->style_set        = gimp_dock_style_set;

  klass->get_description         = gimp_dock_real_get_description;
  klass->set_host_geometry_hints = NULL;
  klass->book_added              = gimp_dock_real_book_added;
  klass->book_removed            = gimp_dock_real_book_removed;
  klass->description_invalidated = NULL;
  klass->geometry_invalidated    = NULL;

  gtk_widget_class_install_style_property (widget_class,
                                           g_param_spec_double ("font-scale",
                                                                NULL, NULL,
                                                                0.0,
                                                                G_MAXDOUBLE,
                                                                DEFAULT_DOCK_FONT_SCALE,
                                                                GIMP_PARAM_READABLE));

  g_type_class_add_private (klass, sizeof (GimpDockPrivate));
}

static void
gimp_dock_init (GimpDock *dock)
{
  static gint  dock_ID = 1;
  gchar       *name    = NULL;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (dock),
                                  GTK_ORIENTATION_VERTICAL);

  dock->p = G_TYPE_INSTANCE_GET_PRIVATE (dock,
                                         GIMP_TYPE_DOCK,
                                         GimpDockPrivate);
  dock->p->ID             = dock_ID++;
  dock->p->shaded         = FALSE;

  name = g_strdup_printf ("gimp-internal-dock-%d", dock->p->ID);
  gtk_widget_set_name (GTK_WIDGET (dock), name);
  g_free (name);

  dock->p->temp_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (dock), dock->p->temp_vbox, FALSE, FALSE, 0);
  /* Never show it */

  dock->p->main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (dock), dock->p->main_vbox, TRUE, TRUE, 0);
  gtk_widget_show (dock->p->main_vbox);

  /* Titlebar */
  gimp_dock_titlebar_init (&dock->p->titlebar, FALSE, G_OBJECT (dock));
  gtk_box_pack_start (GTK_BOX (dock->p->main_vbox), dock->p->titlebar.box, FALSE, TRUE, 0);
  gtk_widget_show (dock->p->titlebar.box);

  /* Contents holder */
  dock->p->content_hbox = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (dock->p->main_vbox), dock->p->content_hbox, TRUE, TRUE, 0);
  gtk_widget_show (dock->p->content_hbox);

  /* Shaded titlebar */
  gimp_dock_titlebar_init (&dock->p->shaded_titlebar, TRUE, G_OBJECT (dock));
  gtk_box_pack_start (GTK_BOX (dock->p->content_hbox), dock->p->shaded_titlebar.box, FALSE, TRUE, 0);
  gtk_widget_hide (dock->p->shaded_titlebar.box);

  dock->p->orig_w = -1;
  dock->p->orig_h = -1;

  /* Contents */

  dock->p->paned_vbox = gimp_paned_box_new (FALSE, 0, GTK_ORIENTATION_VERTICAL);
  gimp_paned_box_set_dropped_cb (GIMP_PANED_BOX (dock->p->paned_vbox),
                                 gimp_dock_dropped_cb,
                                 dock);

  gtk_container_add (GTK_CONTAINER (dock->p->content_hbox), dock->p->paned_vbox);
  gtk_widget_show (dock->p->paned_vbox);

  g_signal_connect(G_OBJECT(dock), "notify::orientation", G_CALLBACK(gimp_dock_orientation_notify), NULL);
}

static GtkWidget *
gimp_dock_create_icon_from_data (const GdkPixdata* data)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_from_pixdata (data, TRUE, NULL);
  GtkWidget *image   = gtk_image_new_from_pixbuf (pixbuf);
  return image;
}

static void
gimp_dock_titlebar_init (GimpDockTitlebar *titlebar, gboolean shaded, GObject *dock)
{
  GtkWidget *box;
  GtkWidget *image;
  GtkWidget *frame;
  GdkColor   fg_color = { 0, 0xffff, 0xffff, 0xffff};
  GdkColor   bg_color = { 0, 0x8000, 0x8000, 0x8000};
  
  titlebar->box = gtk_event_box_new ();
  frame         = gtk_frame_new (NULL);
  gtk_container_add (GTK_CONTAINER(titlebar->box), frame); 
  gtk_widget_show (frame);

  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_OUT);
  
  if (!shaded)
    {
      box           = gtk_hbox_new (FALSE, 3);
      gtk_container_add (GTK_CONTAINER(frame), box);
      gtk_widget_show (box);

      titlebar->label    = gtk_label_new (gtk_widget_get_name (GTK_WIDGET (dock)));
      gtk_box_pack_start (GTK_BOX (box), titlebar->label, TRUE, TRUE, 0);
      gtk_misc_set_alignment (GTK_MISC (titlebar->label), 0.0, 0.5);
      gtk_widget_show (titlebar->label);
      
      titlebar->button = gtk_button_new ();
      gtk_button_set_relief (GTK_BUTTON (titlebar->button), GTK_RELIEF_NONE);
      image = gimp_dock_create_icon_from_data (&gimp_dock_titlebar_minimize);
      gtk_button_set_image (GTK_BUTTON (titlebar->button), image);
      gtk_box_pack_end (GTK_BOX (box), titlebar->button, FALSE, FALSE, 0);
      g_signal_connect_object (G_OBJECT (titlebar->button), "clicked", 
                               G_CALLBACK(gimp_dock_titlebar_button_clicked), 
                               G_OBJECT (dock), 0);
      gtk_widget_show (titlebar->button);
    }
  else
    {
      box           = gtk_vbox_new (FALSE, 3);
      gtk_container_add (GTK_CONTAINER(frame), box);
      gtk_widget_show (box);
      
      titlebar->button = gtk_button_new ();
      gtk_button_set_relief (GTK_BUTTON (titlebar->button), GTK_RELIEF_NONE);
      image = gimp_dock_create_icon_from_data (&gimp_dock_titlebar_maximize);
      gtk_button_set_image (GTK_BUTTON (titlebar->button), image);
      gtk_box_pack_start (GTK_BOX (box), titlebar->button, FALSE, TRUE, 0);
      g_signal_connect_object (G_OBJECT (titlebar->button), "clicked", 
                               G_CALLBACK(gimp_dock_titlebar_button_clicked), 
                               G_OBJECT (dock), 0);
      gtk_widget_show (titlebar->button);

      titlebar->label    = gtk_label_new (gtk_widget_get_name (GTK_WIDGET (dock)));
      gtk_box_pack_start (GTK_BOX (box), titlebar->label, TRUE, TRUE, 0);
      gtk_widget_show (titlebar->label);
      gtk_misc_set_alignment (GTK_MISC (titlebar->label), 0.5, 0.0);
      gtk_label_set_angle (GTK_LABEL (titlebar->label), 90);
    }
    
    gtk_widget_modify_bg (titlebar->box, GTK_STATE_NORMAL, &bg_color);
    gtk_widget_modify_fg (titlebar->label, GTK_STATE_NORMAL, &fg_color);
  
}

static void
gimp_dock_dispose (GObject *object)
{
  GimpDock *dock = GIMP_DOCK (object);

  while (dock->p->dockbooks)
    {
      GimpDockbook *dockbook = dock->p->dockbooks->data;

      g_object_ref (dockbook);
      gimp_dock_remove_book (dock, dockbook);
      gtk_widget_destroy (GTK_WIDGET (dockbook));
      g_object_unref (dockbook);
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_dock_style_set (GtkWidget *widget,
                     GtkStyle  *prev_style)
{
  GimpDock *dock       = GIMP_DOCK (widget);
  gdouble   font_scale = 1.0;

  GTK_WIDGET_CLASS (parent_class)->style_set (widget, prev_style);

  gtk_widget_style_get (widget,
                        "font-scale", &font_scale,
                        NULL);

  if (font_scale != 1.0)
    {
      PangoContext         *context;
      PangoFontDescription *font_desc;
      gint                  font_size;
      gchar                *font_str;
      gchar                *rc_string;

      context = gtk_widget_get_pango_context (widget);
      font_desc = pango_context_get_font_description (context);
      font_desc = pango_font_description_copy (font_desc);

      font_size = pango_font_description_get_size (font_desc);
      font_size = font_scale * font_size;
      pango_font_description_set_size (font_desc, font_size);

      font_str = pango_font_description_to_string (font_desc);
      pango_font_description_free (font_desc);

      rc_string =
        g_strdup_printf ("style \"gimp-dock-style\""
                         "{"
                         "  font_name = \"%s\""
                         "}"
                         "widget \"*.gimp-internal-dock-%d.*\" style \"gimp-dock-style\"",
                         font_str,
                         dock->p->ID);
      g_free (font_str);

      gtk_rc_parse_string (rc_string);
      g_free (rc_string);

      gtk_widget_reset_rc_styles (widget);
    }
}

static gchar *
gimp_dock_real_get_description (GimpDock *dock,
                                gboolean  complete)
{
  GString *desc;
  GList   *list;

  desc = g_string_new (NULL);

  for (list = gimp_dock_get_dockbooks (dock);
       list;
       list = g_list_next (list))
    {
      GimpDockbook *dockbook = list->data;
      GList        *children;
      GList        *child;

      if (complete)
        {
          /* Include all dockables */
          children = gtk_container_get_children (GTK_CONTAINER (dockbook));
        }
      else
        {
          GtkWidget *dockable = NULL;
          gint       page_num = 0;

          page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (dockbook));
          dockable = gtk_notebook_get_nth_page (GTK_NOTEBOOK (dockbook), page_num);

          /* Only include active dockables */
          children = g_list_append (NULL, dockable);
        }

      for (child = children; child; child = g_list_next (child))
        {
          GimpDockable *dockable = child->data;

          g_string_append (desc, gimp_dockable_get_name (dockable));

          if (g_list_next (child))
            g_string_append (desc, GIMP_DOCK_DOCKABLE_SEPARATOR);
        }

      g_list_free (children);

      if (g_list_next (list))
        g_string_append (desc, GIMP_DOCK_BOOK_SEPARATOR);
    }

  return g_string_free (desc, FALSE);
}

static void
gimp_dock_real_book_added (GimpDock     *dock,
                           GimpDockbook *dockbook)
{
  g_signal_connect_object (dockbook, "switch-page",
                           G_CALLBACK (gimp_dock_invalidate_description),
                           dock, G_CONNECT_SWAPPED);
}

static void
gimp_dock_real_book_removed (GimpDock     *dock,
                             GimpDockbook *dockbook)
{
  g_signal_handlers_disconnect_by_func (dockbook,
                                        gimp_dock_invalidate_description,
                                        dock);
}

static void
gimp_dock_invalidate_description (GimpDock *dock)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  
  if (dock->p->shaded)
    gimp_dock_titlebar_update_description (&dock->p->shaded_titlebar, dock);
  else
    gimp_dock_titlebar_update_description (&dock->p->titlebar, dock);

  g_signal_emit (dock, dock_signals[DESCRIPTION_INVALIDATED], 0);
}

static gboolean
gimp_dock_dropped_cb (GtkWidget *source,
                      gint       insert_index,
                      gpointer   data)
{
  GimpDock     *dock     = GIMP_DOCK (data);
  GimpDockable *dockable = gimp_dockbook_drag_source_to_dockable (source);
  GtkWidget    *dockbook = NULL;

  if (!dockable )
    return FALSE;

  /*  if dropping to the same dock, take care that we don't try
   *  to reorder the *only* dockable in the dock
   */
  if (gimp_dockbook_get_dock (gimp_dockable_get_dockbook (dockable)) == dock)
    {
      GList *children;
      gint   n_books;
      gint   n_dockables;

      n_books = g_list_length (gimp_dock_get_dockbooks (dock));

      children = gtk_container_get_children (GTK_CONTAINER (gimp_dockable_get_dockbook (dockable)));
      n_dockables = g_list_length (children);
      g_list_free (children);

      if (n_books == 1 && n_dockables == 1)
        return TRUE; /* successfully do nothing */
    }

  /* Detach the dockable from the old dockbook */
  g_object_ref (dockable);
  gimp_dockbook_remove (gimp_dockable_get_dockbook (dockable), dockable);

  /* Create a new dockbook */
  dockbook = gimp_dockbook_new (global_menu_factory);
  gimp_dock_add_book (dock, GIMP_DOCKBOOK (dockbook), insert_index);

  /* Add the dockable to new new dockbook */
  gimp_dockbook_add (GIMP_DOCKBOOK (dockbook), dockable, -1);
  g_object_unref (dockable);

  return TRUE;
}


/*  public functions  */

/**
 * gimp_dock_get_description:
 * @dock:
 * @complete: If %TRUE, only includes the active dockables, i.e. not the
 *            dockables in a non-active GtkNotebook tab
 *
 * Returns: A string describing the contents of the dock.
 **/
gchar *
gimp_dock_get_description (GimpDock *dock,
                           gboolean  complete)
{
  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  if (GIMP_DOCK_GET_CLASS (dock)->get_description)
    return GIMP_DOCK_GET_CLASS (dock)->get_description (dock, complete);

  return NULL;
}

/**
 * gimp_dock_set_host_geometry_hints:
 * @dock:   The dock
 * @window: The #GtkWindow to adapt to hosting the dock
 *
 * Some docks have some specific needs on the #GtkWindow they are
 * in. This function allows such docks to perform any such setup on
 * the #GtkWindow they are in/will be put in.
 **/
void
gimp_dock_set_host_geometry_hints (GimpDock  *dock,
                                   GtkWindow *window)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GTK_IS_WINDOW (window));

  if (GIMP_DOCK_GET_CLASS (dock)->set_host_geometry_hints)
    GIMP_DOCK_GET_CLASS (dock)->set_host_geometry_hints (dock, window);
}

/**
 * gimp_dock_invalidate_geometry:
 * @dock:
 *
 * Call when the dock needs to setup its host #GtkWindow with
 * GtkDock::set_host_geometry_hints().
 **/
void
gimp_dock_invalidate_geometry (GimpDock *dock)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));

  g_signal_emit (dock, dock_signals[GEOMETRY_INVALIDATED], 0);
}

/**
 * gimp_dock_update_with_context:
 * @dock:
 * @context:
 *
 * Set the @context on all dockables in the @dock.
 **/
void
gimp_dock_update_with_context (GimpDock    *dock,
                               GimpContext *context)
{
  GList *iter = NULL;

  for (iter = gimp_dock_get_dockbooks (dock);
       iter;
       iter = g_list_next (iter))
    {
      GimpDockbook *dockbook = GIMP_DOCKBOOK (iter->data);

      gimp_dockbook_update_with_context (dockbook, context);
    }
}

/**
 * gimp_dock_get_context:
 * @dock:
 *
 * Returns: The #GimpContext for the #GimpDockWindow the @dock is in.
 **/
GimpContext *
gimp_dock_get_context (GimpDock *dock)
{
  GimpContext *context = NULL;

  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  /* First try GimpDockColumns */
  if (! context)
    {
      GimpDockColumns *dock_columns;

      dock_columns =
        GIMP_DOCK_COLUMNS (gtk_widget_get_ancestor (GTK_WIDGET (dock),
                                                    GIMP_TYPE_DOCK_COLUMNS));

      if (dock_columns)
        context = gimp_dock_columns_get_context (dock_columns);
    }

  /* Then GimpDockWindow */
  if (! context)
    {
      GimpDockWindow *dock_window = gimp_dock_window_from_dock (dock);

      if (dock_window)
        context = gimp_dock_window_get_context (dock_window);
    }

  return context;
}

/**
 * gimp_dock_get_dialog_factory:
 * @dock:
 *
 * Returns: The #GimpDialogFactory for the #GimpDockWindow the @dock
 *          is in.
 **/
GimpDialogFactory *
gimp_dock_get_dialog_factory (GimpDock *dock)
{
  GimpDialogFactory *dialog_factory = NULL;

  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  /* First try GimpDockColumns */
  if (! dialog_factory)
    {
      GimpDockColumns *dock_columns;

      dock_columns =
        GIMP_DOCK_COLUMNS (gtk_widget_get_ancestor (GTK_WIDGET (dock),
                                                    GIMP_TYPE_DOCK_COLUMNS));

      if (dock_columns)
        dialog_factory = gimp_dock_columns_get_dialog_factory (dock_columns);
    }

  /* Then GimpDockWindow */
  if (! dialog_factory)
    {
      GimpDockWindow *dock_window = gimp_dock_window_from_dock (dock);

      if (dock_window)
        dialog_factory = gimp_dock_window_get_dialog_factory (dock_window);
    }

  return dialog_factory;
}

/**
 * gimp_dock_get_ui_manager:
 * @dock:
 *
 * Returns: The #GimpUIManager for the #GimpDockWindow the @dock is
 *          in.
 **/
GimpUIManager *
gimp_dock_get_ui_manager (GimpDock *dock)
{
  GimpUIManager *ui_manager = NULL;

  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  /* First try GimpDockColumns */
  if (! ui_manager)
    {
      GimpDockColumns *dock_columns;

      dock_columns =
        GIMP_DOCK_COLUMNS (gtk_widget_get_ancestor (GTK_WIDGET (dock),
                                                    GIMP_TYPE_DOCK_COLUMNS));

      if (dock_columns)
        ui_manager = gimp_dock_columns_get_ui_manager (dock_columns);
    }

  /* Then GimpDockContainer */
  if (! ui_manager)
    {
      GimpDockWindow *dock_window = gimp_dock_window_from_dock (dock);

      if (dock_window)
        {
          GimpDockContainer *dock_container = GIMP_DOCK_CONTAINER (dock_window);

          ui_manager = gimp_dock_container_get_ui_manager (dock_container);
        }
    }

  return ui_manager;
}

GList *
gimp_dock_get_dockbooks (GimpDock *dock)
{
  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  return dock->p->dockbooks;
}

gint
gimp_dock_get_n_dockables (GimpDock *dock)
{
  GList *list = NULL;
  gint   n    = 0;

  g_return_val_if_fail (GIMP_IS_DOCK (dock), 0);

  for (list = dock->p->dockbooks; list; list = list->next)
    n += gtk_notebook_get_n_pages (GTK_NOTEBOOK (list->data));

  return n;
}

GtkWidget *
gimp_dock_get_main_vbox (GimpDock *dock)
{
  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  return dock->p->main_vbox;
}

GtkWidget *
gimp_dock_get_vbox (GimpDock *dock)
{
  g_return_val_if_fail (GIMP_IS_DOCK (dock), NULL);

  return dock->p->paned_vbox;
}

gint
gimp_dock_get_id (GimpDock *dock)
{
  g_return_val_if_fail (GIMP_IS_DOCK (dock), 0);

  return dock->p->ID;
}

void
gimp_dock_set_id (GimpDock *dock,
                  gint      ID)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));

  dock->p->ID = ID;
}

void
gimp_dock_add (GimpDock     *dock,
               GimpDockable *dockable,
               gint          section,
               gint          position)
{
  GimpDockbook *dockbook;

  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKABLE (dockable));
  g_return_if_fail (gimp_dockable_get_dockbook (dockable) == NULL);

  dockbook = GIMP_DOCKBOOK (dock->p->dockbooks->data);

  gimp_dockbook_add (dockbook, dockable, position);
}

void
gimp_dock_remove (GimpDock     *dock,
                  GimpDockable *dockable)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKABLE (dockable));
  g_return_if_fail (gimp_dockable_get_dockbook (dockable) != NULL);
  g_return_if_fail (gimp_dockbook_get_dock (gimp_dockable_get_dockbook (dockable)) == dock);

  gimp_dockbook_remove (gimp_dockable_get_dockbook (dockable), dockable);
}

void
gimp_dock_add_book (GimpDock     *dock,
                    GimpDockbook *dockbook,
                    gint          index)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKBOOK (dockbook));
  g_return_if_fail (gimp_dockbook_get_dock (dockbook) == NULL);

  gimp_dockbook_set_dock (dockbook, dock);

  g_signal_connect_object (dockbook, "dockable-added",
                           G_CALLBACK (gimp_dock_invalidate_description),
                           dock, G_CONNECT_SWAPPED);
  g_signal_connect_object (dockbook, "dockable-removed",
                           G_CALLBACK (gimp_dock_invalidate_description),
                           dock, G_CONNECT_SWAPPED);
  g_signal_connect_object (dockbook, "dockable-reordered",
                           G_CALLBACK (gimp_dock_invalidate_description),
                           dock, G_CONNECT_SWAPPED);

  dock->p->dockbooks = g_list_insert (dock->p->dockbooks, dockbook, index);
  gimp_paned_box_add_widget (GIMP_PANED_BOX (dock->p->paned_vbox),
                             GTK_WIDGET (dockbook),
                             index);
  gtk_widget_show (GTK_WIDGET (dockbook));

  gimp_dock_invalidate_description (dock);

  g_signal_emit (dock, dock_signals[BOOK_ADDED], 0, dockbook);
}

void
gimp_dock_remove_book (GimpDock     *dock,
                       GimpDockbook *dockbook)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GIMP_IS_DOCKBOOK (dockbook));
  g_return_if_fail (gimp_dockbook_get_dock (dockbook) == dock);

  gimp_dockbook_set_dock (dockbook, NULL);

  g_signal_handlers_disconnect_by_func (dockbook,
                                        gimp_dock_invalidate_description,
                                        dock);

  /* Ref the dockbook so we can emit the "book-removed" signal and
   * pass it as a parameter before it's destroyed
   */
  g_object_ref (dockbook);

  dock->p->dockbooks = g_list_remove (dock->p->dockbooks, dockbook);
  gimp_paned_box_remove_widget (GIMP_PANED_BOX (dock->p->paned_vbox),
                                GTK_WIDGET (dockbook));

  gimp_dock_invalidate_description (dock);

  g_signal_emit (dock, dock_signals[BOOK_REMOVED], 0, dockbook);

  g_object_unref (dockbook);
}

/**
 * gimp_dock_temp_add:
 * @dock:
 * @widget:
 *
 * Method to temporarily add a widget to the dock, for example to make
 * font-scale style property to be applied temporarily to the
 * child.
 **/
void
gimp_dock_temp_add (GimpDock  *dock,
                    GtkWidget *child)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_box_pack_start (GTK_BOX (dock->p->temp_vbox), child, FALSE, FALSE, 0);
}

/**
 * gimp_dock_temp_remove:
 * @dock:
 * @child:
 *
 * Removes a temporarly child added with gimp_dock_temp_add().
 **/
void
gimp_dock_temp_remove (GimpDock  *dock,
                       GtkWidget *child)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  g_return_if_fail (GTK_IS_WIDGET (child));

  gtk_container_remove (GTK_CONTAINER (dock->p->temp_vbox), child);
}

static void
gimp_dock_titlebar_set_shaded (GimpDock *dock, gboolean shaded)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));
  gint w, h;
  GtkWidget *parent_paned = NULL;
  GtkWidget *paned_child  = NULL;
  gboolean   resize;
   
  if (dock->p->shaded == shaded)
    return;
  
  dock->p->shaded = shaded;

  parent_paned = gtk_widget_get_parent (GTK_WIDGET (dock));
  paned_child  = GTK_WIDGET (dock);
  while (parent_paned && !GTK_IS_PANED (parent_paned))
    {
      paned_child  = parent_paned;
      parent_paned = gtk_widget_get_parent (parent_paned);
    }
 
  if (!dock->p->shaded)
    {
      if (parent_paned)
        {
          gtk_container_child_get (GTK_CONTAINER (parent_paned),
                                   paned_child, "resize", &resize, NULL);
          gtk_container_child_set (GTK_CONTAINER (parent_paned),
                                   paned_child,
                                   "resize", FALSE, NULL);
        }
      gtk_widget_show (dock->p->paned_vbox);
      gtk_widget_hide (dock->p->shaded_titlebar.box);
      gtk_widget_show (dock->p->titlebar.box);
      gtk_widget_set_size_request (GTK_WIDGET (dock), dock->p->orig_w, dock->p->orig_h);
      gtk_widget_queue_resize (GTK_WIDGET (dock));
      gimp_dock_titlebar_update_description (&dock->p->titlebar, dock);
    }
  else
    {
      if (parent_paned)
        {
          gtk_container_child_get (GTK_CONTAINER (parent_paned),
                                   paned_child, "resize", &resize, NULL);
          gtk_container_child_set (GTK_CONTAINER (parent_paned),
                                   paned_child,
                                   "resize", FALSE, NULL);
          g_object_set (G_OBJECT (parent_paned), "position-set", FALSE, NULL);
          
          paned_child = NULL;
          while (parent_paned)
            {
              if (GTK_IS_PANED (parent_paned))
                {
                  if (paned_child)
                    g_object_set (G_OBJECT (paned_child), "position-set", FALSE, NULL);
                  paned_child = parent_paned;
                }
              parent_paned = gtk_widget_get_parent (parent_paned);
            }
/*  
          if (paned_child)
            g_object_set (G_OBJECT (paned_child), "position-set", FALSE, NULL);
*/
      }

      parent_paned = GTK_WIDGET (dock);
      while (parent_paned)
        {
          if (GIMP_IS_DOCK_WINDOW (parent_paned))
              gtk_window_resize (GTK_WINDOW (parent_paned), 1, 1);

          parent_paned = gtk_widget_get_parent (parent_paned);
        }
      gimp_dock_titlebar_update_description (&dock->p->shaded_titlebar, dock);

      gtk_widget_hide (dock->p->paned_vbox);
      gtk_widget_show (dock->p->shaded_titlebar.box);
      gtk_widget_hide (dock->p->titlebar.box);
      gtk_widget_get_size_request (GTK_WIDGET (dock), &dock->p->orig_w, &dock->p->orig_h);
      gtk_widget_get_size_request (dock->p->shaded_titlebar.box, &w, &h);
      gtk_widget_set_size_request (GTK_WIDGET (dock), w, h);
      gtk_widget_queue_resize (GTK_WIDGET (dock));
    }
}

/**
 * gimp_dock_minimize_button_clicked:
 */
static void
gimp_dock_titlebar_button_clicked (GtkWidget   *source, 
                                   GimpDock *dock)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));

  gimp_dock_titlebar_set_shaded (dock, !dock->p->shaded);
}

static void
gimp_dock_titlebar_update_description (GimpDockTitlebar* titlebar, GimpDock *dock)
{
  gchar *desc = gimp_dock_get_description (dock, TRUE);
  gtk_label_set_label (GTK_LABEL (titlebar->label), desc);
  g_free (desc);
}

gboolean
gimp_dock_is_shaded (GimpDock *dock)
{
  if (!GIMP_IS_DOCK (dock))
    return FALSE;
  return dock->p->shaded;
}

void
gimp_dock_set_shaded (GimpDock *dock, gboolean shaded)
{
  g_return_if_fail (GIMP_IS_DOCK (dock));

  gimp_dock_titlebar_set_shaded (dock, shaded);
}

static void
gimp_dock_orientation_notify     (GimpDock     *dock,
                                  void         *data)
{
  GtkOrientation orientation = gtk_orientable_get_orientation(GTK_ORIENTABLE(dock));
  gtk_orientable_set_orientation(GTK_ORIENTABLE(dock->p->paned_vbox), orientation); 
}