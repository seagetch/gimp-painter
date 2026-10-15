#include <type_traits>
#include <glib-object.h>
#include "base/delegators.hpp"
#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(_GObject, G_TYPE_OBJECT);
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"
extern "C" {
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
}
#include "presets/gimpjsonresource.h"
#include "presets/layer-preset.h"
extern "C" void composite_apply_preset(GimpContext *context,GimpLayer *source,const gchar *path) {
 GimpData *data=gimp_json_resource_new(context,"Composite fixture preset");
 GError *error=NULL;
 g_assert(JsonResourceInterface::cast(data)->load(context,path,&error));g_assert_no_error(error);
 ILayerPresetApplier *applier=ILayerPresetApplier::new_instance(context,GIMP_JSON_RESOURCE(data));
 applier->apply_for(source);
 g_print("ORIGINAL_PRESET_APPLIED %s\n",path);
 /* Retain context and preset for the capture process lifetime. */
}
