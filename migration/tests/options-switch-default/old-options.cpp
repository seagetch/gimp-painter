/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995-1999 Spencer Kimball and Peter Mattis
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

#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"

extern "C" {
#include "config.h"

#include <glib.h>
#include <gegl.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"

#include "paint-types.h"
#include "core/core-types.h"

#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"
#include "core/gimpbrush.h"
#include "core/gimppattern.h"
#include "core/gimpmypaintbrush.h"
#include "core/mypaintbrush-enum-settings.h"
#include "core/mypaintbrush-brushsettings.h"
#include "core/gimpdatafactory.h"
#include "core/gimpcontainer.h"
#include "gimpmypaintoptions.h"

#include "gimp-intl.h"

}; // extern "C"

extern "C++" {
#include "core/gimpmypaintbrush-private.hpp"
#include "base/scopeguard.hpp"
#include "gimpmypaintoptions-history.hpp"
} // extern "C++"

using namespace GLib;

extern "C" {
#define DEFAULT_BRUSH_SIZE             20.0
#define DEFAULT_BRUSH_ASPECT_RATIO     0.0
#define DEFAULT_BRUSH_ANGLE            0.0

#define DEFAULT_BRUSH_MODE             GIMP_MYPAINT_NORMAL

enum
{
  PROP_0 = BRUSH_SETTINGS_COUNT,

  PROP_BRUSH_MODE,

  PROP_BRUSH_VIEW_TYPE,
  PROP_BRUSH_VIEW_SIZE,
};


static void    gimp_mypaint_options_dispose          (GObject      *object);
static void    gimp_mypaint_options_finalize         (GObject      *object);
static void    gimp_mypaint_options_set_property     (GObject      *object,
                                                    guint         property_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);
static void    gimp_mypaint_options_get_property     (GObject      *object,
                                                    guint         property_id,
                                                    GValue       *value,
                                                    GParamSpec   *pspec);
static void    gimp_mypaint_options_mypaint_brush_changed (GObject *object,
                                                           GimpData *data);
static void    gimp_mypaint_options_brush_changed (GObject* object, GimpData* data);
static void    gimp_mypaint_options_prop_brushmark_updated (GObject* object);

static void    gimp_mypaint_options_texture_changed (GObject* object, GimpData* data);
static void    gimp_mypaint_options_prop_texture_updated (GObject* object);


G_DEFINE_TYPE (GimpMypaintOptions, gimp_mypaint_options, GIMP_TYPE_TOOL_OPTIONS);

#define parent_class gimp_mypaint_options_parent_class


static void
gimp_mypaint_options_class_init (GimpMypaintOptionsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose      = gimp_mypaint_options_dispose;
  object_class->finalize     = gimp_mypaint_options_finalize;
  object_class->set_property = gimp_mypaint_options_set_property;
  object_class->get_property = gimp_mypaint_options_get_property;

  List brush_settings(mypaint_brush_get_brush_settings ());
  for (GList* i = brush_settings.ptr(); i; i = i->next) {
    MyPaintBrushSettings* setting = reinterpret_cast<MyPaintBrushSettings*>(i->data);
    gchar* signal_name = mypaint_brush_internal_name_to_signal_name (setting->internal_name);
      
    GIMP_CONFIG_INSTALL_PROP_DOUBLE (object_class, setting->index + 1,
      signal_name, _(setting->displayed_name),
      setting->minimum, setting->maximum,
      setting->default_value, 
      (GParamFlags)(GIMP_PARAM_STATIC_STRINGS));
  }
  List switch_settings(mypaint_brush_get_brush_switch_settings ());
  for (GList* i = switch_settings.ptr(); i; i = i->next) {
    MyPaintBrushSwitchSettings* setting = reinterpret_cast<MyPaintBrushSwitchSettings*>(i->data);
    gchar* signal_name = mypaint_brush_internal_name_to_signal_name (setting->internal_name);
      
    GIMP_CONFIG_INSTALL_PROP_BOOLEAN (object_class, setting->index + 1,
      signal_name, _(setting->displayed_name),
      setting->default_value, 
      (GParamFlags)(GIMP_PARAM_STATIC_STRINGS));
  }
  List text_settings(mypaint_brush_get_brush_text_settings ());
  for (GList* i = text_settings.ptr(); i; i = i->next) {
    MyPaintBrushTextSettings* setting = reinterpret_cast<MyPaintBrushTextSettings*>(i->data);
    gchar* signal_name = mypaint_brush_internal_name_to_signal_name (setting->internal_name);
      
    GIMP_CONFIG_INSTALL_PROP_STRING (object_class, setting->index + 1,
      signal_name, _(setting->displayed_name),
      setting->default_value, 
      (GParamFlags)(GIMP_PARAM_STATIC_STRINGS));
  }

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_BRUSH_MODE,
                                 "brush-mode", _("Every stamp has its own opacity"),
                                 GIMP_TYPE_MYPAINT_BRUSH_MODE,
                                 DEFAULT_BRUSH_MODE,
                                 GParamFlags(GIMP_PARAM_STATIC_STRINGS));

  GIMP_CONFIG_INSTALL_PROP_ENUM (object_class, PROP_BRUSH_VIEW_TYPE,
                                 "brush-view-type", NULL,
                                 GIMP_TYPE_VIEW_TYPE,
                                 GIMP_VIEW_TYPE_GRID,
                                 GParamFlags(GIMP_PARAM_STATIC_STRINGS));

  GIMP_CONFIG_INSTALL_PROP_INT (object_class, PROP_BRUSH_VIEW_SIZE,
                                "brush-view-size", NULL,
                                GIMP_VIEW_SIZE_TINY,
                                GIMP_VIEWABLE_MAX_BUTTON_SIZE,
                                GIMP_VIEW_SIZE_SMALL,
                                GParamFlags(GIMP_PARAM_STATIC_STRINGS));

}

static void
gimp_mypaint_options_init (GimpMypaintOptions *options)
{
  options->brush_mode      = DEFAULT_BRUSH_MODE;
  options->brush           = NULL;
  g_signal_connect(G_OBJECT(options),  
    gimp_context_type_to_signal_name (GIMP_TYPE_MYPAINT_BRUSH),
    G_CALLBACK(gimp_mypaint_options_mypaint_brush_changed),
    NULL);
  g_signal_connect(G_OBJECT(options),
                   gimp_context_type_to_signal_name (GIMP_TYPE_BRUSH),
                   G_CALLBACK(gimp_mypaint_options_brush_changed), NULL);
  g_signal_connect(G_OBJECT(options),
                   gimp_context_type_to_signal_name (GIMP_TYPE_PATTERN),
                   G_CALLBACK(gimp_mypaint_options_texture_changed), NULL);
  {
    CString notify_name = g_strdup_printf("notify::%s", mypaint_brush_internal_name_to_signal_name ("brushmark_name"));
    g_signal_connect(G_OBJECT(options),
                     notify_name, G_CALLBACK(gimp_mypaint_options_prop_brushmark_updated), NULL);
  }
  {
    CString notify_name = g_strdup_printf("notify::%s", mypaint_brush_internal_name_to_signal_name ("use_gimp_brushmark"));
    g_signal_connect(G_OBJECT(options),
                     notify_name, G_CALLBACK(gimp_mypaint_options_prop_brushmark_updated), NULL);
  }
  {
    CString notify_name = g_strdup_printf("notify::%s", mypaint_brush_internal_name_to_signal_name ("brushmark_specified"));
    g_signal_connect(G_OBJECT(options),
                     notify_name, G_CALLBACK(gimp_mypaint_options_prop_brushmark_updated), NULL);
  }
  {
    CString notify_name = g_strdup_printf("notify::%s", mypaint_brush_internal_name_to_signal_name ("texture_name"));
    g_signal_connect(G_OBJECT(options),
                     notify_name, G_CALLBACK(gimp_mypaint_options_prop_texture_updated), NULL);
  }
  {
    CString notify_name = g_strdup_printf("notify::%s", mypaint_brush_internal_name_to_signal_name ("use_gimp_texture"));
    g_signal_connect(G_OBJECT(options),
                     notify_name, G_CALLBACK(gimp_mypaint_options_prop_texture_updated), NULL);
  }
  {
    CString notify_name = g_strdup_printf("notify::%s", mypaint_brush_internal_name_to_signal_name ("texture_specified"));
    g_signal_connect(G_OBJECT(options),
                     notify_name, G_CALLBACK(gimp_mypaint_options_prop_texture_updated), NULL);
  }
}

static void
gimp_mypaint_options_dispose (GObject *object)
{
  GimpMypaintOptions *options = GIMP_MYPAINT_OPTIONS (object);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gimp_mypaint_options_finalize (GObject *object)
{
  GimpMypaintOptions *options = GIMP_MYPAINT_OPTIONS (object);

  if (options->brush) {
    g_object_unref(options->brush);
    options->brush = NULL;
  }
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gimp_mypaint_options_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GimpMypaintOptions* options = GIMP_MYPAINT_OPTIONS (object);

  if (property_id <= PROP_0) {
    if (options->brush) {
      GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(options->brush->p);
      gchar* name = mypaint_brush_settings_index_to_internal_name (property_id - 1);
      GType type  = mypaint_brush_get_prop_type (name);

      switch (type) {
      case G_TYPE_DOUBLE:
        priv->set_base_value (property_id - 1, g_value_get_double (value));
        break;
      case G_TYPE_BOOLEAN:
        priv->set_bool_value (property_id - 1, g_value_get_boolean (value));
        break;
      case G_TYPE_STRING:
        priv->set_text_value (property_id - 1, g_value_get_string (value));
        break;
      }
    } else
      ; //G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);

  } else {
    switch (property_id) {
    case PROP_BRUSH_MODE:
      options->brush_mode = (GimpMypaintBrushMode)(g_value_get_enum (value));
      break;

    case PROP_BRUSH_VIEW_TYPE:
      options->brush_view_type = (GimpViewType)(g_value_get_enum (value));
      break;

    case PROP_BRUSH_VIEW_SIZE:
      options->brush_view_size = (GimpViewSize)(g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}

static void
gimp_mypaint_options_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GimpMypaintOptions     *options           = GIMP_MYPAINT_OPTIONS (object);

  if (property_id <= PROP_0) {
    if (options->brush) {
      GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(options->brush->p);
      gchar* name = mypaint_brush_settings_index_to_internal_name (property_id - 1);
      GType type  = mypaint_brush_get_prop_type (name);
      switch (type) {
      case G_TYPE_DOUBLE: {
        float fval = priv->get_base_value (property_id - 1);
        g_value_set_double (value, fval);
        break;
      }
      case G_TYPE_BOOLEAN: {
        bool bval = priv->get_bool_value (property_id - 1);
        g_value_set_boolean (value, bval);
        break;
      }
      case G_TYPE_STRING:
        char* text = priv->get_text_value (property_id - 1);
        g_value_set_string (value, text);
        break;
      }
    } else
      ;//G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);

  } else {
    switch (property_id) {
    case PROP_BRUSH_MODE:
      g_value_set_enum (value, options->brush_mode);
      break;

    case PROP_BRUSH_VIEW_TYPE:
      g_value_set_enum (value, options->brush_view_type);
      break;

    case PROP_BRUSH_VIEW_SIZE:
      g_value_set_int (value, options->brush_view_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}


GimpMypaintOptions *
gimp_mypaint_options_new (GimpToolInfo *tool_info)
{
  GimpMypaintOptions *options;

  options = GIMP_MYPAINT_OPTIONS (g_object_new (GIMP_TYPE_MYPAINT_OPTIONS,
                          "gimp",       tool_info->gimp,
                          "name",       "Mypaint",
                          NULL));

  return options;
}

GimpMypaintBrushMode
gimp_mypaint_options_get_brush_mode (GimpMypaintOptions *mypaint_options)
{
  GimpDynamics       *dynamics;
  GimpDynamicsOutput *force_output;

  g_return_val_if_fail (GIMP_IS_MYPAINT_OPTIONS (mypaint_options), GIMP_MYPAINT_NORMAL);

  return GIMP_MYPAINT_NORMAL;
}

void
gimp_mypaint_options_copy_brush_props (GimpMypaintOptions *src,
                                     GimpMypaintOptions *dest)
{
  gdouble  brush_size;
  gdouble  brush_angle;
  gdouble  brush_aspect_ratio;

  g_return_if_fail (GIMP_IS_MYPAINT_OPTIONS (src));
  g_return_if_fail (GIMP_IS_MYPAINT_OPTIONS (dest));

  g_object_get (src,
                "brush-size", &brush_size,
                "brush-angle", &brush_angle,
                "brush-aspect-ratio", &brush_aspect_ratio,
                NULL);

  g_object_set (dest,
                "brush-size", brush_size,
                "brush-angle", brush_angle,
                "brush-aspect-ratio", brush_aspect_ratio,
                NULL);
}

static void    
gimp_mypaint_options_mypaint_brush_changed (GObject *object,
                                            GimpData *data)
{
  g_return_if_fail (GIMP_IS_MYPAINT_OPTIONS (object));
  g_return_if_fail (GIMP_IS_MYPAINT_BRUSH (data));

  GimpMypaintOptions* options = GIMP_MYPAINT_OPTIONS(object);
  GimpData* brush_copied = gimp_mypaint_brush_duplicate(GIMP_MYPAINT_BRUSH(data));

  if (options->brush) {
    GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(options->brush->p);
    if (priv && priv->is_dirty()) {
      GimpMypaintOptionsHistory* history;
      history = GimpMypaintOptionsHistory::get_singleton();
      history->push_brush(options->brush);
    } else
      g_object_unref(options->brush);
  }

  options->brush = GIMP_MYPAINT_BRUSH(brush_copied);
  GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(options->brush->p);
  priv->clear_dirty_flag();
  
  List brush_settings = mypaint_brush_get_brush_settings ();  
  for (GList* i = brush_settings.ptr(); i; i = i->next) {
    MyPaintBrushSettings* setting = reinterpret_cast<MyPaintBrushSettings*>(i->data);
    CString signal(mypaint_brush_internal_name_to_signal_name(setting->internal_name));
    g_object_notify(object, signal.ptr());
  }

  List switch_settings = mypaint_brush_get_brush_switch_settings ();  
  for (GList* i = switch_settings.ptr(); i; i = i->next) {
    MyPaintBrushSwitchSettings* setting = reinterpret_cast<MyPaintBrushSwitchSettings*>(i->data);
    CString signal(mypaint_brush_internal_name_to_signal_name(setting->internal_name));
    g_object_notify(object, signal.ptr());
  }

  List text_settings = mypaint_brush_get_brush_text_settings ();  
  for (GList* i = brush_settings.ptr(); i; i = i->next) {
    MyPaintBrushTextSettings* setting = reinterpret_cast<MyPaintBrushTextSettings*>(i->data);
    CString signal (mypaint_brush_internal_name_to_signal_name(setting->internal_name));
    g_object_notify(object, signal.ptr());
  }
}

static void
gimp_mypaint_options_brush_changed (GObject* object, GimpData* data)
{
  auto options = ref(GIMP_MYPAINT_OPTIONS(object));
  auto brush = ref(data);

  if (!options.get("use-gimp-brushmark") || !options.get("brushmark-specified"))
    return;

  options.freeze();
  CString name = g_strdup(brush.get("name"));
  CString brushmark_name = g_strdup(options.get("brushmark-name"));
  if (g_strcmp0(name, brushmark_name) != 0) {
    options.set("brushmark-name", name.ptr());
  }
  options.thaw();
}

static void
gimp_mypaint_options_prop_brushmark_updated (GObject* object)
{
  auto options = ref(GIMP_MYPAINT_OPTIONS(object));

  if (!gimp_mypaint_options_is_ready(options.ptr()))
    return;

  if (!options.get("use-gimp-brushmark") || !options.get("brushmark-specified"))
    return;

  CString brushmark_name = g_strdup(options.get("brushmark-name"));

  auto brush = ref(gimp_context_get_brush(GIMP_CONTEXT(options.ptr())));
  CString name = g_strdup(brush? (const char*)brush.get("name"): "");

  options.freeze();
  if (options.get("brushmark-specified") && 
      (!brushmark_name.ptr() || strlen(brushmark_name) == 0) &&
      name && strlen(name) > 0) {
    // set current brush name as brushmark_name
    options.set("brushmark-name", name.ptr());
  } else if (brushmark_name.ptr() && g_strcmp0(name, brushmark_name) != 0) {
    auto container     = ref(gimp_data_factory_get_container(GIMP_CONTEXT(options.ptr())->gimp->brush_factory));
    auto matched_brush = ref(GIMP_BRUSH(gimp_container_get_child_by_name(container, brushmark_name)));
    if (matched_brush) {
      gimp_context_set_brush(GIMP_CONTEXT(options.ptr()), matched_brush);
    } else {
      g_print("no matching brush found for %s\n", brushmark_name.ptr());
    }
  }
  options.thaw();
}


static void
gimp_mypaint_options_texture_changed (GObject* object, GimpData* data)
{
  auto options = ref(GIMP_MYPAINT_OPTIONS(object));
  auto texture = ref(data);

  if (!options.get("use-gimp-texture") || !options.get("texture-specified"))
    return;

  options.freeze();
  CString name = g_strdup(texture.get("name"));
  CString texture_name = g_strdup(options.get("texture-name"));
  if (g_strcmp0(name, texture_name) != 0) {
    options.set("texture-name", name.ptr());
  }
  options.thaw();
}

static void
gimp_mypaint_options_prop_texture_updated (GObject* object)
{
  auto options = ref(GIMP_MYPAINT_OPTIONS(object));

  if (!gimp_mypaint_options_is_ready(options.ptr()))
    return;

  if (!options.get("use-gimp-texture") || !options.get("texture-specified"))
    return;

  CString texture_name = g_strdup(options.get("texture-name"));

  auto texture = ref(gimp_context_get_pattern(GIMP_CONTEXT(options.ptr())));
  CString name = g_strdup(texture? (const gchar*)texture.get("name"): "");

  options.freeze();
  if (options.get("texture-specified") && 
      (!texture_name.ptr() || strlen(texture_name) == 0) &&
      name && strlen(name) > 0) {
    // set current pattern name as texture_name
    options.set("texture-name", name.ptr());
  } else if (texture_name.ptr() && g_strcmp0(name, texture_name) != 0) {
    auto container       = ref(gimp_data_factory_get_container(GIMP_CONTEXT(options.ptr())->gimp->pattern_factory));
    auto matched_texture = ref(GIMP_PATTERN(gimp_container_get_child_by_name(container, texture_name)));
    if (matched_texture) {
      gimp_context_set_pattern(GIMP_CONTEXT(options.ptr()), matched_texture);
    } else {
      g_print("no matching texture found for %s\n", texture_name.ptr());
    }
  }
  options.thaw();
}

GimpMypaintBrush*
gimp_mypaint_options_get_current_brush (GimpMypaintOptions* options) 
{
  return options->brush;
}

void
gimp_mypaint_options_set_mapping_point(GimpMypaintOptions* options, 
                                       gchar* prop_name,
                                       guint inputs,
                                       guint size, 
                                       GimpVector2* points)
{
  IHashTable<gchar*, MyPaintBrushSettings*> brush_settings_dict = mypaint_brush_get_brush_settings_dict ();
  CString     brush_setting_name  = mypaint_brush_signal_name_to_internal_name(prop_name);

  MyPaintBrushSettings* brush_setting = brush_settings_dict[brush_setting_name];

  if (options->brush) {
    GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(options->brush->p);
    GimpMypaintBrushPrivate::Value* setting = priv->get_setting(brush_setting->index);

    if (setting && setting->mapping) {
      Mapping* mapping = setting->mapping;
      mapping->set_n(inputs, size);

      for (int i = 0; i < size; i ++)
        mapping->set_point(inputs, i, points[i].x, points[i].y);

    }

    priv->mark_as_dirty();

    g_object_notify(G_OBJECT(options), prop_name);
  }
}


void
gimp_mypaint_options_get_mapping_point(GimpMypaintOptions* options, 
                                       gchar*              prop_name,
                                       guint               inputs,
                                       guint*              size, 
                                       GimpVector2**       points)
{
  float x, y;
  IHashTable<gchar*, MyPaintBrushSettings*> brush_settings_dict = mypaint_brush_get_brush_settings_dict ();
  CString     brush_setting_name  = mypaint_brush_signal_name_to_internal_name(prop_name);

  MyPaintBrushSettings* brush_setting = brush_settings_dict[brush_setting_name];
  
  *size = 0;
  
  if (options->brush) {
    GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(options->brush->p);
    GimpMypaintBrushPrivate::Value* setting = priv->get_setting(brush_setting->index);
    
    if (setting && setting->mapping) {
      Mapping* mapping = setting->mapping;
      *size = mapping->get_n(inputs);

      *points = g_new0(GimpVector2, *size);
  
      for (int i = 0; i < *size; i++) {
        mapping->get_point(inputs, i, &x, &y);
        (*points)[i].x = x;
        (*points)[i].y = y;
      }
  
    }
  }
}

gboolean
gimp_mypaint_options_is_ready(GimpMypaintOptions* options)
{
  return options->brush ? TRUE: FALSE;
}

};
