/* GIMP-painter
 * Copyright (C) 2022 seagetch
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

#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-def-utils.hpp"

extern "C" {
#include "config.h"

#include <glib-object.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "tools-types.h"

#include "base/temp-buf.h"

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimptooloptions.h"
#include "core/gimpdatafactory.h"
#include "core/gimpmypaintbrush.h"

#include "widgets/gimpview.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpcontainereditor.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimppopupbutton.h"
#include "widgets/gimpcontainerbox.h"

#include "gimptooloptions-gui.h"
#include "gimpbrushoptions-gui.h"

#include "gimp-intl.h"

};
#include "base/delegators.hpp"
#include "base/glib-cxx-utils.hpp"
#include "gimptooloptions-gui-cxx.hpp"
#include "core/gimpmypaintbrush-private.hpp"
#include "paint/gimpmypaintoptions-history.hpp"

using namespace GLib;

class MypaintPopupPrivate {
  static const int MYPAINT_BRUSH_VIEW_SIZE = 256;
  static const int HISTORY_PREVIEW_SIZE = 24;

  GimpContainer*         container;
  GimpContext*           context;
  GtkListStore*          store;
  PageRemindAction       page_reminder;
  Delegators::Connection* brush_changed_handler;
  Delegators::Connection* history_name_edited_handler;
  
public:
  MypaintPopupPrivate(GimpContainer* ctn, GimpContext* ctx) : 
    page_reminder(NULL) {
    container = ctn;
    context   = ctx;
    brush_changed_handler = NULL;
  }
  
  ~MypaintPopupPrivate();
  void create(GObject  *button,
              GtkWidget **result);
  void destroy(GObject* object);
  void update_brush (GObject *adjustment);
  void update_history ();
  void notify_brush (GObject *brush, GParamSpec *pspec);
  void brush_changed (GObject *object, GimpData *brush_data);
  void history_cursor_changed (GObject *object);
  void history_name_edited (GObject* renderer, gchar* path, gchar* new_text);

};


void
MypaintPopupPrivate::update_brush (GObject* object)
{
  GtkAdjustment *adjustment = GTK_ADJUSTMENT(object);
  gdouble              d_value_brush;
  gdouble              d_value_adj;
  gint                 i_value_brush;
  gint                 i_value_adj;
  GimpMypaintBrush    *brush;
  gchar               *prop_name;
  GtkAdjustment       *adj;

  g_return_if_fail (G_IS_OBJECT (adj));
  g_return_if_fail (G_IS_OBJECT (context));

  brush = gimp_context_get_mypaint_brush (context);

}

void
MypaintPopupPrivate::notify_brush (GObject* object,
                                   GParamSpec         *pspec)
{
  GimpMypaintBrush *brush = GIMP_MYPAINT_BRUSH (object);
  GtkAdjustment *adj     = NULL;
  gdouble        d_value = 0.0;
  gint           i_value = 0;
  
  g_print ("notify_brush: %s\n", pspec->name);

  if (! strcmp (pspec->name, "shape"))
    {
    }
  else
    {
    }
}

void
MypaintPopupPrivate::update_history ()
{
  GimpMypaintOptionsHistory* history = GimpMypaintOptionsHistory::get_singleton();
  int history_size = history->get_brush_history_size();
  gtk_list_store_clear(store);
  for (int i = 0; i < history_size; i ++) {
    GtkTreeIter iter;
    GimpMypaintBrush* brush = history->get_brush(i);
    if (brush) {
      auto brush_obj = ref(brush);
      CString name = g_strdup(brush_obj.get("name"));
      g_print("%d: brush %s\n", i, name.ptr());
      GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(brush->p);
      gtk_list_store_append(store, &iter);
      if (context) {
        GdkPixbuf* pixbuf = gimp_viewable_get_new_pixbuf(GIMP_VIEWABLE(brush), 
                                                         context, 
                                                         HISTORY_PREVIEW_SIZE, 
                                                         HISTORY_PREVIEW_SIZE);
        gtk_list_store_set(store, &iter, 0, pixbuf, -1);
      }
      gtk_list_store_set(store, &iter, 1, name.ptr(), 2, brush, -1);
    }
  }
}

void
MypaintPopupPrivate::brush_changed (GObject*  object,
                                    GimpData* brush_data)
{
  GimpMypaintBrush        *brush        = GIMP_MYPAINT_BRUSH (brush_data);
  gdouble                  radius       = 0.0;
  gdouble                  ratio        = 0.0;
  gdouble                  angle        = 0.0;
  gboolean                 editable     = false;

  update_history();
  g_print("brush changed\n");
}

void
MypaintPopupPrivate::history_cursor_changed (GObject*  object)
{
  GtkTreeSelection* selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(object));
  GtkTreeModel* model;
  GtkTreeIter   iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
    GimpMypaintBrush* brush = NULL;
    gtk_tree_model_get(model, &iter, 2, &brush, -1);
    if (brush)
      gimp_context_set_mypaint_brush(context, brush);
  }
}

void
MypaintPopupPrivate::history_name_edited (GObject* renderer, 
                                          gchar* path, gchar* new_text)
{
}


MypaintPopupPrivate::~MypaintPopupPrivate ()
{
}

void
MypaintPopupPrivate::destroy (GObject* object)
{
  if (container) {
    g_object_unref (G_OBJECT (container));
    container = NULL;
  }

  if (context) {
    if (brush_changed_handler) {
      delete brush_changed_handler;
      brush_changed_handler = NULL;
    }
  }
}

void
MypaintPopupPrivate::create (GObject* object,
                             GtkWidget **result)
{
  GtkButton*                     button = GTK_BUTTON(object);
  GimpContainerEditor           *editor;
  GimpMypaintBrush              *brush;
  GtkWidget                     *vbox;
  GtkWidget                     *frame;
  GtkWidget                     *box;
  GtkWidget                     *table;
  GtkWidget                     *frame2;
  GtkWidget                     *history;
  GimpViewType                   view_type = GIMP_VIEW_TYPE_GRID;
  GimpViewSize                   view_size = GIMP_VIEW_SIZE_LARGE;
  gint                           view_border_width = 1;
  gint                           default_view_size = GIMP_VIEW_SIZE_LARGE;
  GimpToolOptionsTableIncrement  inc = gimp_tool_options_table_increment (FALSE);
  GList                         *children;
  GtkAdjustment                 *adj = NULL;
  
  container = gimp_data_factory_get_container (context->gimp->mypaint_brush_factory);
  brush     = gimp_context_get_mypaint_brush (context);

  g_object_ref(G_OBJECT(container));
  
  g_return_if_fail (GIMP_IS_CONTAINER (container));
  g_return_if_fail (GIMP_IS_CONTEXT (context));
  g_return_if_fail (view_size >  0 &&
                    view_size <= GIMP_VIEWABLE_MAX_BUTTON_SIZE);
  g_return_if_fail (view_border_width >= 0 &&
                    view_border_width <= GIMP_VIEW_MAX_BORDER_WIDTH);

  *result    = gtk_notebook_new ();
  gtk_widget_show (*result);
  
  editor = GIMP_CONTAINER_EDITOR (
    g_object_new (GIMP_TYPE_CONTAINER_EDITOR,
      "view-type", view_type,
      "container", container,
      "context",   context,
      "view-size", view_size,
      "view-border-width", view_border_width,
      NULL));
  gimp_container_view_set_reorderable (GIMP_CONTAINER_VIEW (editor->view),
                                       FALSE);

  gimp_container_box_set_size_request (GIMP_CONTAINER_BOX (editor->view),
                                       6  * (default_view_size +
                                             2 * view_border_width),
                                       10 * (default_view_size +
                                             2 * view_border_width));

  gtk_notebook_insert_page (GTK_NOTEBOOK (*result), GTK_WIDGET (editor), 
                            gtk_label_new(_("Preset")),0);
  gtk_widget_show (GTK_WIDGET (editor));

  // Custom brush history view.
  GtkTreeViewColumn* column;
  store   = gtk_list_store_new(3, GDK_TYPE_PIXBUF, G_TYPE_STRING, GIMP_TYPE_MYPAINT_BRUSH);
  history = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  
  GtkCellRenderer* renderer;
  renderer = gtk_cell_renderer_pixbuf_new();
  column = gtk_tree_view_column_new_with_attributes("Icon", renderer, "pixbuf", 0, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(history), column);

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 1, NULL);
  g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(history), column);

  g_signal_connect_delegator(G_OBJECT(renderer), "edited",
                             Delegators::delegator(this, &MypaintPopupPrivate::history_name_edited));

  g_signal_connect_delegator(G_OBJECT(history), "cursor-changed",
                             Delegators::delegator(this, &MypaintPopupPrivate::history_cursor_changed));

  gtk_widget_show (GTK_WIDGET (history));
  gtk_notebook_insert_page (GTK_NOTEBOOK (*result), GTK_WIDGET (history), 
                            gtk_label_new(_("Custom")),1);


  page_reminder.bind_to(GTK_NOTEBOOK (*result));

  brush_changed_handler = 
    g_signal_connect_delegator (G_OBJECT(context),
                                gimp_context_type_to_signal_name (GIMP_TYPE_MYPAINT_BRUSH),
                                Delegators::delegator(this, &MypaintPopupPrivate::brush_changed));

/*
//  g_signal_connect (brush, "notify", G_CALLBACK (notify_brush), p);
*/
  if (context && brush)
    brush_changed (G_OBJECT(context), GIMP_DATA(brush));
  children = gtk_container_get_children (GTK_CONTAINER (table));  
  gimp_tool_options_setup_popup_layout (children, FALSE);
  
  g_signal_connect_delegator (G_OBJECT (*result), "destroy", 
                              Delegators::delegator(this, &MypaintPopupPrivate::destroy));
}

extern "C" {
/*  public functions  */
GtkWidget*
gimp_mypaint_brush_button_with_popup (GObject *config)
{
  GimpContainer *container;
  GimpContext   *context;
  GimpViewSize   view_size = GIMP_VIEW_SIZE_SMALL;
  const gchar        *prop_name;
  GtkWidget     *label_widget;

  context   = GIMP_CONTEXT (config);
  container = gimp_data_factory_get_container (context->gimp->mypaint_brush_factory);
  
  g_return_val_if_fail (GIMP_IS_CONTAINER (container), NULL);
  g_return_val_if_fail (GIMP_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (view_size >  0 &&
                        view_size <= GIMP_VIEWABLE_MAX_BUTTON_SIZE, NULL);

  prop_name = gimp_context_type_to_prop_name (gimp_container_get_children_type (container));

  label_widget = gimp_prop_view_new (G_OBJECT (context), prop_name,
                                     context, view_size);
  gtk_widget_show (label_widget);
  
  MypaintPopupPrivate* priv = new MypaintPopupPrivate(container, context);
  
  return gimp_tool_options_button_with_popup (label_widget,
                                               Delegators::delegator(priv, &MypaintPopupPrivate::create),
                                               priv);
}
};
