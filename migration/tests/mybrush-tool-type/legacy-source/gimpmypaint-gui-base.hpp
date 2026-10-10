/*
 * gimpmypaint-gui-base.hpp
 *
 * Copyright (C) 2012 - seagetch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GIMP_MYPAINT_GUI_BASE_HPP__
#define __GIMP_MYPAINT_GUI_BASE_HPP__

///////////////////////////////////////////////////////////////////////////////
class MypaintGUIPrivateBase {
protected:
  GimpMypaintOptions* options;

  class MypaintOptionsPropertyGUIPrivate {
    typedef MypaintOptionsPropertyGUIPrivate Class;
    GimpMypaintOptions* options;
    MyPaintBrushSettings* setting;
    
    GtkWidget* widget;
    gchar*   property_name;
    gchar*   internal_name;
    Delegators::Connection* notify_handler;
    Delegators::Connection* value_changed_handler;

  public:
    MypaintOptionsPropertyGUIPrivate(GimpMypaintOptions* opts,
                                     GHashTable* dict,
                                     const gchar* name) 
    {
      CString name_replaced(mypaint_brush_signal_name_to_internal_name(name));

      setting = reinterpret_cast<MyPaintBrushSettings*>(g_hash_table_lookup(dict, name_replaced.ptr()));
      if (!setting) {
        g_print("property %s is not found in the lookup dictionary.\n", name_replaced.ptr());
      }
      internal_name = (gchar*)g_strdup(name);
      property_name = (gchar*)g_strdup_printf("notify::%s", name);
      options = opts;
      g_object_add_weak_pointer(G_OBJECT(options), (void**)&options);
      notify_handler = value_changed_handler = NULL;
    }
      
    ~MypaintOptionsPropertyGUIPrivate() {
      if (property_name)
        g_free(property_name);

      if (internal_name)
        g_free(internal_name);
      
/*
      if (widget && value_changed_closure) {
	  gulong handler_id = g_signal_handler_find(gpointer(widget), G_SIGNAL_MATCH_CLOSURE, 0, 0, value_changed_closure, NULL, NULL);
	  g_signal_handler_disconnect(gpointer(widget), handler_id);
	  value_changed_closure = NULL;
      }

      if (options && notify_closure) {
        gulong handler_id = g_signal_handler_find(gpointer(optios), G_SIGNAL_MATCH_CLOSURE, 0, 0, notify_closure, NULL, NULL);
	g_signal_handler_disconnect(gpointer(options), handler_id);
	value_changed_closure = NULL;
      }*/
    }
      
    void notify(GObject* object) {
      gdouble value;
      g_object_get(G_OBJECT(options), internal_name, &value, NULL);
    }

    void value_changed(GObject* object) {
      gdouble value = gtk_spin_button_get_value(GTK_SPIN_BUTTON(object));
      g_object_set(G_OBJECT(options), property_name, value, NULL);
    }
      
    GtkWidget* create() {
      gdouble range = setting->maximum - setting->minimum;
      widget  = gimp_prop_spin_scale_new (G_OBJECT(options), 
                                          internal_name,
                                          _(setting->displayed_name),
                                          range / 1000.0, range / 100.0, 2);
      g_object_add_weak_pointer(G_OBJECT(widget), (void**)&widget);
      gimp_spin_scale_set_scale_limits (GIMP_SPIN_SCALE (widget), 
                                        setting->minimum, 
                                        setting->maximum);

/*      value_changed_closure = 
        g_signal_connect_delegator(G_OBJECT(widget),
				   "value-changed",
				   Delegator::delegator(this, &Class::value_changed));
      notify_closure = 
        g_signal_connect_delegator(G_OBJECT(options),
				   property_name,
				   Delegator::delegator(this, &Class::notify));*/
      g_object_set_cxx_object (G_OBJECT(widget), "behavior", this);
//      gtk_widget_set_size_request (widget, 200, -1);
      return widget;
    }
  };

public:
  MypaintGUIPrivateBase(GimpToolOptions* opts)
  {
    options = GIMP_MYPAINT_OPTIONS(opts);
  }

  virtual ~MypaintGUIPrivateBase() {}
  virtual GtkWidget* create() = 0;
};

#endif //__GIMP_MYPAINT_GUI_BASE_HPP__
