/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_JSON_RESOURCE_H
#define GIMP_JSON_RESOURCE_H
#include "gimpdata.h"
typedef struct _JsonNode JsonNode;
G_BEGIN_DECLS
#define GIMP_TYPE_JSON_RESOURCE (gimp_json_resource_get_type ())
#define GIMP_JSON_RESOURCE(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_JSON_RESOURCE, GimpJsonResource))
#define GIMP_IS_JSON_RESOURCE(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_JSON_RESOURCE))
#define GIMP_JSON_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_JSON_RESOURCE, GimpJsonResourceClass))
#define GIMP_IS_JSON_RESOURCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_JSON_RESOURCE))
#define GIMP_JSON_RESOURCE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_JSON_RESOURCE, GimpJsonResourceClass))
typedef struct _GimpJsonResource GimpJsonResource;
typedef struct _GimpJsonResourceClass GimpJsonResourceClass;
struct _GimpJsonResource { GimpData parent_instance; };
struct _GimpJsonResourceClass {
  GimpDataClass parent_class;
  /* Native subclass policy, not a historical binary class-layout promise. */
  gboolean (*prepare_root) (GimpJsonResource *, JsonNode *, JsonNode **owned);
  void (*root_changed) (GimpJsonResource *, JsonNode *);
};
GType gimp_json_resource_get_type (void) G_GNUC_CONST;
GimpData *gimp_json_resource_new (GimpContext *context, const gchar *name);
/* Nullable arbitrary JSON. Like the original root property these retain the
 * node/tree; specialized resources may make their own deep copy first. */
gboolean gimp_json_resource_set_json (GimpJsonResource *, JsonNode *);
JsonNode *gimp_json_resource_ref_json (GimpJsonResource *);
gboolean gimp_json_resource_is_live (GimpJsonResource *);
G_END_DECLS
#endif
