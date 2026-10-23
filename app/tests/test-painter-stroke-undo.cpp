/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <memory>
#include <vector>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpimage-new.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "widgets/widgets-types.h"
#include "core/gimpdrawable.h"
#include "core/gimpprojection.h"
#include "core/gimppickable.h"
#include "core/gimpundostack.h"
#include "core/gimpundopopguard.h"
#include "paint/gimpmypaintcoreundo.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/gimpmypaintcoreundo.hpp"
using namespace GimpPainter::MyPaint;
namespace {
static Gimp*app;
struct GIMP_PAINTER_PRIVATE State:StrokeState {
 GimpUndo*entry=nullptr;bool close_prepare=false,reenter_finish=false;int live=2,prepares=0,applies=0,finishes=0;bool locked=false,blocked=false,expired=false;
 bool prepare(GimpUndoMode)noexcept override{++prepares;if(blocked||locked)return false;locked=true;if(close_prepare)g_object_run_dispose(G_OBJECT(entry));return true;}
 void apply(GimpUndoMode m)noexcept override{g_assert_true(locked);++applies;if(!expired)live=m==GIMP_UNDO_MODE_UNDO?1:2;}
 void finish(GimpUndoMode)noexcept override{g_assert_true(locked);locked=false;++finishes;if(reenter_finish){gimp_undo_pop_guard_finish(entry,GIMP_UNDO_MODE_UNDO);g_object_run_dispose(G_OBJECT(entry));}}
 std::size_t memsize()const noexcept override{return sizeof(*this);}
};
static void fill(GeglBuffer*b,const char*c){auto*v=gegl_color_new(c);gegl_buffer_set_color(b,nullptr,v);g_object_unref(v);}
static int pixel(GimpDrawable*d){guchar p[4];gegl_buffer_get(gimp_drawable_get_buffer(d),GEGL_RECTANGLE(0,0,1,1),1,babl_format("R'G'B'A u8"),p,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p[0]>200?1:2;}
struct Fixture {
 GimpImage*image;GimpLayer*layer;GimpDrawable*drawable;GeglBuffer*before;GimpUndo*undo=nullptr;std::shared_ptr<State>state=std::make_shared<State>();
 Fixture(int width=8,int height=8){image=gimp_image_new(app,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);layer=gimp_layer_new(image,width,height,babl_format("R'G'B'A u8"),"Paired history",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));drawable=GIMP_DRAWABLE(layer);fill(gimp_drawable_get_buffer(drawable),"blue");before=gegl_buffer_new(GEGL_RECTANGLE(0,0,width,height),gimp_drawable_get_format(drawable));fill(before,"red");gimp_image_undo_free(image);}
 void create(){GError*e=nullptr;undo=create_stroke_undo(image,drawable,before,0,0,"Paired test",state,&e);g_assert_no_error(e);g_assert_nonnull(undo);g_assert_true(GIMP_IS_MYPAINT_CORE_UNDO(undo));g_assert_true(GIMP_IS_DRAWABLE_UNDO(undo));}
 void publish(){if(!undo)create();GError*e=nullptr;g_object_ref(undo);g_assert_true(gimp_image_undo_push_prepared(image,undo,&e)==undo);g_assert_no_error(e);}
 ~Fixture(){if(undo)g_object_unref(undo);g_object_unref(before);g_object_unref(image);}
};
static void roundtrip(){Fixture f;f.publish();for(int i=0;i<3;i++){f.state->live=99;g_assert_true(gimp_image_undo(f.image));g_assert_cmpint(pixel(f.drawable),==,1);g_assert_cmpint(f.state->live,==,1);g_assert_true(gimp_image_redo(f.image));g_assert_cmpint(pixel(f.drawable),==,2);g_assert_cmpint(f.state->live,==,2);}g_assert_cmpint(f.state->prepares,==,6);g_assert_cmpint(f.state->finishes,==,6);g_assert_false(f.state->locked);}
static void rejection(){Fixture f;f.state->blocked=true;f.create();g_assert_cmpint(f.state->prepares,==,0);f.publish();auto*top=gimp_undo_stack_peek(gimp_image_get_undo_stack(f.image));g_assert_false(gimp_image_undo(f.image));g_assert_true(top==gimp_undo_stack_peek(gimp_image_get_undo_stack(f.image)));g_assert_cmpint(pixel(f.drawable),==,2);g_assert_cmpint(f.state->applies,==,0);g_assert_cmpint(f.state->finishes,==,0);f.state->blocked=false;g_assert_true(gimp_image_undo(f.image));}
static void all_pixels(GeglBuffer*b,int expected){auto r=*gegl_buffer_get_extent(b);std::vector<guchar>pixels(std::size_t(r.width)*r.height*4);gegl_buffer_get(b,&r,1,babl_format("R'G'B'A u8"),pixels.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);for(std::size_t i=0;i<pixels.size();i+=4){g_assert_cmpint(pixels[i],==,expected==1?255:0);g_assert_cmpint(pixels[i+1],==,0);g_assert_cmpint(pixels[i+2],==,expected==1?0:255);g_assert_cmpint(pixels[i+3],==,255);}}
static void projection_pixels(Fixture&f,int expected){auto*p=gimp_image_get_projection(f.image);auto*b=gimp_pickable_get_buffer(GIMP_PICKABLE(p));gimp_projection_flush_now(p,TRUE);all_pixels(b,expected);}
struct Observe {Fixture*f;int expected,calls=0;bool dispose=false,projection=false;};
static void update(GimpDrawable*,gint,gint,gint,gint,gpointer data){auto&o=*static_cast<Observe*>(data);++o.calls;g_assert_true(o.f->state->locked);g_assert_cmpint(pixel(o.f->drawable),==,o.expected);g_assert_cmpint(o.f->state->live,==,o.expected);if(o.projection)projection_pixels(*o.f,o.expected);g_assert_false(gimp_image_undo(o.f->image));g_assert_false(gimp_image_redo(o.f->image));if(o.dispose){gimp_undo_free(o.f->undo,GIMP_UNDO_MODE_UNDO);g_object_run_dispose(G_OBJECT(o.f->undo));g_assert_true(o.f->state->locked);}}
static void observer_coherence(){Fixture f;f.publish();Observe o{&f,1,0,false};auto h=g_signal_connect(f.drawable,"update",G_CALLBACK(update),&o);g_assert_true(gimp_image_undo(f.image));g_assert_cmpint(o.calls,>,0);o.expected=2;g_assert_true(gimp_image_redo(f.image));g_signal_handler_disconnect(f.drawable,h);g_assert_cmpint(f.state->finishes,==,2);}
static void update_dispose(){Fixture f;f.publish();Observe o{&f,1,0,true};auto h=g_signal_connect(f.drawable,"update",G_CALLBACK(update),&o);g_assert_true(gimp_image_undo(f.image));g_signal_handler_disconnect(f.drawable,h);g_assert_cmpint(o.calls,>,0);g_assert_cmpint(f.state->finishes,==,1);g_assert_false(f.state->locked);g_assert_null(GIMP_DRAWABLE_UNDO(f.undo)->buffer);g_assert_null(GIMP_ITEM_UNDO(f.undo)->item);g_assert_false(gimp_image_redo(f.image));}
static void clean_dispose(GimpImage*,GimpDirtyMask,gpointer data){auto&f=*static_cast<Fixture*>(data);g_assert_true(f.state->locked);g_assert_cmpint(pixel(f.drawable),==,2);g_assert_cmpint(f.state->live,==,2);gimp_undo_free(f.undo,GIMP_UNDO_MODE_UNDO);g_object_run_dispose(G_OBJECT(f.undo));g_assert_nonnull(GIMP_DRAWABLE_UNDO(f.undo)->buffer);}
static void dirty_dispose(){Fixture f;f.publish();auto h=g_signal_connect(f.image,"clean",G_CALLBACK(clean_dispose),&f);g_assert_true(gimp_image_undo(f.image));g_signal_handler_disconnect(f.image,h);g_assert_cmpint(pixel(f.drawable),==,1);g_assert_cmpint(f.state->live,==,1);g_assert_cmpint(f.state->finishes,==,1);g_assert_null(GIMP_DRAWABLE_UNDO(f.undo)->buffer);}
static void unpublished_lifetime(){Fixture f;f.create();gpointer u=f.undo;g_object_add_weak_pointer(G_OBJECT(u),&u);std::weak_ptr<State>state=f.state;f.state.reset();g_object_unref(f.undo);f.undo=nullptr;g_assert_null(u);g_assert_true(state.expired());g_assert_cmpuint(G_OBJECT(f.before)->ref_count,==,1);}
static void typed_property(){Fixture f;f.create();auto*p=g_object_class_find_property(G_OBJECT_GET_CLASS(f.undo),"stroke");g_assert_nonnull(p);g_assert_true(G_IS_PARAM_SPEC_OBJECT(p));g_assert_true(g_type_is_a(G_PARAM_SPEC_VALUE_TYPE(p),G_TYPE_OBJECT));g_assert_cmpstr(g_type_name(G_PARAM_SPEC_VALUE_TYPE(p)),==,"GimpPainterStrokePayload");}
static void expired_owner(){Fixture f;f.state->expired=true;f.publish();g_assert_true(gimp_image_undo(f.image));g_assert_cmpint(pixel(f.drawable),==,1);g_assert_cmpint(f.state->live,==,2);g_assert_cmpint(f.state->finishes,==,1);}
static void invalid_factory(){
 Fixture f;f.publish();g_assert_true(gimp_image_undo(f.image));auto*redo=gimp_undo_stack_peek(gimp_image_get_redo_stack(f.image));GError*e=nullptr;
 auto*u=create_stroke_undo(f.image,f.drawable,f.before,0,0,"Invalid",nullptr,&e);g_assert_null(u);g_assert_nonnull(e);g_clear_error(&e);
 auto*live=gimp_drawable_get_buffer(f.drawable);auto*alias=gegl_buffer_create_sub_buffer(live,GEGL_RECTANGLE(0,0,8,8));g_assert_true(gegl_buffer_share_storage(alias,live));
 for(auto*b:{live,alias}){u=create_stroke_undo(f.image,f.drawable,b,0,0,"alias",f.state,&e);g_assert_null(u);g_assert_nonnull(e);g_clear_error(&e);}g_object_unref(alias);
 auto*shifted=gegl_buffer_new(GEGL_RECTANGLE(1,0,8,8),gimp_drawable_get_format(f.drawable));u=create_stroke_undo(f.image,f.drawable,shifted,0,0,"nonzero origin",f.state,&e);g_assert_null(u);g_assert_nonnull(e);g_clear_error(&e);g_object_unref(shifted);
 g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(f.image)),==,0);g_assert_true(redo==gimp_undo_stack_peek(gimp_image_get_redo_stack(f.image)));g_assert_cmpint(pixel(f.drawable),==,1);g_assert_true(gimp_image_redo(f.image));g_assert_cmpint(pixel(f.drawable),==,2);
}
static void prepare_dispose(){Fixture f;f.publish();f.state->entry=f.undo;f.state->close_prepare=true;auto*top=gimp_undo_stack_peek(gimp_image_get_undo_stack(f.image));g_assert_false(gimp_image_undo(f.image));g_assert_true(top==gimp_undo_stack_peek(gimp_image_get_undo_stack(f.image)));g_assert_cmpint(f.state->applies,==,0);g_assert_cmpint(f.state->finishes,==,1);g_assert_false(f.state->locked);g_assert_null(GIMP_DRAWABLE_UNDO(f.undo)->buffer);g_assert_cmpint(pixel(f.drawable),==,2);}
static void finish_reentry(){Fixture f;f.publish();f.state->entry=f.undo;f.state->reenter_finish=true;g_assert_true(gimp_image_undo(f.image));g_assert_cmpint(f.state->finishes,==,1);g_assert_false(f.state->locked);g_assert_cmpint(pixel(f.drawable),==,1);g_assert_null(GIMP_DRAWABLE_UNDO(f.undo)->buffer);}
static void guard_once(){Fixture f;f.create();g_assert_true(gimp_undo_pop_guard_prepare(f.undo,GIMP_UNDO_MODE_UNDO));g_assert_false(gimp_undo_pop_guard_prepare(f.undo,GIMP_UNDO_MODE_UNDO));gimp_undo_free(f.undo,GIMP_UNDO_MODE_UNDO);g_assert_nonnull(GIMP_DRAWABLE_UNDO(f.undo)->buffer);gimp_undo_pop_guard_finish(f.undo,GIMP_UNDO_MODE_UNDO);gimp_undo_pop_guard_finish(f.undo,GIMP_UNDO_MODE_UNDO);g_assert_cmpint(f.state->prepares,==,1);g_assert_cmpint(f.state->finishes,==,1);g_assert_null(GIMP_DRAWABLE_UNDO(f.undo)->buffer);}
struct ConstructorToggle {Fixture*f;bool fired=false;};
static void constructor_toggle(gpointer data,GObject*,gboolean last){auto&t=*static_cast<ConstructorToggle*>(data);if(last||t.fired)return;t.fired=true;gimp_image_remove_layer(t.f->image,t.f->layer,FALSE,nullptr);}
static void factory_reentry(){Fixture f;g_object_ref(f.layer);ConstructorToggle toggle{&f,false};g_object_add_toggle_ref(G_OBJECT(f.image),constructor_toggle,&toggle);g_object_unref(f.image);GError*e=nullptr;auto*u=create_stroke_undo(f.image,f.drawable,f.before,0,0,"reentry",f.state,&e);g_object_ref(f.image);g_object_remove_toggle_ref(G_OBJECT(f.image),constructor_toggle,&toggle);g_assert_true(toggle.fired);g_assert_null(u);g_assert_nonnull(e);g_clear_error(&e);g_assert_true(gimp_image_add_layer(f.image,f.layer,nullptr,0,FALSE));g_object_unref(f.layer);g_assert_cmpint(f.state.use_count(),==,1);}
struct BufferObserve {Fixture*f;unsigned calls=0;const char*name;bool dispose=false;};
static void buffer_changed(GeglBuffer*,const GeglRectangle*,gpointer data){auto&o=*static_cast<BufferObserve*>(data);++o.calls;g_test_message("%s changed: historical-state=%d live-pixel=%d guarded=%d",o.name,o.f->state->live,pixel(o.f->drawable),o.f->state->locked);g_assert_cmpint(o.f->state->live,==,pixel(o.f->drawable));all_pixels(gimp_drawable_get_buffer(o.f->drawable),o.f->state->live);g_assert_true(o.f->state->locked);g_assert_false(gimp_image_undo(o.f->image));g_assert_false(gimp_image_redo(o.f->image));if(o.dispose){gimp_undo_free(o.f->undo,GIMP_UNDO_MODE_UNDO);g_object_run_dispose(G_OBJECT(o.f->undo));g_assert_true(o.f->state->locked);}}
static void buffer_observer_coherence(){Fixture f(257,193);f.publish();projection_pixels(f,2);Observe update_oracle{&f,1,0,false,true};auto u=g_signal_connect(f.drawable,"update",G_CALLBACK(update),&update_oracle);BufferObserve payload{&f,0,"payload"},live{&f,0,"live"};auto*target=gimp_drawable_get_buffer(f.drawable);auto p=gegl_buffer_signal_connect(f.before,"changed",G_CALLBACK(buffer_changed),&payload);auto l=gegl_buffer_signal_connect(target,"changed",G_CALLBACK(buffer_changed),&live);g_assert_true(gimp_image_undo(f.image));g_test_message("Undo changed counts payload=%u live=%u",payload.calls,live.calls);g_assert_cmpuint(payload.calls,>,0);g_assert_cmpuint(live.calls,>,0);update_oracle.expected=2;g_assert_true(gimp_image_redo(f.image));g_signal_handler_disconnect(f.drawable,u);g_assert_cmpint(update_oracle.calls,>=,2);g_signal_handler_disconnect(f.before,p);g_signal_handler_disconnect(target,l);g_assert_cmpint(f.state->finishes,==,2);}
static void buffer_observer_dispose(){Fixture f;f.publish();BufferObserve payload{&f,0,"payload-dispose",true},live{&f,0,"live-dispose",true};auto*target=gimp_drawable_get_buffer(f.drawable);auto p=gegl_buffer_signal_connect(f.before,"changed",G_CALLBACK(buffer_changed),&payload);auto l=gegl_buffer_signal_connect(target,"changed",G_CALLBACK(buffer_changed),&live);g_assert_true(gimp_image_undo(f.image));g_assert_cmpuint(payload.calls,>,0);g_assert_cmpuint(live.calls,>,0);g_signal_handler_disconnect(f.before,p);g_signal_handler_disconnect(target,l);g_assert_cmpint(f.state->finishes,==,1);g_assert_false(f.state->locked);g_assert_null(GIMP_DRAWABLE_UNDO(f.undo)->buffer);}
struct NestedNotification {Fixture*f;unsigned calls=0;};
static void nested_notification(GeglBuffer*,const GeglRectangle*r,gpointer data){auto&o=*static_cast<NestedNotification*>(data);++o.calls;GeglRectangle needed={0,0,8,8};g_assert_true(gegl_rectangle_contains(r,&needed));g_assert_false(o.f->state->locked);all_pixels(gimp_drawable_get_buffer(o.f->drawable),o.f->state->live);}
static void nested_buffer_freeze(){Fixture f;f.publish();NestedNotification o{&f,0};auto h=gegl_buffer_signal_connect(f.before,"changed",G_CALLBACK(nested_notification),&o);gegl_buffer_freeze_changed(f.before);g_assert_true(gimp_image_undo(f.image));g_assert_true(gimp_image_redo(f.image));g_assert_cmpuint(o.calls,==,0);g_assert_cmpint(f.state->finishes,==,2);gegl_buffer_thaw_changed(f.before);g_assert_cmpuint(o.calls,==,1);g_signal_handler_disconnect(f.before,h);}
} // namespace
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");app=gimp_init_for_testing();g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);g_test_add_func("/stroke-undo/roundtrip",roundtrip);g_test_add_func("/stroke-undo/active-rejection",rejection);g_test_add_func("/stroke-undo/observer-coherence",observer_coherence);g_test_add_func("/stroke-undo/update-dispose",update_dispose);g_test_add_func("/stroke-undo/dirty-dispose",dirty_dispose);g_test_add_func("/stroke-undo/unpublished-lifetime",unpublished_lifetime);g_test_add_func("/stroke-undo/typed-property",typed_property);g_test_add_func("/stroke-undo/expired-owner",expired_owner);g_test_add_func("/stroke-undo/invalid-factory",invalid_factory);g_test_add_func("/stroke-undo/prepare-dispose",prepare_dispose);g_test_add_func("/stroke-undo/finish-reentry",finish_reentry);g_test_add_func("/stroke-undo/guard-once",guard_once);g_test_add_func("/stroke-undo/factory-reentry",factory_reentry);g_test_add_func("/stroke-undo/buffer-observer-coherence",buffer_observer_coherence);g_test_add_func("/stroke-undo/buffer-observer-dispose",buffer_observer_dispose);g_test_add_func("/stroke-undo/nested-buffer-freeze",nested_buffer_freeze);int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);return result;}
