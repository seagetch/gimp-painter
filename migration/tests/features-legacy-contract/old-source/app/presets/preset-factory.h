/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpPreset
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
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

#ifndef __PRESET_FACTORY_H__
#define __PRESET_FACTORY_H__

#ifdef __cplusplus

#include "base/glib-cxx-types.hpp"
#include "gimp-features.h"

extern "C" {
#endif


#include "core/gimpdata.h"
#include "core/gimpdatafactory.h"


#ifdef __cplusplus
}; // extern "C"

/////////////////////////////////////////////////////////////////////
class PresetConfig {
  GimpDataFactory* factory;

public:
  PresetConfig() : factory(NULL) { };
  virtual ~PresetConfig() {
    if (factory)
      gimp_data_factory_data_free (factory);
  };

  ////////////////////////////////////////////
  // Core Interface

  virtual GimpDataFactory*  get_data_factory            () { return factory; }
  virtual void              set_data_factory            (GimpDataFactory* f) { factory = f; }

  virtual const gchar*      get_readonly_path_prop_name () = 0;
  virtual const gchar*      get_writable_path_prop_name () = 0;
  virtual const gchar*      get_readonly_default_paths  () = 0;
  virtual const gchar*      get_writable_default_paths  () = 0;

  ////////////////////////////////////////////
  // GUI Interface

  virtual GObject*          create_view                 (GObject* context,
                                                         gint view_size,
                                                         GObject* menu_factory) { return NULL; }
  virtual GObject*          create_editor               () { return NULL; }


  ////////////////////////////////////////////
  // Internal use (for framework)

  class ConfigExtender {
    GLib::CString readonly_path;
    GLib::CString writable_path;
  public:
    ConfigExtender() : readonly_path(NULL), writable_path(NULL) { };
    virtual ~ConfigExtender() { }
    virtual const gchar*      get_readonly_paths ();
    virtual const gchar*      get_writable_paths ();
    virtual void              set_readonly_paths (const gchar* p);
    virtual void              set_writable_paths (const gchar* p);
  };

  virtual ConfigExtender*   get_extender                () { return new ConfigExtender(); }
  ConfigExtender*           get_extender_for            (GObject* obj);
  virtual void              extend                      (gpointer klass);

};

/////////////////////////////////////////////////////////////////////
class PresetFactory : virtual public GIMP::Feature
{
protected:
  GLib::HashTable _registry;

  GimpDataFactory* create_data_factory    (Gimp* gimp, PresetConfig* config);

  static GList*    load                   (GimpContext* context,
                                           const gchar* filename,
                                           GError** error);
public:
  PresetFactory() : GIMP::Feature() { set_lazy(true); }
  GLib::IHashTable<gconstpointer, PresetConfig*> registry();
  void             register_config        (PresetConfig* config);

  virtual void     initialize_factory     ();
  virtual void     entry_point            (Gimp* gimp);
  virtual void     initialize             (Gimp               *gimp,
                                           GimpInitStatusFunc  status_callback);
  virtual void     restore                (Gimp               *gimp,
                                           GimpInitStatusFunc  status_callback);
  virtual gboolean exit                   (Gimp               *gimp,
                                           gboolean            force);

  static GIMP::Feature* get_factory();
};

PresetFactory* get_preset_factory(void);

#endif // __cplusplus

#endif /* __PRESET_FACTORY_H__ */
