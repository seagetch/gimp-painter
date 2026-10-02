/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_LAYER_PRESET_H__
#define __GIMP_LAYER_PRESET_H__
typedef struct _JsonNode JsonNode;
#include "gimpdata.h"
G_BEGIN_DECLS
#define GIMP_TYPE_LAYER_PRESET (gimp_layer_preset_get_type ())
#define GIMP_LAYER_PRESET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_LAYER_PRESET, GimpLayerPreset))
#define GIMP_IS_LAYER_PRESET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_LAYER_PRESET))
typedef struct _GimpLayerPreset GimpLayerPreset;
typedef struct _GimpLayerPresetClass GimpLayerPresetClass;
struct _GimpLayerPreset { GimpData parent_instance; JsonNode *root; };
struct _GimpLayerPresetClass { GimpDataClass parent_class; };
GType gimp_layer_preset_get_type (void) G_GNUC_CONST;
GimpData *gimp_layer_preset_new (GimpContext *, const gchar *name);
GList *gimp_layer_preset_load (GimpContext *, GFile *, GInputStream *, GError **);
/* Copies the complete JSON tree, including unknown fields; never borrows it. */
gboolean gimp_layer_preset_set_json (GimpLayerPreset *, JsonNode *, GError **);
JsonNode *gimp_layer_preset_dup_json (GimpLayerPreset *);
gboolean gimp_layer_preset_matches (GimpLayerPreset *, GimpLayer *);
/* Single owner-thread transaction. Existing source identity is retained. */
gboolean gimp_layer_preset_apply (GimpLayerPreset *, GimpContext *, GimpLayer *, GError **);
gboolean gimp_layer_preset_apply_full (GimpLayerPreset *, GimpContext *, GimpLayer *, GCancellable *, GError **);
G_END_DECLS
#endif
