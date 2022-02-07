/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpToolOptionsToolbar.c
 * Copyright (C) 2003 Michael Natterer <mitch@gimp.org>
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

#include <gtk/gtk.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimplist.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimptoolpreset.h"

#include "gimpdnd.h"
#include "gimpdocked.h"
#include "gimphelp-ids.h"
#include "gimpmenufactory.h"
#include "gimppropwidgets.h"
#include "gimpview.h"
#include "gimpviewrenderer.h"
#include "gimptooloptionstoolbar.h"
#include "gimpuimanager.h"
#include "gimpwidgets-utils.h"

#include "gimp-intl.h"

enum
{
  PROP_0,
  PROP_GIMP,
};

struct _GimpToolOptionsToolbarPrivate
{
  Gimp            *gimp;

  GimpToolOptions *visible_tool_options;
  
  GtkWidget *scrolled_window;
  GtkWidget *view;
  GtkWidget *options_hbox;

  gulong tool_changed_handler;
};


static GObject   * gimp_tool_options_toolbar_constructor       (GType                  type,
                                                               guint                  n_params,
                                                               GObjectConstructParam *params);
static void        gimp_tool_options_toolbar_dispose           (GObject               *object);
static void        gimp_tool_options_toolbar_set_property      (GObject               *object,
                                                               guint                  property_id,
                                                               const GValue          *value,
                                                               GParamSpec            *pspec);
static void        gimp_tool_options_toolbar_get_property      (GObject               *object,
                                                               guint                  property_id,
                                                               GValue                *value,
                                                               GParamSpec            *pspec);

static void        gimp_tool_options_toolbar_tool_changed      (GimpContext           *context,
                                                               GimpToolInfo          *tool_info,
                                                               GimpToolOptionsToolbar *toolbar);
static void    gimp_tool_options_toolbar_hide_toolbar    (GimpToolOptionsToolbar *toolbar);
static void    gimp_tool_options_toolbar_show_toolbar    (GimpToolOptionsToolbar *toolbar);


G_DEFINE_TYPE (GimpToolOptionsToolbar, gimp_tool_options_toolbar, GTK_TYPE_TOOLBAR)

#define parent_class gimp_tool_options_toolbar_parent_class


static void
gimp_tool_options_toolbar_class_init (GimpToolOptionsToolbarClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructor  = gimp_tool_options_toolbar_constructor;
  object_class->dispose      = gimp_tool_options_toolbar_dispose;
  object_class->set_property = gimp_tool_options_toolbar_set_property;
  object_class->get_property = gimp_tool_options_toolbar_get_property;

  g_object_class_install_property (object_class, PROP_GIMP,
                                   g_param_spec_object ("gimp",
                                                        NULL, NULL,
                                                        GIMP_TYPE_GIMP,
                                                        GIMP_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (klass, sizeof (GimpToolOptionsToolbarPrivate));
}

static void
gimp_tool_options_toolbar_init (GimpToolOptionsToolbar *toolbar)
{

  toolbar->p = G_TYPE_INSTANCE_GET_PRIVATE (toolbar,
                                            GIMP_TYPE_TOOL_OPTIONS_TOOLBAR,
                                            GimpToolOptionsToolbarPrivate);
                                           
}

static GObject *
gimp_tool_options_toolbar_constructor (GType                  type,
                                      guint                  n_params,
                                      GObjectConstructParam *params)
{
  GObject               *object       = NULL;
  GimpToolOptionsToolbar *toolbar       = NULL;
  GimpContext           *user_context = NULL;
  GtkToolItem *item;
  GtkLayout     *scrolled_window;

  object = G_OBJECT_CLASS (parent_class)->constructor (type, n_params, params);

  toolbar = GIMP_TOOL_OPTIONS_TOOLBAR (object);

  user_context = gimp_get_user_context (toolbar->p->gimp);


  item = gtk_tool_item_new ();
  gtk_tool_item_set_expand (GTK_TOOL_ITEM (item), TRUE);
  toolbar->p->scrolled_window = gtk_layout_new (NULL, NULL);
  scrolled_window             = GTK_LAYOUT (toolbar->p->scrolled_window);
  gtk_widget_show (toolbar->p->scrolled_window);
  
  /*  The hbox containing the tool options  */
  toolbar->p->options_hbox = gtk_hbox_new (FALSE, 6);
  gtk_layout_put (scrolled_window, toolbar->p->options_hbox, 0, 0);
  gtk_widget_show (toolbar->p->options_hbox);  
  
  /* The toolbar item containing scrolled_window */
  gtk_container_add (GTK_CONTAINER (item), toolbar->p->scrolled_window);
  gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);
  gtk_widget_show (GTK_WIDGET(item));

  g_signal_connect (G_OBJECT(toolbar), "hide", G_CALLBACK(gimp_tool_options_toolbar_hide_toolbar), NULL);
  g_signal_connect (G_OBJECT(toolbar), "show", G_CALLBACK(gimp_tool_options_toolbar_show_toolbar), NULL);

  if (gtk_widget_get_visible(GTK_WIDGET(toolbar))) {
    gimp_tool_options_toolbar_tool_changed (user_context,
                                           gimp_context_get_tool (user_context),
                                           toolbar);
  }

  return object;
}

static void
gimp_tool_options_toolbar_dispose (GObject *object)
{
  GimpToolOptionsToolbar* toolbar = GIMP_TOOL_OPTIONS_TOOLBAR (object);
  g_print("GimpToolOptionsToolbar::dispose\n");
  gimp_tool_options_toolbar_hide_toolbar (toolbar);
  if (toolbar->p->options_hbox) {
    gimp_tool_options_toolbar_hide_toolbar(toolbar);
    toolbar->p->options_hbox = NULL;
  }
/*
  gimp_tool_options_toolbar_save_presets (toolbar);
*/
  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_tool_options_toolbar_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GimpToolOptionsToolbar *toolbar = GIMP_TOOL_OPTIONS_TOOLBAR (object);

  switch (property_id)
    {
    case PROP_GIMP:
      toolbar->p->gimp = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gimp_tool_options_toolbar_get_property (GObject    *object,
                                       guint       property_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GimpToolOptionsToolbar *toolbar = GIMP_TOOL_OPTIONS_TOOLBAR (object);

  switch (property_id)
    {
    case PROP_GIMP:
      g_value_set_object (value, toolbar->p->gimp);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

/*  public functions  */

GtkWidget *
gimp_tool_options_toolbar_new (Gimp            *gimp,
                              GimpMenuFactory *menu_factory)
{
  g_print("GtimpToolOptionsToolbar::new\n");
  g_return_val_if_fail (GIMP_IS_GIMP (gimp), NULL);

  return g_object_new (GIMP_TYPE_TOOL_OPTIONS_TOOLBAR,
                       "gimp",            gimp,
                       NULL);
}

GimpToolOptions *
gimp_tool_options_toolbar_get_tool_options (GimpToolOptionsToolbar *toolbar)
{
  g_print("GtimpToolOptionsToolbar::get_tool_options\n");
  g_return_val_if_fail (GIMP_IS_TOOL_OPTIONS_TOOLBAR (toolbar), NULL);

  return toolbar->p->visible_tool_options;
}


static void
gimp_tool_options_toolbar_tool_changed (GimpContext           *context,
                                       GimpToolInfo          *tool_info,
                                       GimpToolOptionsToolbar *toolbar)
{
  GtkWidget       *options_gui;

//  if (tool_info && tool_info->tool_options == toolbar->p->visible_tool_options)
//    return;
  g_print("GimpToolOptionsToolbar::tool_changed\n");

  if (toolbar->p->visible_tool_options)
    {

      options_gui = g_object_get_data (G_OBJECT (toolbar->p->visible_tool_options),
                                       "gimp-tool-options-toolbar-gui");

      if (options_gui) {
        g_print("GimpToolOptionsToolbar::tool_changed::hide %p\n", options_gui);
        gtk_widget_hide (options_gui);
      }

      toolbar->p->visible_tool_options = NULL;
    }

  if (tool_info && tool_info->tool_options)
    {
      GtkRequisition req;
      
      options_gui = g_object_get_data (G_OBJECT (tool_info->tool_options),
                                       "gimp-tool-options-toolbar-gui");


      if (gtk_widget_get_parent (options_gui) != toolbar->p->options_hbox) {

        GtkWidget* parent = gtk_widget_get_parent(options_gui);
        g_object_ref(G_OBJECT(options_gui));

        if (parent)
          gtk_container_remove(GTK_CONTAINER(parent), options_gui);
        g_print("gimp_tool_options_toolbar_tool_changed: Add %p to toolbar %p.\n", options_gui, toolbar->p->options_hbox);
        gtk_box_pack_start (GTK_BOX (toolbar->p->options_hbox), options_gui,
                            TRUE, FALSE, 0);

        g_object_unref(G_OBJECT(options_gui));
        
      } else {
        g_print("gimp_tool_options_toolbar_tool_changed: options_gui already in use.\n");
      }

      gtk_widget_size_request (options_gui, &req);
      g_print("size[w,h]=[%d,%d]\n", req.width, req.height);
      gtk_widget_set_size_request (toolbar->p->scrolled_window, -1, req.height);
      gtk_widget_show_all (options_gui);
      gtk_widget_show (toolbar->p->scrolled_window);
      gtk_widget_show (toolbar->p->options_hbox);
      gtk_widget_show (toolbar);

      toolbar->p->visible_tool_options = tool_info->tool_options;
    }
  else
    {
      g_print("GimpToolOptionsToolbar::tool_changed, no_visible_tool_options\n");
    }

}

static void
gimp_tool_options_toolbar_hide_toolbar (GimpToolOptionsToolbar *toolbar)
{
  GimpContext            *user_context = gimp_get_user_context (toolbar->p->gimp);

  g_print("GimpToolOptionsToolbar::hide(%p)\n", toolbar);

  if (toolbar->p->options_hbox) {
    GList *options;
    GList *list;

    options =
      gtk_container_get_children (GTK_CONTAINER (toolbar->p->options_hbox));

    for (list = options; list; list = g_list_next (list)) {
        g_object_ref (list->data);
        gtk_container_remove (GTK_CONTAINER (toolbar->p->options_hbox),
                              GTK_WIDGET (list->data));
    }

    g_list_free (options);
  }

  if (toolbar->p->tool_changed_handler) {
    g_signal_handler_disconnect(G_OBJECT(user_context), 
                                toolbar->p->tool_changed_handler);
    toolbar->p->tool_changed_handler = 0;
  }
}

static void
gimp_tool_options_toolbar_show_toolbar (GimpToolOptionsToolbar *toolbar)
{
  GimpContext            *user_context = gimp_get_user_context (toolbar->p->gimp);

  g_print("GimpToolOptionsToolbar::show(%p)\n", toolbar);

  toolbar->p->tool_changed_handler = 
    g_signal_connect_object (user_context, "tool-changed",
                             G_CALLBACK (gimp_tool_options_toolbar_tool_changed),
                             toolbar,
                             0);

  gimp_tool_options_toolbar_tool_changed (user_context,
                                          gimp_context_get_tool (user_context),
                                          toolbar);
}