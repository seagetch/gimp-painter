/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * gimpbrush-load.c
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

extern "C" {

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#include <glib-object.h>
#include <glib/gstdio.h>
#include <cairo.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"

#ifdef G_OS_WIN32
#include "libgimpbase/gimpwin32-io.h"
#endif

#include <glib.h> /* GIOChannel */
#include <glib/gprintf.h> /* strip / split */


#include "core-types.h"

#include "base/temp-buf.h"

#include "gimpmypaintbrush.h"
#include "gimpmypaintbrush-load.h"
#include "mypaintbrush-brushsettings.h"
#include "gimpmypaintbrush-save.h"

#include "gimp-intl.h"

#include <json-glib/json-glib.h>

}
#include "gimpmypaintbrush-private.hpp"
#include "base/glib-cxx-utils.hpp"

using namespace GLib;

#define BRUSHFILE_JSON_VERSION 3
#define CURRENT_BRUSHFILE_VERSION 3

/*  local function prototypes  */

class MyPaintBrushReader {
  private:
  GLib::IObject<GimpMypaintBrush> result;
  GHashTable       *raw_pair;
  gint64            version;
  
  typedef gfloat (TransformY)(gfloat input);

  GHashTable* read_file_v2 (const gchar *filename, GError **error);
  void load_defaults   ();
  bool parse_v3        (const gchar *filename, GError **error);
  void parse_raw_v2    (gint64 version);
  void parse_points_v1 (gchar *string, gint inputs_index, Mapping *mapping, TransformY trans = NULL);
  void parse_points_v2 (gchar *string, gint inputs_index, Mapping *mapping, TransformY trans = NULL);
  gchar *unquote        (const gchar *string);
  void load_icon       (gchar *filename);
  void dump();
  
  public:
  MyPaintBrushReader();
  ~MyPaintBrushReader();
  GimpMypaintBrush *
  load_brush (GimpContext  *context,
              const gchar  *filename,
              GError      **error);

    
};

extern "C" {
/*  public functions  */

GList *
gimp_mypaint_brush_load (GimpContext  *context,
                 const gchar  *filename,
                 GError      **error)
{
  GimpMypaintBrush *brush;

//  g_print ("Read mypaint brush %s\n", filename);

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (g_path_is_absolute (filename), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  

  MyPaintBrushReader reader;
  brush = reader.load_brush (context, filename, error);

  if (! brush)
    return NULL;

  return g_list_prepend (NULL, brush);
}

}

/*  private functions  */


MyPaintBrushReader::MyPaintBrushReader() : result(), raw_pair(NULL), version(0)
{
}

MyPaintBrushReader::~MyPaintBrushReader()
{
  if (raw_pair)
    g_hash_table_unref (raw_pair);
}

GimpMypaintBrush*
MyPaintBrushReader::load_brush (GimpContext  *context,
                       const gchar  *filename,
                       GError      **error)
{
  CString basename  = g_path_get_basename (filename);
  CString brushname = g_strndup(basename.ptr(),
                                     strlen(basename.ptr()) - strlen(GIMP_MYPAINT_BRUSH_FILE_EXTENSION));
  version = 0;  

  result = GIMP_MYPAINT_BRUSH(gimp_mypaint_brush_new (context, brushname));
  load_defaults();
  
  if (!parse_v3(filename, error)) {
    g_print("Failed to read `%s': fallback to v2.\n", filename);
    // fallback to v2
    raw_pair = read_file_v2 (filename, error);
    parse_raw_v2 (version);
  }
  dump();
  
  CString filename_dup  = g_strndup(filename,
                                         strlen(filename) - strlen(GIMP_MYPAINT_BRUSH_FILE_EXTENSION));
  CString icon_filename = g_strconcat (filename_dup.ptr(), GIMP_MYPAINT_BRUSH_ICON_FILE_EXTENSION, NULL);
//  g_print ("Read Icon: %s\n", icon_filename.ptr());
  load_icon (icon_filename.ptr());
  GimpMypaintBrushPrivate* priv = reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);
  priv->clear_dirty_flag();
  return result;
}

void
MyPaintBrushReader::load_defaults ()
{
  List settings = mypaint_brush_get_brush_settings ();
  GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);

  for (GList* item = settings.ptr(); item; item = item->next) {
    MyPaintBrushSettings* setting = reinterpret_cast<MyPaintBrushSettings*>(item->data);
    priv->set_base_value(setting->index, setting->default_value);
//    priv->deallocate_mapping(setting->index);
    if (g_strcmp0(setting->internal_name, "opaque_multiply") == 0) {
      priv->allocate_mapping(setting->index);
      GimpMypaintBrushPrivate::Value* v = priv->get_setting(setting->index);
      v->mapping->set_n(INPUT_PRESSURE, 2);
      v->mapping->set_point(INPUT_PRESSURE, 0, 0.0, 0.0);
      v->mapping->set_point(INPUT_PRESSURE, 1, 1.0, 1.0);      
    }
  }
}

void
MyPaintBrushReader::dump ()
{
  List settings = mypaint_brush_get_brush_settings ();
  List inputs   = mypaint_brush_get_input_settings ();
  GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);
  
  for (GList* item = settings.ptr(); item; item = item->next) {
    MyPaintBrushSettings* setting = reinterpret_cast<MyPaintBrushSettings*>(item->data);
    GimpMypaintBrushPrivate::Value* v = priv->get_setting(setting->index);
    if (v->mapping) {
      for (GList* input_iter = inputs.ptr(); input_iter; input_iter = input_iter->next) {
        MyPaintBrushInputSettings* input = reinterpret_cast<MyPaintBrushInputSettings*>(input_iter->data);
        int n = v->mapping->get_n(input->index);
        if (n == 0)
          continue;
        for (int i = 0; i < n; i ++) {
          float x, y;
          v->mapping->get_point(input->index, i, &x, &y);
        }
      }
    }
  }
}

bool
MyPaintBrushReader::parse_v3(const gchar *filename, GError **error)
{
  auto parser = ref(json_parser_new());
  JsonNode* root;
  GError* jerror = NULL;
  json_parser_load_from_file(parser, filename, &jerror);
  *error = NULL;
  if (jerror) {
    *error = NULL;
    return false;
  }
  root = json_parser_get_root(parser);
  auto reader = ref(json_reader_new(root));
  GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);

  //version
  bool version_result = json_reader_read_member(reader, "version");
  if (version_result) {
    version = json_reader_get_int_value(reader);
    if (version < BRUSHFILE_JSON_VERSION) {
      // TBD: 
      g_print("[MyB-Load]Expected %d, but version is older (%ld)\n", BRUSHFILE_JSON_VERSION, version);
    } else if (version > CURRENT_BRUSHFILE_VERSION) {
      // TBD: 
      g_print("[MyB-Load]Expected %d, but version is newer!! (%ld)\n", CURRENT_BRUSHFILE_VERSION, version);
    }
  }
  json_reader_end_member(reader);
  //parent_brush_name
  bool brushname_result = json_reader_read_member(reader, "parent_brush_name");
  if (brushname_result) {
    if (strlen(json_reader_get_string_value(reader)) > 0) {
      CString uq_value = g_strdup(json_reader_get_string_value(reader));
      priv->set_parent_brush_name(uq_value);
      result.set("name", uq_value.ptr());
    } else {
      CString name = g_strdup(result.get("name"));
      priv->set_parent_brush_name(name.ptr());
    }
  }
  json_reader_end_member(reader);
  //group
  bool group_result = json_reader_read_member(reader, "group");
  if (group_result) {
    const gchar *uq_value = json_reader_get_string_value(reader);
    priv->set_group(uq_value);
  }
  json_reader_end_member(reader);
  //comment
  //settings
  bool settings_result = json_reader_read_member(reader, "settings");
  if (settings_result) {
    auto _settings_dict = hold(mypaint_brush_get_brush_settings_dict ());
    auto settings_dict  = ref<const gchar*, MyPaintBrushSettings*>(_settings_dict);
    auto _inputs_dict   = hold( mypaint_brush_get_input_settings_dict () );
    auto inputs_dict    = ref<const gchar*, MyPaintBrushInputSettings*>(_inputs_dict);

    bool element_result;
    const char* key;
    gint count = json_reader_count_members(reader);
    for (gint i = 0; i < count; i ++, json_reader_end_element(reader)) {
      json_reader_read_element(reader, i);
      key = json_reader_get_member_name(reader);
      MyPaintBrushSettings             *setting;
      setting = settings_dict[key];
      if (setting) {
        bool prop_result;
	prop_result = json_reader_read_member(reader, "base_value");
	if (!prop_result) {
          g_print ("invalid format for '%s'\n", key);
          json_reader_end_member(reader);
	  continue;
	}
	priv->set_base_value(setting->index, json_reader_get_double_value(reader));
        priv->allocate_mapping(setting->index);
        json_reader_end_member(reader);

	prop_result = json_reader_read_member(reader, "inputs");
	if (prop_result) {
          GimpMypaintBrushPrivate::Value* v = priv->get_setting(setting->index);
	  const char* input_key;
          gint input_count = json_reader_count_members(reader);
          for (gint j = 0; j < input_count; j ++, json_reader_end_element(reader)) {
            json_reader_read_element(reader, j);
            input_key = json_reader_get_member_name(reader);
            MyPaintBrushInputSettings *input_setting;
            input_setting = inputs_dict[input_key];
	    if (input_setting) {
              size_t num_seq = json_reader_count_elements(reader);
              v->mapping->set_n(input_setting->index, num_seq);
              for (int k = 0; k < num_seq; k ++, json_reader_end_element(reader)) {
                json_reader_read_element(reader, k);

                gdouble x_val, y_val;

                json_reader_read_element(reader, 0);
                x_val = json_reader_get_double_value(reader);
                json_reader_end_element(reader);

                json_reader_read_element(reader, 1);
                y_val = json_reader_get_double_value(reader);
                json_reader_end_element(reader);

                v->mapping->set_point(input_setting->index, k, (float)x_val, (float)y_val);
              }
	    }
	  }
	}
        json_reader_end_member(reader);
      } else {
        g_print ("unknown key '%s'\n", key);
      }

    }
  }
  json_reader_end_member(reader);

  //switches
  bool switch_result = json_reader_read_member(reader, "switches");
  if (switch_result) {
    auto _switches_dict = hold( mypaint_brush_get_brush_switch_settings_dict () );
    auto switches_dict  = ref<const gchar*, MyPaintBrushSwitchSettings*>(_switches_dict);

    gint count = json_reader_count_members(reader);
    const char* key;
    for(gint i = 0; i < count; i ++, json_reader_end_element(reader)) {
      json_reader_read_element(reader, i);
      key = json_reader_get_member_name(reader);
      MyPaintBrushSwitchSettings             *setting;
      setting = switches_dict[key];
      if (setting) {
        priv->set_bool_value(setting->index, json_reader_get_boolean_value(reader));
      } else {
        g_print ("unknown key '%s'\n", key);
      }
    }
  }
  json_reader_end_member(reader);

  //texts
  bool text_result = json_reader_read_member(reader, "texts");
  if (text_result) {
    auto _text_dict = hold( mypaint_brush_get_brush_text_settings_dict () );
    auto text_dict  = ref<const gchar*, MyPaintBrushTextSettings*>(_text_dict);

    gint count = json_reader_count_members(reader);
    const char* key;
    for (gint i = 0; i < count; i ++, json_reader_end_element(reader)) {
      json_reader_read_element(reader, i);
      key = json_reader_get_member_name(reader);
      MyPaintBrushTextSettings             *setting;
      setting = text_dict[key];
      if (setting) {
        priv->set_text_value(setting->index, json_reader_get_string_value(reader));
      } else {
        g_print ("unknown key '%s'\n", key);
      }
    }
  }
  json_reader_end_member(reader);

  return true;
}

GHashTable*
MyPaintBrushReader::read_file_v2 (const gchar *filename, GError **error)
{
  GIOChannel       *config_stream;
  gchar            *line;
  gsize             line_len;
  gsize             line_term_pos;

  g_return_val_if_fail ( GIMP_IS_MYPAINT_BRUSH (result.ptr()), NULL);

  config_stream = g_io_channel_new_file (filename, "r", error);
  ScopedPointer<GIOChannel, void(GIOChannel*), g_io_channel_unref> config_stream_holder(config_stream);
    
  if (! config_stream)
    {
      g_clear_error (error);
      return NULL;
    }
    
  raw_pair = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  
  while (g_io_channel_read_line (config_stream, &line, &line_len, &line_term_pos, error) != G_IO_STATUS_EOF)
    {
      CString line_holder(line);
      if (!line)
        break;
      
      line = g_strstrip (line);
      if (line[0] != '#')
        {
          gchar **tokens = g_strsplit (line, " ", 2);
          ScopedPointer<gchar*, void(gchar**), g_strfreev> token_holder(tokens);
          if (tokens[0] == 0) 
            {
              continue;
            }
          else if (g_strcmp0(tokens[0], "version") == 0) 
            {
              version = g_ascii_strtoll (tokens[1], NULL, 0);
              if (version > CURRENT_BRUSHFILE_VERSION)
                {
                  // TBD: 
                  break;
                }
            } 
          else
            {
              g_hash_table_insert (raw_pair, g_strdup(tokens[0]), g_strdup(tokens[1])); // Note: ownership of tokens is moved to GHashTable object.
            }
        }
    }
  return raw_pair;
}

gchar* 
MyPaintBrushReader::unquote (const gchar *string)
{
  return g_uri_unescape_string (string, NULL);
}

/// Parses the points list format from versions prior to version 2.
void
MyPaintBrushReader::parse_points_v1 (
    gchar *string, 
    gint inputs_index, 
    Mapping *mapping, 
    MyPaintBrushReader::TransformY* trans)
{
  gchar **iter;
  gint num_seq = 0;
  gfloat prev_x;
  gint i;
  
  g_print ("Parse v1 params : %s\n", string);

  ScopedPointer<gchar*, void(gchar**), g_strfreev> seq(g_strsplit (string, " ", 0)); 
  
  for (iter = seq.ptr(); *iter; iter++)
    num_seq ++;

  mapping->set_n(inputs_index, num_seq / 2);
  g_print ("set_n: %d\n", num_seq / 2);

  prev_x = 0;
  i = 0;
  for (iter = seq.ptr(); *iter; )
    {
      gdouble x_val, y_val;
      
      x_val = g_ascii_strtod (*(iter++), NULL);
      if (!(x_val > prev_x));
        {
          g_print ("Invalid input points: x > previous x\n");
          goto next;
        }
      prev_x = x_val;

      y_val = g_ascii_strtod (*(iter++), NULL);
      if (trans)
        y_val = (*trans)((gfloat)y_val);
      
      g_print ("X=%lf,Y=%lf\n", x_val, y_val);
      
      mapping->set_point(inputs_index, i ++, (float)x_val, (float)y_val);
    next:;;
    }  
}

/// Parses the newer points list format of v2 and beyond.
void
MyPaintBrushReader::parse_points_v2 (
    gchar *string, 
    gint inputs_index, 
    Mapping *mapping,
    MyPaintBrushReader::TransformY* trans)

{
  gchar **seq;
  gchar **iter;
  gint num_seq = 0;
  gint i;

  seq = g_strsplit (string, ", ", 0);  
  
  for (iter = seq; *iter; iter++)
    num_seq ++;

  mapping->set_n(inputs_index, num_seq);

  i = 0;
  for (iter = seq; *iter; iter++)
    {
      gchar *str_pos = *iter;
      gdouble x_val, y_val;
      str_pos = g_strstrip (str_pos);
      if (str_pos[0] != '(') 
        {
          // NG
          g_print ("ERROR: must be start with '(', but %c appears.\n", str_pos[0]);
        }
      str_pos = g_strstrip (++str_pos);
      x_val = g_ascii_strtod (str_pos, &str_pos);
      
      str_pos = g_strstrip (str_pos);
      y_val = g_ascii_strtod (str_pos, &str_pos);
      if (trans)
        y_val = (*trans)((gfloat)y_val);

      str_pos = g_strstrip (str_pos);
      if (str_pos[0] != ')') 
        {
          // NG
          g_print ("ERROR: must be end with ')', but %c appears.\n", str_pos[0]);
        }
      mapping->set_point(inputs_index, i ++, (float)x_val, (float)y_val);
    }  
}

void
MyPaintBrushReader::parse_raw_v2 (
                  gint64            version)
{
  auto _settings_dict = hold( mypaint_brush_get_brush_settings_dict () );
  auto settings_dict  = ref<const gchar*, MyPaintBrushSettings*>(_settings_dict);
  auto _inputs_dict   = hold( mypaint_brush_get_input_settings_dict () );
  auto inputs_dict    = ref<const gchar*, MyPaintBrushInputSettings*>(_inputs_dict);
  auto _migrate_dict  = hold( mypaint_brush_get_setting_migrate_dict () );
  auto migrate_dict   = ref<const gchar*, MyPaintBrushSettingMigrate*>(_migrate_dict);
  GHashTableIter    raw_pair_iter;
  gpointer          raw_key, raw_value;
  GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);

  g_hash_table_iter_init (&raw_pair_iter, raw_pair);
  
  while (g_hash_table_iter_next (&raw_pair_iter, &raw_key, &raw_value))
    {
      gchar    *key          = (gchar*)raw_key;
      gchar    *value        = (gchar*)raw_value;
      
      if (strcmp (key, "parent_brush_name") == 0)
        {
          CString uq_value = unquote (value);
          priv->set_parent_brush_name(uq_value);
          result.set("name", uq_value.ptr());
          goto next_pair;
        }
      else if (strcmp (key, "group") == 0)
        {
          CString uq_value = unquote (value);
          priv->set_group(value);
          goto next_pair;
        }
      else if (version <= 1 && strcmp (key, "color") == 0) 
        {
          ScopedPointer<gchar*, void(gchar**), g_strfreev>  tokens(g_strsplit(value, " ", 0));
          MyPaintBrushSettings               *setting;
          gint64 r, g, b;
          r = g_ascii_strtoll(tokens.ptr()[0], NULL, 10);
          g = g_ascii_strtoll(tokens.ptr()[1], NULL, 10);
          b = g_ascii_strtoll(tokens.ptr()[2], NULL, 10);
          GimpRGB rgb;
          rgb.r = r / 255.0; 
          rgb.g = g / 255.0; 
          rgb.b = b / 255.0; 
          rgb.a = 1.0;
          GimpHSV hsv;
          gimp_rgb_to_hsv (&rgb, &hsv);

          setting = settings_dict["color_h"];
          priv->set_base_value (setting->index, hsv.h);

          setting = settings_dict["color_s"];
          priv->set_base_value (setting->index, hsv.s);

          setting = settings_dict["color_v"];
          priv->set_base_value (setting->index, hsv.v);

          goto next_pair;
        }
      else if (version <= 1 && strcmp (key, "change_radius") == 0)
        {
          if (strcmp (value, "0.0") == 0)
            goto next_pair;
          else
            {
              //FIXME! ERROR
              //raise Obsolete, 'change_radius is not supported any more'
            }
        }
      else if (version <= 2 && strcmp (key, "adapt_color_from_image") == 0)
        {
          if (strcmp (value, "0.0") == 0)
            goto next_pair;
          else
            {
              //FIXME! ERROR
              //raise Obsolete, 'adapt_color_from_image is obsolete, ignored;' + \
              //                ' use smudge and smudge_length instead'
            }
        }
      else if (version <= 1 && strcmp (key, "painting_time") == 0)
        {
          goto next_pair;
        }      
      else if (version <= 1 && strcmp (key, "speed") == 0)
        {
          key = (gchar*)"speed1";
        }
      {
      MyPaintBrushSettingMigrate *migrate = migrate_dict[key];
      if (migrate)
        {
          if (migrate->new_name)
            key = migrate->new_name;
        }
      
      
      MyPaintBrushSettings             *setting;
      setting = settings_dict[key];
      if (setting)
        {
          ScopedPointer<gchar*, void(gchar**), g_strfreev>   split_values(g_strsplit (value, "|", 0));
          gchar                          **values_pos;
          values_pos = split_values.ptr();
          
          gdouble d_val = g_ascii_strtod (*values_pos, NULL);
          if (migrate && migrate->transform)
            d_val = (*migrate->transform)((gfloat)d_val);

        
          priv->set_base_value(setting->index, d_val);
          values_pos ++;      
          if (*values_pos)
            {
              priv->allocate_mapping(setting->index);
            }
          GimpMypaintBrushPrivate::Value* v = priv->get_setting(setting->index);
          while (*values_pos)
            {
              //      inputname, rawpoints = part.strip().split(' ', 1)
              gchar *str_pos = *values_pos;
              str_pos = g_strstrip (str_pos);
              ScopedPointer<gchar *, void(gchar**), g_strfreev> tokens(g_strsplit (str_pos, " ", 2));
              MyPaintBrushInputSettings *input_setting;
              input_setting = inputs_dict[tokens.ptr()[0]];
              if (tokens.ptr()[1] && input_setting)
                {
                  if (version <= 1)
                    parse_points_v1 (tokens.ptr()[1], input_setting->index, v->mapping, migrate? migrate->transform: NULL);
                  else
                    parse_points_v2 (tokens.ptr()[1], input_setting->index, v->mapping, migrate? migrate->transform: NULL);
                }
              else
                {
                  g_print ("invalid parameter '%s'\n", *values_pos);
                }
              values_pos ++;
            }
        }
      else
        {
          g_print ("invalid key '%s'\n", key);
        }
      }
    next_pair:
      ;;
    }
}

void
MyPaintBrushReader::load_icon (gchar *filename)
{
//  g_print("ICON_IMAGE=%s\n", filename);
  GimpMypaintBrushPrivate *priv = reinterpret_cast<GimpMypaintBrushPrivate*>(result->p);
	cairo_surface_t* icon_image = cairo_image_surface_create_from_png(filename);
  priv->set_icon_image(icon_image);
  cairo_surface_destroy (icon_image);
}
