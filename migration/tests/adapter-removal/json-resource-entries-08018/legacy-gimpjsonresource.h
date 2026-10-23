/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * GimpJsonResource
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

#ifndef __GIMPJSONRESOURCE_H__
#define __GIMPJSONRESOURCE_H__

#ifdef __cplusplus

extern "C" {
#include "base/glib-cxx-types.hpp"
#endif


#include "core/gimpdata.h"
#include "core/gimpdatafactory.h"
#include <json-glib/json-glib.h>

#define GIMP_TYPE_JSON_RESOURCE            (gimp_json_resource_get_type ())
#define GIMP_JSON_RESOURCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_JSON_RESOURCE, GimpJsonResource))
#define GIMP_JSON_RESOURCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_JSON_RESOURCE, GimpJsonResourceClass))
#define GIMP_IS_JSON_RESOURCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_JSON_RESOURCE))
#define GIMP_IS_JSON_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_JSON_RESOURCE))
#define GIMP_JSON_RESOURCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_JSON_RESOURCE, GimpJsonResourceClass))



struct _GimpJsonResource
{
  GimpData  parent_instance;
};

struct GimpJsonResourceClass
{
  GimpDataClass parent_class;
};

GType     gimp_json_resource_get_type       (void) G_GNUC_CONST;
GimpData* gimp_json_resource_new            (GimpContext* context, const gchar* name);


#ifdef __cplusplus
} /* extern "C" */

/////////////////////////////////////////////////////////////////////
class JsonResourceInterface {
public:

  // static methods

  static GimpData*              new_instance   (GimpContext* context, const gchar* name);
  static JsonResourceInterface* cast           (gpointer obj);
  static bool                   is_instance    (gpointer obj);

  // instance methods

  virtual ~JsonResourceInterface () { };

  virtual JsonNode* get_json     ()                      = 0;
  virtual void      set_json     (JsonNode* node)        = 0;
  virtual gboolean  load         (GimpContext* context,
                                  const gchar* filename,
                                  GError** error)        = 0;
  virtual gboolean  save         (GError** error)        = 0;

};

namespace GLib {
template<> inline GType g_type<GimpJsonResource>() { return gimp_json_resource_get_type(); }
template<> inline auto g_class_type<GimpJsonResource>(const GimpJsonResource* instance) { return GIMP_JSON_RESOURCE_GET_CLASS(instance); }
};

#endif // __cplusplus

#endif /* __GIMPJSONRESOURCE_H__ */
