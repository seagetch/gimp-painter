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

#include "base/delegators.hpp"
#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(GObject, G_TYPE_OBJECT);
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"

extern "C" {
#include "config.h"
#include <string.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "libgimpmath/gimpmath.h"

#ifdef G_OS_WIN32
#include "libgimpbase/gimpwin32-io.h"
#endif

//#include "presets-types.h"
#include "presets-enums.h"

#include "core/gimp.h"

#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdata.h"

#include "gimp-intl.h"

#include "core/gimpparamspecs.h"
#include "config/gimpcoreconfig.h"
}
#include "gimpjsonresource.h"


#include "preset-factory.h"
#include "layer-preset.h"


using namespace GLib;

GLib::GClassWrapper<UseCStructs<GimpBaseConfig, GimpCoreConfig> > core_class;

////////////////////////////////////////////////////////////////////////////
// PresetConfig

const gchar*
PresetConfig::ConfigExtender::get_readonly_paths ()
{
  g_print("get::readonly path\n");
  return readonly_path;
}
const gchar*
PresetConfig::ConfigExtender::get_writable_paths ()
{
  g_print("get::writable path\n");
  return writable_path;
}
void
PresetConfig::ConfigExtender::set_readonly_paths (const gchar* p)
{
  g_print("set::readonly path to `%s'\n", p);
  readonly_path = g_strdup(p);
}
void
PresetConfig::ConfigExtender::set_writable_paths (const gchar* p)
{
  g_print("set::writable path to `%s'\n", p);
  writable_path = g_strdup(p);
}


PresetConfig::ConfigExtender*
PresetConfig::get_extender_for(GObject* obj)
{
  CString type_key         = g_strdup_printf("config_extend_%p", &typeid(*this));
  ConfigExtender* extender = (PresetConfig::ConfigExtender*)(g_object_get_data(obj, type_key) );
  if (!extender) {
    g_print("Install ConfigExtender.\n");
    extender = get_extender();
    g_object_set_cxx_object(obj, (const gchar*)type_key, extender);
  }
  return extender;
}


void
PresetConfig::extend (gpointer klass)
{
  const gchar* readonly_prop = get_readonly_path_prop_name();
  const gchar* writable_prop = get_writable_path_prop_name();
  const gchar* readonly_path = get_readonly_default_paths();
  const gchar* writable_path = get_writable_default_paths();

  g_print("  Installing %s with default = %s\n", readonly_prop, readonly_path);
  g_print("  Installing %s with default = %s\n", writable_prop, writable_path);
  core_class.with_class(klass)->
      install_property(gimp_param_spec_config_path (
                           readonly_prop, NULL, NULL,
                           GIMP_CONFIG_PATH_DIR_LIST, readonly_path,
                           GParamFlags(GIMP_PARAM_STATIC_STRINGS |GIMP_CONFIG_PARAM_RESTART |GIMP_CONFIG_PARAM_FLAGS)),
                       [this] (GObject* obj) -> CopyValue { // getter
                         auto extender = this->get_extender_for(obj);
                         return extender->get_readonly_paths();
                       },
                       [this] (GObject* obj, IValue src) { // setter
                         auto extender = this->get_extender_for(obj);
                         extender->set_readonly_paths(src);
                       }
                       )->
      install_property(gimp_param_spec_config_path (
                           writable_prop, NULL, NULL,
                           GIMP_CONFIG_PATH_DIR_LIST, writable_path,
                           GParamFlags(GIMP_PARAM_STATIC_STRINGS |GIMP_CONFIG_PARAM_RESTART |GIMP_CONFIG_PARAM_FLAGS)),
                       [this] (GObject* obj) -> CopyValue { // getter
                         auto extender = this->get_extender_for(obj);
                         return extender->get_writable_paths();
                       },
                       [this] (GObject* obj, IValue src) { // setter
                         auto extender = this->get_extender_for(obj);
                         extender->set_writable_paths(src);
                       }
                       );
}


////////////////////////////////////////////////////////////////////////////
// PresetFactory

static PresetFactory* singleton = NULL;
GIMP::Feature* PresetFactory::get_factory() {
  if (!singleton)
    singleton = new PresetFactory();
  return singleton;
}


GLib::IHashTable<gconstpointer, PresetConfig*> PresetFactory::registry()
{
  if (!_registry)
    _registry = g_hash_table_new(NULL, NULL);
  return ref<gconstpointer, PresetConfig*>(_registry);
}


void
PresetFactory::register_config (PresetConfig* config)
{
  gconstpointer type_ptr = &typeid(config);
  registry().insert(type_ptr, config);
}


void
PresetFactory::initialize_factory()
{
  register_config( LayerPresetConfig::config() );
}


void PresetFactory::entry_point(Gimp* gimp)
{
  initialize_factory();

  GIMP::Feature::entry_point(gimp);

  GimpCoreConfigClass* klass = GIMP_CORE_CONFIG_CLASS (g_type_class_ref(GIMP_TYPE_CORE_CONFIG));

  registry().each([&](auto key, auto it) {
    it->extend(klass);
  });
  g_type_class_unref(klass);
}


GList*
PresetFactory::load (GimpContext* context,
                     const gchar* filename,
                     GError**     error)
{
  CString basename  = g_path_get_basename (filename);
  CString res_name = g_strndup(basename, strlen(basename) - strlen(".json"));
  g_print("PresetFactory::load:: %s\n", filename);

  auto json_res = GIMP_JSON_RESOURCE(JsonResourceInterface::new_instance(context, res_name));
  auto json_impl = JsonResourceInterface::cast(json_res);

  json_impl->load(context, filename, error);

  return g_list_prepend(NULL, json_res);
}


GimpDataFactory*
PresetFactory::create_data_factory (Gimp* gimp, PresetConfig* config)
{
  const gchar* readonly_prop = config->get_readonly_path_prop_name();
  const gchar* writable_prop = config->get_writable_path_prop_name();
  static GimpDataFactoryLoaderEntry entries[] = {
      {
        PresetFactory::load, //load function
        ".json", // extension
        writable_prop != NULL // writable
      }
  };

  GimpDataFactory* result =
      gimp_data_factory_new (gimp,
                             GLib::Traits<GimpJsonResource>::get_type(),
                             readonly_prop, writable_prop,
                             entries,
                             G_N_ELEMENTS (entries),
                             JsonResourceInterface::new_instance,
                             NULL);
  return result;
}


void
PresetFactory::initialize (Gimp               *gimp,
                           GimpInitStatusFunc  status_callback)
{
  registry().each([&](auto key, auto it) {
    GimpDataFactory* factory = this->create_data_factory(gimp, it);
    it->set_data_factory(factory);
  });
}


void
PresetFactory::restore (Gimp               *gimp,
                        GimpInitStatusFunc  status_callback)
{
  registry().each([&](auto key, auto it) {
    GimpDataFactory* factory = it->get_data_factory();
    gimp_data_factory_data_init (factory, gimp->user_context,
                                 gimp->no_data);
    gimp_register_data_factory (gimp, "layer-preset", factory);                                 
  });

  // Dialog initialization.
}


gboolean
PresetFactory::exit(Gimp *gimp,
                    gboolean force)
{
  registry().each([&](auto key, auto it) {
    GimpDataFactory* factory = it->get_data_factory();
    gimp_data_factory_data_save (factory);
  });
  return FALSE;
}
