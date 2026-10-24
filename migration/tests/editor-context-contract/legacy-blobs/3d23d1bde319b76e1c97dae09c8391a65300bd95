// gimpmypaintbrusheditor.cpp
//
// Copyright (C) 2012 - seagetch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-utils.hpp"
#include "base/glib-cxx-def-utils.hpp"

extern "C" {
#include "config.h"

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"

#include "tools-types.h"

#include "base/temp-buf.h"

#include "core/gimpbrush.h"
#include "core/gimpmypaintbrush.h"
#include "core/gimptoolinfo.h"
#include "core/mypaintbrush-brushsettings.h"
#include "core/gimpmypaintbrush-private.hpp"
#include "core/gimpcurve.h"
  // for BRUSH VIEW
#include "core/gimp.h"
#include "core/gimpdatafactory.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "widgets/gimpview.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimpcontainereditor.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimppopupbutton.h"
#include "widgets/gimpcontainerbox.h"


#include "paint/gimpmypaintoptions.h"

#include "widgets/gimppropwidgets.h"
#include "widgets/gimpspinscale.h"
#include "widgets/gimpviewablebox.h"
#include "widgets/gimpwidgets-constructors.h"
#include "widgets/gimppopupbutton.h"
#include "widgets/gimpcurveview.h"

#include "widgets/gimpwidgets-utils.h"

#include "gimpairbrushtool.h"
#include "gimpclonetool.h"
#include "gimpconvolvetool.h"
#include "gimpdodgeburntool.h"
#include "gimperasertool.h"
#include "gimphealtool.h"
#include "gimpinktool.h"
#include "gimpmypaintoptions-gui.h"
#include "gimppenciltool.h"
#include "gimpperspectiveclonetool.h"
#include "gimpsmudgetool.h"
#include "gimptooloptions-gui.h"
#include "gimpmypaintbrushoptions-gui.h"
}; // extern "C"

#include "gimp-intl.h"

#include "base/delegators.hpp"
#include "base/scopeguard.hpp"
#include "base/glib-cxx-utils.hpp"
#include "gimptooloptions-gui-cxx.hpp"
#include "paint/gimpmypaintoptions-history.hpp"
#include "gimpmypaint-gui-base.hpp"
#include "gimpmypaintbrusheditor.hpp"


///////////////////////////////////////////////////////////////////////////////
class CurveViewActions {
  GObject*                   receiver;
  GimpMypaintOptions*        options;
  MyPaintBrushSettings*      brush_setting;
  MyPaintBrushInputSettings* input_setting;
  GtkTreeModel*              model;
  GtkAdjustment*             x_min_adj;
  GtkAdjustment*             x_max_adj;
  GtkAdjustment*             y_max_adj;
  GtkAdjustment*             y_min_adj;
  CXXPointer<Delegators::Connection>     options_notify_handler;
  CXXPointer<Delegators::Connection>     curve_notify_handler;
  CXXPointer<Delegators::Connection>     x_min_adj_changed_handler;
  CXXPointer<Delegators::Connection>     x_max_adj_changed_handler;
  CXXPointer<Delegators::Connection>     y_max_adj_changed_handler;
  CXXPointer<Delegators::Connection>     y_min_adj_changed_handler;
  static const int         MAXIMUM_NUM_POINTS = 8;

public:

  CurveViewActions(GObject* tree_view, 
                   GObject* receiver_, 
                   GimpMypaintOptions* opts, 
                   gchar* setting_name_, 
                   GObject* toggle, 
                   GtkAdjustment* x_min_adj_,
                   GtkAdjustment* x_max_adj_,
                   GtkAdjustment* y_min_adj_,
                   GtkAdjustment* y_max_adj_) 
  {
    IHashTable<gchar*, MyPaintBrushSettings*> brush_settings_dict = mypaint_brush_get_brush_settings_dict();
    receiver      = receiver_;
    options       = opts;
//      setting_index = setting_index_;
    brush_setting = brush_settings_dict[setting_name_];
    model         = GTK_TREE_MODEL( gtk_tree_view_get_model( GTK_TREE_VIEW(tree_view) ) );
    x_min_adj     = x_min_adj_;
    x_max_adj     = x_max_adj_;
    y_min_adj     = y_min_adj_; 
    y_max_adj     = y_max_adj_; 
    GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW (tree_view));
    input_setting = NULL;

    g_object_set_cxx_object(G_OBJECT(tree_view), "behavior", this);

    g_signal_connect_delegator (G_OBJECT(tree_sel), "changed", Delegators::delegator(this, &CurveViewActions::selected));
    g_signal_connect_delegator (G_OBJECT(toggle), "toggled", Delegators::delegator(this, &CurveViewActions::toggled));

    options_notify_handler      = g_signal_connect_delegator (G_OBJECT(options), "notify", Delegators::delegator(this, &CurveViewActions::notify_options));
    x_min_adj_changed_handler   = g_signal_connect_delegator (G_OBJECT(x_min_adj), "value-changed", Delegators::delegator(this, &CurveViewActions::value_changed));
    x_max_adj_changed_handler   = g_signal_connect_delegator (G_OBJECT(x_max_adj), "value-changed", Delegators::delegator(this, &CurveViewActions::value_changed));
    y_min_adj_changed_handler   = g_signal_connect_delegator (G_OBJECT(y_min_adj), "value-changed", Delegators::delegator(this, &CurveViewActions::value_changed));
    y_max_adj_changed_handler   = g_signal_connect_delegator (G_OBJECT(y_max_adj), "value-changed", Delegators::delegator(this, &CurveViewActions::value_changed));
    curve_notify_handler        = NULL;
  };
  
  ~CurveViewActions() {
  }
  
  void get_mapping_range(Mapping* mapping, int index, float* xmin, float* xmax, float* ymin, float* ymax) {
    int n_points = mapping->get_n(index);
    if (n_points > 0) {
      float x, y;

      mapping->get_point(index, 0, &x, ymin);
      *ymax = *ymin;
      *xmin = MIN(*xmin, x);
      *xmax = MAX(*xmax, x);
      for (int i = 1; i < n_points; i ++) {
        mapping->get_point(index, i, &x, &y);
        if (x == -FLT_MAX || y == -FLT_MAX || x == FLT_MAX || y == FLT_MAX || y == FLT_MAX)
          continue;
        if (*xmin > x)
          *xmin = x;
        if (*xmax < x)
          *xmax = x;
        if (*ymin > y)
          *ymin = y;
        if (*ymax < y)
          *ymax = y;
      }
    } else {
      *xmin = *xmax = 0;
      *ymin = *ymax = 0;
    }
    
  }

  void selected(GObject* event_target) {
    GtkTreeModel *model;
    GtkTreeIter   iter;

    if (gtk_tree_selection_get_selected (GTK_TREE_SELECTION(event_target), &model, &iter)) {
      gchar* name = NULL;

      gtk_tree_model_get (model, &iter,
                          2, &name,
                          -1);

      activate_input (name);
      if (name)
        g_free(name);
    }
  };
  
  Mapping* get_mapping() {
    GimpMypaintBrush* myb = gimp_mypaint_options_get_current_brush(options);
    GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(myb->p);
    Mapping* mapping = priv->get_setting(brush_setting->index)->mapping;
    return mapping;
  }
  
  MyPaintBrushInputSettings* get_input_setting(gchar* input_name) {
    IHashTable<gchar*, MyPaintBrushInputSettings*> input_settings_dict(mypaint_brush_get_input_settings_dict());
    return input_settings_dict[input_name];
  }
  
  void activate_input(gchar* name) {
    input_setting = get_input_setting(name);
    
    g_return_if_fail (input_setting != NULL);
    
    GimpCurveView* curve_view = GIMP_CURVE_VIEW(receiver);

    GimpCurve* old_curve = GIMP_CURVE(gimp_curve_view_get_curve(curve_view));

    curve_notify_handler = NULL;

    gimp_curve_view_set_curve (curve_view, NULL, NULL);
    gimp_curve_view_remove_all_backgrounds (curve_view);
    
    GimpCurve* curve = GIMP_CURVE(gimp_curve_new(input_setting->displayed_name));
    gimp_curve_set_curve_type(curve, GIMP_CURVE_SMOOTH);
    g_object_set(G_OBJECT(curve), "n-points", MAXIMUM_NUM_POINTS, NULL); // "8" is the maximum number of control points of mypaint dynamics...
    Mapping* mapping = get_mapping();
    float xmin = input_setting->soft_maximum;
    float ymin = 0.0;
    float xmax = input_setting->soft_minimum;
    float ymax = 0;
    
    if (mapping)
      get_mapping_range(mapping, input_setting->index, &xmin, &xmax, &ymin, &ymax);

    bool is_used = mapping && (ymin != ymax || ymax != 0.0);
    
    if (input_setting->hard_minimum == input_setting->hard_maximum) {
      gtk_adjustment_set_lower(x_min_adj, -20);
      gtk_adjustment_set_upper(x_min_adj,  20 - 0.1);
      gtk_adjustment_set_lower(x_max_adj, -20 + 0.1);
      gtk_adjustment_set_upper(x_max_adj,  20);
      
    } else {
      gtk_adjustment_set_lower(x_min_adj, input_setting->hard_minimum);
      gtk_adjustment_set_upper(x_min_adj, input_setting->hard_maximum - 0.1);
      gtk_adjustment_set_lower(x_max_adj, input_setting->hard_minimum + 0.1);
      gtk_adjustment_set_upper(x_max_adj, input_setting->hard_maximum);
    }

    float y_minimum = MIN(-brush_setting->maximum, brush_setting->minimum);

    if (brush_setting->maximum > -FLT_MAX && brush_setting->maximum < FLT_MAX) {
      gtk_adjustment_set_upper(y_max_adj,  brush_setting->maximum);
      gtk_adjustment_set_upper(y_min_adj,  brush_setting->maximum);
    }
    if (y_minimum > -FLT_MAX && y_minimum < FLT_MAX) {
      gtk_adjustment_set_lower(y_max_adj,  y_minimum);
      gtk_adjustment_set_lower(y_min_adj,  y_minimum);
    }

    if (is_used) {
      mapping_to_curve(curve);
    }

    GimpRGB color = {1.0, 0., 0.};
    gimp_curve_view_set_curve (curve_view,
                                 curve, &color);
    curve_notify_handler = g_signal_connect_delegator(G_OBJECT(curve), "notify::points",
                                                      Delegators::delegator(this, &CurveViewActions::notify_curve));
    
    update_range_parameters();        

    gimp_curve_view_set_x_axis_label (curve_view,
                                      input_setting->displayed_name);
  }
  
  
  void mapping_to_curve(GimpCurve* curve) {
    g_return_if_fail( input_setting != NULL );

    float xmin = input_setting->soft_maximum;
    float ymin = 0.0;
    float xmax = input_setting->soft_minimum;
    float ymax = 0;

    Mapping* mapping = get_mapping();
    if (mapping) {
      int mapping_n_points = mapping->get_n(input_setting->index);
      int curve_n_points;

      g_object_get(G_OBJECT(curve), "n-points", &curve_n_points, NULL);

      get_mapping_range(mapping, input_setting->index, &xmin, &xmax, &ymin, &ymax);
      
      if (ymin != ymax || ymax != 0.0) {
        
        float xrange = xmax - xmin;
        float yrange = ymax - ymin;
        float x, y;
        int i = 0;

        for (; i < MAXIMUM_NUM_POINTS; i ++) {
          gimp_curve_set_point(curve, i, -1.0, -1.0);
        }

        for (i = 0; i < mapping_n_points; i ++) {
          mapping->get_point(input_setting->index, i, &x, &y);
          float normalized_x = xrange != 0.0? (x - xmin) / xrange : 0.;
          float normalized_y = yrange != 0.0? (y - ymin) / yrange : 0.;
          int closest_index = (int)(normalized_x * curve_n_points);
          closest_index = CLAMP(closest_index, 0, curve_n_points - 1);
          double tmp_x, tmp_y;
          gimp_curve_get_point(curve, closest_index, &tmp_x, &tmp_y);
          
          if (tmp_x < 0)
            gimp_curve_set_point(curve, i, normalized_x, normalized_y);
          else
            ;;
        }
      }
      
    }
  }
  
  
  void update_range_parameters() {
    g_return_if_fail( input_setting != NULL );
    GimpCurveView* curve_view = GIMP_CURVE_VIEW(receiver);
    g_return_if_fail( curve_view != NULL );

    float xmin = input_setting->soft_maximum;
    float ymin = 0;
    float xmax = input_setting->soft_minimum;
    float ymax;
    Mapping* mapping = get_mapping();
    get_mapping_range(mapping, input_setting->index, &xmin, &xmax, &ymin, &ymax);
    g_print("xmin,xmax=%3.2f,%3.2f: ymin,ymax=%3.2f, %3.2f in [%f,%f]\n", xmin, xmax, ymin, ymax,brush_setting->minimum, brush_setting->maximum);

    gdouble xmin_value = xmin;
    gdouble xmax_value = xmax;
    if (xmin_value == xmax_value) {
      xmin_value = input_setting->soft_minimum;
      xmax_value = input_setting->soft_maximum;
      if (xmin_value == xmax_value) {
        xmin_value = -20;
        xmax_value = 20;
      }
    }
//    gdouble ymax_value = MAX(ABS(ymax), ABS(ymin));
    
    
    gtk_adjustment_set_value(x_min_adj, xmin_value);
    gtk_adjustment_set_value(x_max_adj, xmax_value);
    gtk_adjustment_set_value(y_min_adj, ymin);
    gtk_adjustment_set_value(y_max_adj, ymax);
    
    gimp_curve_view_set_range_x(curve_view,  xmin_value, xmax_value);
    gimp_curve_view_set_range_y(curve_view, ymin, ymax);
  }
  
  
  void update_list() {
    IHashTable<gchar*, MyPaintBrushInputSettings*> input_settings_dict = mypaint_brush_get_input_settings_dict();
    GtkTreeIter iter;
    g_return_if_fail (gtk_tree_model_get_iter_first(model, &iter));
    do {
      gchar* name = NULL;

      gtk_tree_model_get (model, &iter, 2, &name, -1);

      MyPaintBrushInputSettings* s = input_settings_dict[name];
      Mapping* mapping = get_mapping();
      float xmin = s->soft_maximum;
      float ymin = 0.0;
      float xmax = s->soft_minimum;
      float ymax = 0.0;
      
      if (mapping)
        get_mapping_range(mapping, s->index, &xmin, &xmax, &ymin, &ymax);
      
      bool is_used = mapping && (ymin != ymax || ymax != 0.0);
      gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                          0, is_used,
                          -1);
    } while (gtk_tree_model_iter_next(model, &iter));        
  }

  
  void toggled(GObject* event_target, gchar* path) {
    GtkTreeIter                      iter;

    if (gtk_tree_model_get_iter_from_string (model, &iter, path)) {
      gchar*   name;
      gboolean use;

      gtk_tree_model_get (model, &iter,
                          2, &name,
                          0, &use,
                          -1);
      g_print("%s is now %d\n", name, use);

      Mapping* mapping = get_mapping();
      input_setting = get_input_setting(name);
      
      if (mapping && input_setting) {
        float xmin = input_setting->soft_maximum;
        float ymin = 0.0;
        float xmax = input_setting->soft_minimum;
        float ymax = 0.0;
        
        if (mapping)
          get_mapping_range(mapping, input_setting->index, &xmin, &xmax, &ymin, &ymax);
        
        bool is_used = mapping && (ymin != ymax || ymax != 0.0);
        CString signal_name = mypaint_brush_internal_name_to_signal_name(brush_setting->internal_name);
        if (!is_used) {
          GimpVector2 points[] = {
            {input_setting->hard_minimum, 0},
            {input_setting->hard_maximum, input_setting->normal }
          };
          gimp_mypaint_options_set_mapping_point(options, signal_name, input_setting->index, 2, points);
        } else {
          gimp_mypaint_options_set_mapping_point(options, signal_name, input_setting->index, 0, NULL);
        }
        gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                            0, !is_used,
                            -1);
        activate_input(name);
      }
    }
  }
  
  
  void notify_curve(GObject* target, GParamSpec *pspec) {
    GimpCurve* curve = GIMP_CURVE(target);
//      g_print("notify_curve (%lx)->(%lx)\n", (gulong)target, (gulong)receiver);
//      g_print("block notify for %lx\n", (gulong)options);

    options_notify_handler->block();
    curve_to_mapping(curve);

//      g_print("unblock notify for %lx\n", (gulong)options);
    options_notify_handler->unblock();

//      g_print("/notify_curve\n");
  }
  
  void curve_to_mapping(GimpCurve* curve) {
    Mapping* mapping = get_mapping();
    GimpCurveView* curve_view = GIMP_CURVE_VIEW(receiver);
    
    g_return_if_fail(input_setting && mapping && curve);

    gint n_points;
    g_object_get(G_OBJECT(curve), "n-points", &n_points, NULL);
    
    int count = 0;
    GimpVector2 points[n_points], tmp;

    for (int i = 0; i < n_points; i ++) {
      gimp_curve_get_point(curve, i, &(points[count].x), &(points[count].y));
      if (points[count].x == -1.0 && points[count].y == -1.0)
        continue;
        
      // insert by bubble sort method.
      for (int j = count; j > 0; j --) {
        if (points[j].x < points[j-1].x) {
          tmp = points[j];
          points[j]   = points[j-1];
          points[j-1] = tmp;
        }
      };
      count ++;
    }
    
    gdouble xmin, xmax, ymin, ymax, xrange, yrange;
    gimp_curve_view_get_range_x(curve_view, &xmin, &xmax);
    gimp_curve_view_get_range_y(curve_view, &ymin, &ymax);
    
    xrange = xmax - xmin;
    yrange = ymax - ymin;
    
    float ymin_value = ymax, ymax_value = ymin;

    for (int i = 0; i < count; i ++) {
      points[i].x = points[i].x * xrange + xmin;
      points[i].y = points[i].y * yrange + ymin;
      ymin_value = MIN(points[i].y, ymin_value);
      ymax_value = MAX(points[i].y, ymax_value);
      g_print("curve_to_mapping: %d: %f, %f\n", i, points[i].x, points[i].y);
    }
    
    if (ymin_value == ymax_value) {
      count = 0;
    }
    
    if (count > 1 || count == 0) {
      CString signal_name = mypaint_brush_internal_name_to_signal_name(brush_setting->internal_name);
      gimp_mypaint_options_set_mapping_point(options, signal_name, input_setting->index, count, points);
    }
  }
  
  
  void notify_options(GObject* config, GParamSpec *pspec) {
    GimpCurveView* curve_view = GIMP_CURVE_VIEW(receiver);
    
    g_return_if_fail (curve_view);
    
    GimpCurve*     curve      = GIMP_CURVE(gimp_curve_view_get_curve(curve_view));
    
    if (curve_notify_handler) {
      curve_notify_handler->block();
    }
  
    update_list();


    if (input_setting && strcmp(input_setting->name, pspec->name) == 0)
      mapping_to_curve(curve);

    if (curve_notify_handler) {
      curve_notify_handler->unblock();
    }
  
  }
  
  
  void value_changed(GObject* target) {
    GimpCurveView* curve_view = GIMP_CURVE_VIEW(receiver);
    GimpCurve*     curve      = GIMP_CURVE(gimp_curve_view_get_curve(curve_view));

    gdouble xmin_value = gtk_adjustment_get_value(x_min_adj);
    gdouble xmax_value = gtk_adjustment_get_value(x_max_adj);
    gdouble ymin_value = gtk_adjustment_get_value(y_min_adj);
    gdouble ymax_value = gtk_adjustment_get_value(y_max_adj);
    
    gimp_curve_view_set_range_x(curve_view, xmin_value, xmax_value);
    gimp_curve_view_set_range_y(curve_view, ymin_value, ymax_value);

    curve_to_mapping(curve);
  };

}; // class CurveViewActions


//////////////////////////////////////////////////////////////////////////
class CurveViewCreator {
  gchar*              brush_setting_name;
  GimpMypaintOptions* options;
public:
  CurveViewCreator(GimpMypaintOptions* opts, const gchar* name) 
  {
    options = opts;
    brush_setting_name = g_strdup (name);
  };

  ~CurveViewCreator() {
    if (brush_setting_name) {
      g_free(brush_setting_name);
    }
  };
  void create_view(GObject* button, GtkWidget** result);
};


void 
CurveViewCreator::create_view(GObject* button, GtkWidget** result) 
{
  IHashTable<const gchar*, MyPaintBrushSettings*> brush_settings_dict = mypaint_brush_get_brush_settings_dict();
  List      inputs               (mypaint_brush_get_input_settings());
  
  MyPaintBrushSettings* setting = brush_settings_dict[brush_setting_name];
  
  GtkWidget* result_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);

  // List view  
  GtkWidget* list_view  = gtk_tree_view_new();
  gtk_box_pack_start(GTK_BOX(result_box), list_view, FALSE, TRUE, 0);
  gtk_widget_show(list_view);
  
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list_view), FALSE);

  {  
    auto list_store = ref(gtk_list_store_new(3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING));
    gtk_tree_view_set_model (GTK_TREE_VIEW(list_view), GTK_TREE_MODEL(list_store.ptr()));
  }
  
  GtkListStore* store = GTK_LIST_STORE( gtk_tree_view_get_model( GTK_TREE_VIEW(list_view) ) );
  gtk_list_store_clear (store);
  
  for (GList* iter = inputs; iter; iter = iter->next) {
    
    GtkTreeIter tree_iter;
    MyPaintBrushInputSettings* input = reinterpret_cast<MyPaintBrushInputSettings*>(iter->data);
    gtk_list_store_append(store, &tree_iter);
    
    GimpMypaintBrush* myb         = gimp_mypaint_options_get_current_brush(options);
    GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(myb->p);
    Mapping* mapping              = priv->get_setting(setting->index)->mapping;
    
    bool is_enabled = (mapping && mapping->get_n(input->index));
    gtk_list_store_set(store, &tree_iter, 0, is_enabled, 1, input->displayed_name, 2, input->name, -1);
  }

  GtkTreeViewColumn   *column;
  GtkCellRenderer *renderer;
  GtkCellRenderer *used;

  renderer = gtk_cell_renderer_toggle_new();
  g_object_set (G_OBJECT(renderer),
                "mode",        GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                "activatable", TRUE,
                NULL);
  column = gtk_tree_view_column_new_with_attributes(_("Input"), renderer,
                                                    "active",  0,
                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
  used = renderer;

  renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(_("Input"), renderer,
                                                    "text",  1,
                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);

  // GimpCurveView
  GtkWidget* table = gtk_table_new(4, 4, FALSE);
  gtk_widget_show(table);
  gtk_box_pack_start(GTK_BOX(result_box), table, TRUE, TRUE, 0);

  GtkWidget* curve_view = gimp_curve_view_new();
  gtk_widget_set_size_request(curve_view, 250, 250);
  g_object_set (curve_view, "border-width", 1, NULL);
  g_object_set (curve_view, "y-axis-label", "", NULL);
  gtk_widget_show(curve_view);

  gtk_table_attach(GTK_TABLE(table), curve_view, 0, 3, 0, 3, 
                   GtkAttachOptions(GTK_EXPAND | GTK_FILL), 
                   GtkAttachOptions(GTK_EXPAND | GTK_FILL), 0, 0);
 
  GtkAdjustment* x_min_adj    = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0, 0.1, 0.01, 0.1, 0));
  GtkAdjustment* x_max_adj    = GTK_ADJUSTMENT(gtk_adjustment_new(1.0, 0.9, 1.0, 0.01, 0.1, 0));
  GtkAdjustment* y_min_adj  = GTK_ADJUSTMENT(gtk_adjustment_new(0, -1.0, 1.0, 0.01, 0.1, 0)); 
  GtkAdjustment* y_max_adj  = GTK_ADJUSTMENT(gtk_adjustment_new(1.0/4.0, -1.0, 1.0, 0.01, 0.1, 0)); 
  GtkWidget*     x_min_edit   = gtk_spin_button_new(x_min_adj, 0.01, 2);
  GtkWidget*     x_max_edit   = gtk_spin_button_new(x_max_adj, 0.01, 2);
  GtkWidget*     y_min_edit   = gtk_spin_button_new(y_min_adj, 0.01, 2);
  GtkWidget*     y_max_edit   = gtk_spin_button_new(y_max_adj, 0.01, 2);

  gtk_widget_show(x_min_edit);
  gtk_widget_show(x_max_edit);
  gtk_widget_show(y_min_edit);
  gtk_widget_show(y_max_edit);

  gtk_table_attach(GTK_TABLE(table), x_min_edit, 0, 1, 3, 4, 
                   GtkAttachOptions(GTK_FILL), 
                   GtkAttachOptions(GTK_FILL), 0, 0);
  gtk_table_attach(GTK_TABLE(table), x_max_edit, 2, 3, 3, 4, 
                   GtkAttachOptions(GTK_FILL), 
                   GtkAttachOptions(GTK_FILL), 0, 0);
  gtk_table_attach(GTK_TABLE(table), y_min_edit, 3, 4, 2, 3, 
                   GtkAttachOptions(GTK_FILL), 
                   GtkAttachOptions(GTK_FILL), 0, 0);
  gtk_table_attach(GTK_TABLE(table), y_max_edit, 3, 4, 0, 1, 
                   GtkAttachOptions(GTK_FILL), 
                   GtkAttachOptions(GTK_FILL), 0, 0);
 
  
  // Event handler construction
  GtkTreeSelection* tree_sel = gtk_tree_view_get_selection(GTK_TREE_VIEW (list_view));
  gtk_tree_selection_set_mode(tree_sel, GTK_SELECTION_BROWSE);

  CurveViewActions* select_action = new CurveViewActions(G_OBJECT(list_view), 
                                                         G_OBJECT(curve_view), 
                                                         options, 
                                                         setting->internal_name, 
                                                         G_OBJECT(used),
                                                         x_min_adj,
                                                         x_max_adj,
                                                         y_min_adj,
                                                         y_max_adj);

  *result = result_box;
}; // class CurveViewCreator

////////////////////////////////////////////////////////////////////////////////
GtkWidget*
MypaintBrushEditorPrivate::create() {
  GtkWidget* brushsetting_vbox;
  
  if (type == EXPANDER) {
    brushsetting_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_show(brushsetting_vbox);
  } else {
    brushsetting_vbox = gtk_notebook_new();
    gtk_notebook_set_scrollable (GTK_NOTEBOOK(brushsetting_vbox), TRUE);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(brushsetting_vbox), GTK_POS_LEFT);
  }

  GList* setting_groups;
  setting_groups = mypaint_brush_get_setting_group_list();

  for (GList* iter = setting_groups; iter; iter = iter->next) {
    MypaintBrushSettingGroup* group =
      (MypaintBrushSettingGroup*)iter->data;

    CString bold_title(g_strdup_printf("<b>%s</b>", group->display_name));
    GtkWidget* group_expander;
    
    if (type == EXPANDER) {
      group_expander = gtk_expander_new(bold_title.ptr());
      gtk_expander_set_use_markup(GTK_EXPANDER(group_expander), TRUE);
      gtk_widget_show(group_expander);
      if (strcmp(group->name, "basic") == 0)
        gtk_expander_set_expanded (GTK_EXPANDER(group_expander), TRUE);

    }
    
    GtkWidget* table = gtk_table_new(3, 2 * g_list_length(group->setting_list), FALSE);
    gtk_widget_show(table);

    int count = 0;
    for (GList* iter2 = group->setting_list; iter2; iter2 = iter2->next, count += 2) {
      MyPaintBrushSettingEntry* entry = (MyPaintBrushSettingEntry*)iter2->data;
      switch (entry->type) {
        case G_TYPE_DOUBLE: {
          MyPaintBrushSettings* s = entry->f;
          CString prop_name(mypaint_brush_internal_name_to_signal_name(s->internal_name));
          g_print("%s : type=double\n", prop_name.ptr());

          GtkWidget* h;
          h = gimp_prop_spin_scale_new(G_OBJECT(options), prop_name.ptr(),
                                       s->displayed_name,
                                       0.1, 1.0, 2);
          gtk_widget_set_tooltip_text(h, s->tooltip);
          gtk_widget_show(h);

          GtkWidget* button = gimp_stock_button_new (GIMP_STOCK_RESET, NULL);
          gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
          gtk_image_set_from_stock (GTK_IMAGE (gtk_bin_get_child (GTK_BIN (button))),
                                    GIMP_STOCK_RESET, GTK_ICON_SIZE_MENU);
          gtk_widget_show (button);
          gtk_widget_set_tooltip_text(button, _("Reset to default value"));

          //g_signal_connect_...(button, "clicked,...);
          GtkWidget* button2;
          if (s->constant) {
            button2 = gtk_label_new("");
            gtk_widget_set_tooltip_text(button2, _("No additional configuration"));
            gtk_misc_set_alignment(GTK_MISC(button2), 0.5, 0.5);

          } else {
            //        button2 = gtk_button_new_with_label("...");
            CurveViewCreator* create_view = new CurveViewCreator(options, s->internal_name);
            button2 = 
              gimp_tool_options_button_with_popup(gtk_label_new("..."),
                                                  Delegators::delegator(create_view, &CurveViewCreator::create_view),
                                                  create_view);
            gtk_widget_set_tooltip_text(button2, _("Add input value mapping"));
            //g_signal_connect...(button2, "clicked", self.details_clicked_cb, adj, s)
          }

          gtk_widget_show(button2);
          gtk_table_attach(GTK_TABLE(table), h, 0, 1, count, count+1, 
                           GtkAttachOptions(GTK_EXPAND | GTK_FILL), 
                           GtkAttachOptions(GTK_FILL), 0, 0);

          gtk_table_attach(GTK_TABLE(table), button, 1, 2, count, count+1, 
                           GtkAttachOptions(GTK_FILL), 
                           GtkAttachOptions(0), 0, 0);

          gtk_table_attach(GTK_TABLE(table), button2, 2, 3, count, count+1,                         
                           GtkAttachOptions(GTK_FILL),                         
                           GtkAttachOptions(0), 0, 0);
          break;
        }  
        case G_TYPE_BOOLEAN: {
          MyPaintBrushSwitchSettings* s = entry->b;
          CString prop_name(mypaint_brush_internal_name_to_signal_name(s->internal_name));
          g_print("%s : type=boolean\n", prop_name.ptr());
          GtkWidget* h;
          h = gimp_prop_check_button_new (G_OBJECT(options), prop_name.ptr(),
                                          s->displayed_name);
          gtk_widget_show(h);
          gtk_table_attach(GTK_TABLE(table), h, 0, 1, count, count+1, 
                           GtkAttachOptions(GTK_EXPAND | GTK_FILL), 
                           GtkAttachOptions(0), 0, 0);
          break;
        }
      };
      
      //GtkWidget* input_editor = create_input_editor(s->internal_name);

      //new ToggleWidgetAction(G_OBJECT(button2), G_OBJECT(input_editor));
      //gtk_table_attach(GTK_TABLE(table), input_editor, 0, 3, count + 1, count + 2,
      //                 GtkAttachOptions(GTK_FILL),
      //                 GtkAttachOptions(GTK_FILL), 0, 0);
      
    }
    if (type == EXPANDER) {
      gtk_container_add(GTK_CONTAINER(group_expander), table);
      gtk_box_pack_start(GTK_BOX(brushsetting_vbox) ,group_expander,TRUE,TRUE,0);
    } else {
      GtkWidget* editor_container = gtk_scrolled_window_new (NULL, NULL);
      gtk_widget_set_size_request(editor_container, 400, 500);
      gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(editor_container), table);
      gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(editor_container), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
      gtk_widget_show(editor_container);
      gtk_notebook_append_page(GTK_NOTEBOOK(brushsetting_vbox), editor_container, gtk_label_new(group->display_name));
    }
  }

  if (type == TAB && page_reminder) {
    page_reminder->bind_to(GTK_NOTEBOOK(brushsetting_vbox));
  }
  
  g_object_set_cxx_object(G_OBJECT(brushsetting_vbox), "behavior-editor-private", this);
  return brushsetting_vbox;
}
