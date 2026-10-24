#define PAIRED_MODEL_NO_MAIN
#include "/workspace/shared/gimp-native-preset-gui/source/app/tests/test-paired-stroke-undo.cpp"
extern "C" {
#include "core/gimplayermask.h"
#include "core/gimpcontext.h"
}
struct GeometryProbe {PairFixture*f;std::string before,painted;std::vector<guchar> baseline,edited;const char*phase="edit";unsigned sequence=0;
 void record(const char*event){auto*d=f->d;auto state=identity(*f->core);auto p=pixels(d);g_print("EVENT %u phase=%s event=%s geometry=%d,%d,%d,%d state_before=%d state_painted=%d pixels_before=%d pixels_edited=%d active=%d\n",++sequence,phase,event,gimp_item_get_offset_x(GIMP_ITEM(d)),gimp_item_get_offset_y(GIMP_ITEM(d)),gimp_item_get_width(GIMP_ITEM(d)),gimp_item_get_height(GIMP_ITEM(d)),state==before,state==painted,p==baseline,p==edited,f->core->active());}
};
static void gp_update(GimpDrawable*,gint,gint,gint,gint,gpointer p){static_cast<GeometryProbe*>(p)->record("update");}
static void gp_notify(GObject*,GParamSpec*s,gpointer p){static_cast<GeometryProbe*>(p)->record(s->name);}
static void gp_changed(GeglBuffer*,const GeglRectangle*,gpointer p){static_cast<GeometryProbe*>(p)->record("buffer-changed");}
static void gp_size(GimpViewable*,gpointer p){static_cast<GeometryProbe*>(p)->record("size-changed");}
static void geometry_probe(){const char*kind=g_getenv("PROBE_KIND");bool mask=g_getenv("PROBE_MASK")!=nullptr;PairFixture f;if(mask){auto*m=gimp_layer_create_mask(f.layer,GIMP_ADD_MASK_WHITE,nullptr);g_assert_nonnull(gimp_layer_add_mask(f.layer,m,FALSE,nullptr));f.d=GIMP_DRAWABLE(m);}gimp_item_set_offset(GIMP_ITEM(f.d),3,5);GeometryProbe p{&f,identity(*f.core),"",pixels(f.d),{}};f.core->begin_batch(true);stroke(*f.core,f.d);p.painted=identity(*f.core);auto*b=gimp_drawable_get_buffer(f.d);g_object_ref(b);auto u=g_signal_connect(f.d,"update",G_CALLBACK(gp_update),&p);auto n=g_signal_connect(f.d,"notify",G_CALLBACK(gp_notify),&p);auto s=g_signal_connect(f.d,"size-changed",G_CALLBACK(gp_size),&p);auto c=gegl_buffer_signal_connect(b,"changed",G_CALLBACK(gp_changed),&p);g_print("CASE kind=%s mask=%d\n",kind,mask);p.record("before-external");
if(!strcmp(kind,"offset"))gimp_item_set_offset(GIMP_ITEM(f.d),7,8);
else if(!strcmp(kind,"replace")){auto*copy=gegl_buffer_dup(b);GeglColor*color=gegl_color_new("red");gegl_buffer_set_color(copy,nullptr,color);g_object_unref(color);gimp_drawable_set_buffer(f.d,FALSE,nullptr,copy);g_object_unref(copy);}
else gimp_item_resize(GIMP_ITEM(f.d),gimp_get_user_context(gimp),GIMP_FILL_TRANSPARENT,!strcmp(kind,"grow")?72:20,!strcmp(kind,"grow")?56:16,0,0);
p.edited=pixels(f.d);p.record("after-external");p.phase="cancel";try{f.core->end_batch(false);}catch(const std::exception&e){g_print("EXCEPTION %s\n",e.what());}p.record("after-cancel");g_signal_handler_disconnect(f.d,u);g_signal_handler_disconnect(f.d,n);g_signal_handler_disconnect(f.d,s);g_signal_handler_disconnect(b,c);g_object_unref(b);}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();g_object_set(gimp->config,"painter-canvas-ui",TRUE,nullptr);g_test_add_func("/geometry/probe",geometry_probe);int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;}
