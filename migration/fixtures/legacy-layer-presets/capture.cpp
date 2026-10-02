/* SPDX-License-Identifier: GPL-3.0-or-later
 * Calls the pinned, unchanged real LayerPresetApplier. */
#include "base/delegators.hpp"
#include "base/glib-cxx-types.hpp"
__DECLARE_GTK_CLASS__(_GObject, G_TYPE_OBJECT);
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"
extern "C" {
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <stdio.h>
#include <signal.h>
#include <execinfo.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpcolor/gimpcolor.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
#include "tests.h"
}
#include "core/gimpfilterlayer.h"
#include "core/gimpclonelayer.h"
#include "presets/gimpjsonresource.h"
#include "presets/layer-preset.h"
static void dump (const char *file, GimpContainer *container, const std::string &parent, GimpLayer *source)
{
  int i = 0;
  for (GList *it = GIMP_LIST (container)->list; it; it = it->next, ++i)
    {
      GimpLayer *layer = GIMP_LAYER (it->data);
      std::string path = parent + "/" + std::to_string(i);
      const char *type = GIMP_IS_GROUP_LAYER(layer)?"group":GIMP_IS_CLONE_LAYER(layer)?"clone":GIMP_IS_FILTER_LAYER(layer)?"filter":"normal";
      printf("PRESET\t%s\t%s\t%s\t%s\t%d\t%.17g\t%d\t%d\t%d\t%d\t%d\t%d",file,path.c_str(),type,gimp_object_get_name(layer),(int)gimp_layer_get_mode(layer),gimp_layer_get_opacity(layer),gimp_item_get_offset_x(GIMP_ITEM(layer)),gimp_item_get_offset_y(GIMP_ITEM(layer)),gimp_item_get_width(GIMP_ITEM(layer)),gimp_item_get_height(GIMP_ITEM(layer)),layer==source,GIMP_IS_CLONE_LAYER(layer)&&gimp_clone_layer_get_source(GIMP_CLONE_LAYER(layer))==source);
      if (GIMP_IS_FILTER_LAYER(layer)) {
        const char *proc=gimp_filter_layer_get_procedure(GIMP_FILTER_LAYER(layer));
        printf("\t%s",proc?proc:"");
        GArray *args=gimp_filter_layer_get_procedure_args(GIMP_FILTER_LAYER(layer));
        if(args)for(guint j=0;j<args->len;++j){GValue *v=&g_array_index(args,GValue,j);printf("\t%s:",G_VALUE_TYPE(v)?G_VALUE_TYPE_NAME(v):"null");if(G_VALUE_HOLDS_FLOAT(v))printf("%.17g",(double)g_value_get_float(v));else if(G_VALUE_HOLDS_DOUBLE(v))printf("%.17g",g_value_get_double(v));else if(G_VALUE_HOLDS_INT(v))printf("%d",g_value_get_int(v));}
      }
      puts("");
      if(GIMP_IS_GROUP_LAYER(layer))dump(file,gimp_viewable_get_children(GIMP_VIEWABLE(layer)),path,source);
    }
}
static void crash(int signum){void *frames[80];int n=backtrace(frames,80);backtrace_symbols_fd(frames,n,2);_exit(128+signum);}
int main(int argc,char **argv)
{
  setvbuf(stdout,NULL,_IONBF,0);signal(SIGSEGV,crash);Gimp *gimp=gimp_init_for_testing();
  const char *names[]={"crop-with-selection.json","duplicate-as-clone.json","duplicate-as-normal.json","new-anime-layer.json","new-cropped-layer.json","test.json","test2.json","watercolor.json"};
  for(const char *name:names){
    GimpImage *image=gimp_image_new(gimp,100,80,GIMP_RGB);
    GimpLayer *source=(strcmp(name,"test.json")==0||strcmp(name,"test2.json")==0)?
      FilterLayerInterface::new_instance(image,40,30,"source",.6,GIMP_SCREEN_MODE):
      gimp_layer_new(image,40,30,GIMP_RGBA_IMAGE,"source",.6,GIMP_SCREEN_MODE);
    gimp_image_add_layer(image,source,NULL,0,FALSE);gimp_item_set_offset(GIMP_ITEM(source),7,9);
    gimp_context_set_image(gimp->user_context,image);
    gimp_channel_select_rectangle(gimp_image_get_mask(image),12,15,20,17,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
    gchar *filename=g_build_filename(argv[1],name,NULL);GError *error=NULL;
    auto *resource=GIMP_JSON_RESOURCE(gimp_json_resource_new(gimp->user_context,name));
    g_assert(JsonResourceInterface::cast(resource)->load(gimp->user_context,filename,&error));g_assert_no_error(error);
    ILayerPresetApplier *applier=ILayerPresetApplier::new_instance(gimp->user_context,resource);
    applier->apply_for(source);delete applier;
    dump(name,gimp_image_get_layers(image),"",source);
    printf("PRESET_UNDO\t%s\t%d\n",name,gimp_image_get_undo_group_count(image));
    /* Old JsonResource destructor wrongly g_object_unref()s JsonNode.
     * Keep fixture resources alive until process exit; never patch old applier. */
    g_free(filename);gimp_context_set_image(gimp->user_context,NULL);g_object_unref(image);
  }
  puts("LAYER_PRESET_CAPTURE_COMPLETE");return 0;
}
