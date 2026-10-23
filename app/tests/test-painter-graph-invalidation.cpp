/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <gegl-plugin.h>
#include <vector>
#include <mutex>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp-gui.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpdisplayshell-transform.h"
#include "display/gimpdisplayshell-scale.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-new.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpdrawable.h"
#include "core/gimppickable.h"
#include "core/gimpprojection.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "gegl/gimptilehandlervalidate.h"
#include "tests.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
}
namespace {
Gimp*app;
struct Trace {GeglNode*graph=nullptr;std::vector<GeglRectangle>computed,source,buffer,projection;std::mutex mutex;};
Trace*active;
void compute(GeglNode*n,const GeglRectangle*r){if(active&&n==active->graph){std::lock_guard<std::mutex>lock(active->mutex);active->computed.push_back(*r);}}
}
extern "C" void __real_gegl_node_blit(GeglNode*,gdouble,const GeglRectangle*,const Babl*,gpointer,gint,GeglBlitFlags);
extern "C" void __wrap_gegl_node_blit(GeglNode*,gdouble,const GeglRectangle*,const Babl*,gpointer,gint,GeglBlitFlags);
extern "C" void __wrap_gegl_node_blit(GeglNode*n,gdouble scale,const GeglRectangle*r,const Babl*f,gpointer d,gint stride,GeglBlitFlags flags){compute(n,r);__real_gegl_node_blit(n,scale,r,f,d,stride,flags);}
extern "C" void __real_gegl_node_blit_buffer(GeglNode*,GeglBuffer*,const GeglRectangle*,gint,GeglAbyssPolicy);
extern "C" void __wrap_gegl_node_blit_buffer(GeglNode*,GeglBuffer*,const GeglRectangle*,gint,GeglAbyssPolicy);
extern "C" void __wrap_gegl_node_blit_buffer(GeglNode*n,GeglBuffer*b,const GeglRectangle*r,gint level,GeglAbyssPolicy abyss){compute(n,r);__real_gegl_node_blit_buffer(n,b,r,level,abyss);}
namespace {
void rect_signal(GObject*,const GeglRectangle*r,gpointer data){static_cast<std::vector<GeglRectangle>*>(data)->push_back(*r);}
void projection_signal(GimpProjection*,gboolean,gint x,gint y,gint w,gint h,gpointer data){static_cast<std::vector<GeglRectangle>*>(data)->push_back({x,y,w,h});}
void show(const char*name,const std::vector<GeglRectangle>&rows){for(auto r:rows)g_test_message("%s %d,%d %dx%d",name,r.x,r.y,r.width,r.height);}
void fill(GimpLayer*l,const GeglRectangle*r,const char*color){auto*c=gegl_color_new(color);gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(l)),r,c);g_object_unref(c);}
GimpLayer*layer(GimpImage*i,GimpLayer*parent,int x,int y,int w=256,int h=192){auto*l=gimp_layer_new(i,w,h,babl_format("R'G'B'A u8"),"ROI source",1,GIMP_LAYER_MODE_NORMAL);gimp_image_add_layer(i,l,parent,0,FALSE);gimp_item_set_offset(GIMP_ITEM(l),x,y);fill(l,nullptr,"blue");return l;}
struct Scene {
 GimpImage*image;GimpLayer*source;GeglBuffer*projection;GimpProjection*proj;Trace trace;gulong node_id=0,buffer_id=0,projection_id=0;GeglNode*node=nullptr;GeglBuffer*watched_buffer=nullptr;
 Scene(bool group=false){image=gimp_image_new(app,512,384,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);GimpLayer*parent=nullptr;if(group){parent=gimp_group_layer_new(image);gimp_image_add_layer(image,parent,nullptr,0,FALSE);}source=layer(image,parent,17,23);}
 void warm(GimpLayer*watched=nullptr){if(!watched)watched=source;gimp_pickable_flush(GIMP_PICKABLE(image));proj=gimp_image_get_projection(image);projection=gimp_pickable_get_buffer(GIMP_PICKABLE(image));std::vector<guchar>all(512*384*4);gegl_buffer_get(projection,GEGL_RECTANGLE(0,0,512,384),1,babl_format("R'G'B'A u8"),all.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);auto*handler=gimp_tile_handler_validate_get_assigned(projection);g_assert_nonnull(handler);trace.graph=handler->graph;g_test_message("projection tile %dx%d",handler->tile_width,handler->tile_height);g_assert_false(handler->whole_tile);node=gimp_drawable_get_source_node(GIMP_DRAWABLE(watched));node_id=g_signal_connect(node,"invalidated",G_CALLBACK(rect_signal),&trace.source);watched_buffer=GEGL_BUFFER(g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(watched))));buffer_id=gegl_buffer_signal_connect(watched_buffer,"changed",G_CALLBACK(rect_signal),&trace.buffer);projection_id=g_signal_connect(proj,"update",G_CALLBACK(projection_signal),&trace.projection);active=&trace;}
 void pixel(int x,int y,const char*color){guchar actual[4],expected[4];auto*c=gegl_color_new(color);gegl_color_get_pixel(c,babl_format("R'G'B'A u8"),expected);g_object_unref(c);gegl_buffer_get(projection,GEGL_RECTANGLE(x,y,1,1),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpmem(actual,4,expected,4);}
 ~Scene(){active=nullptr;if(node_id)g_signal_handler_disconnect(node,node_id);if(projection_id)g_signal_handler_disconnect(proj,projection_id);if(buffer_id)g_signal_handler_disconnect(watched_buffer,buffer_id);g_clear_object(&watched_buffer);g_object_unref(image);}
};
GeglRectangle quantized(GeglRectangle r){int right=(r.x+r.width+31)/32*32,bottom=(r.y+r.height+31)/32*32;r.x=r.x/32*32;r.y=r.y/32*32;r.width=right-r.x;r.height=bottom-r.y;return r;}
void contains_union(const std::vector<GeglRectangle>&rows,GeglRectangle expected,bool exact){auto*region=cairo_region_create();for(auto r:rows){cairo_rectangle_int_t q={r.x,r.y,r.width,r.height};cairo_region_union_rectangle(region,&q);}cairo_rectangle_int_t e={expected.x,expected.y,expected.width,expected.height};g_assert_cmpint(cairo_region_contains_rectangle(region,&e),==,CAIRO_REGION_OVERLAP_IN);if(exact){auto*want=cairo_region_create_rectangle(&e);g_assert_true(cairo_region_equal(region,want));cairo_region_destroy(want);}cairo_region_destroy(region);}
void ordinary(bool group){Scene s(group);s.warm();GeglRectangle local={70,65,7,9},image={87,88,7,9};fill(s.source,&local,"red");gimp_drawable_update(GIMP_DRAWABLE(s.source),local.x,local.y,local.width,local.height);gimp_pickable_flush(GIMP_PICKABLE(s.image));show("buffer",s.trace.buffer);show("source",s.trace.source);show("projection",s.trace.projection);contains_union(s.trace.buffer,local,true);contains_union(s.trace.source,local,true);auto expected=quantized(image);if(group){auto child=quantized(local);child.x+=17;child.y+=23;expected=quantized(child);}contains_union(s.trace.projection,expected,true);s.trace.computed.clear();s.pixel(250,180,"blue");show("outside-computed",s.trace.computed);g_assert_true(s.trace.computed.empty());s.pixel(88,89,"red");show("computed",s.trace.computed);g_assert_false(s.trace.computed.empty());for(auto r:s.trace.computed)g_assert_true(gegl_rectangle_contains(&expected,&r));s.trace.computed.clear();s.pixel(88,89,"red");g_assert_true(s.trace.computed.empty());}
void clone_roi()
{
 Scene s;auto*clone=gimp_clone_layer_new(s.image,s.source,256,192,"ROI clone",1,GIMP_LAYER_MODE_NORMAL);
 g_assert_true(gimp_image_add_layer(s.image,clone,nullptr,0,FALSE));gimp_item_set_offset(GIMP_ITEM(clone),201,101);
 gimp_item_set_visible(GIMP_ITEM(s.source),FALSE,FALSE);s.warm(clone);
 GeglRectangle local={70,65,7,9},image={271,166,7,9};fill(s.source,&local,"red");
 gimp_drawable_update(GIMP_DRAWABLE(s.source),local.x,local.y,local.width,local.height);gimp_pickable_flush(GIMP_PICKABLE(s.image));
 show("clone-buffer",s.trace.buffer);show("clone-source",s.trace.source);show("clone-projection",s.trace.projection);
 contains_union(s.trace.buffer,local,true);contains_union(s.trace.source,local,true);contains_union(s.trace.projection,quantized(image),true);
 s.trace.computed.clear();s.pixel(202,102,"blue");g_assert_true(s.trace.computed.empty());s.pixel(272,167,"red");g_assert_false(s.trace.computed.empty());
 const auto expected=quantized(image);for(auto r:s.trace.computed)g_assert_true(gegl_rectangle_contains(&expected,&r));
 gimp_clone_layer_set_source(GIMP_CLONE_LAYER(clone),nullptr);gimp_pickable_flush(GIMP_PICKABLE(s.image));s.pixel(272,167,"red");
 s.trace.buffer.clear();s.trace.source.clear();s.trace.projection.clear();s.trace.computed.clear();fill(s.source,&local,"green");gimp_drawable_update(GIMP_DRAWABLE(s.source),local.x,local.y,local.width,local.height);gimp_pickable_flush(GIMP_PICKABLE(s.image));s.pixel(272,167,"red");
 g_assert_true(s.trace.buffer.empty());g_assert_true(s.trace.source.empty());g_assert_true(s.trace.projection.empty());g_assert_true(s.trace.computed.empty());
}
void spin(gint ms){auto until=g_get_monotonic_time()+ms*1000;while(g_get_monotonic_time()<until){g_main_context_iteration(nullptr,FALSE);g_usleep(100);}}
void filter_settle(GimpFilterLayer*f){auto until=g_get_monotonic_time()+20*G_TIME_SPAN_SECOND;while(g_get_monotonic_time()<until){auto state=gimp_filter_layer_get_state(f);if(state==GIMP_FILTER_LAYER_CLEAN)return;g_assert_cmpint(state,!=,GIMP_FILTER_LAYER_FAILED);g_main_context_iteration(nullptr,FALSE);g_usleep(100);}g_error("Gaussian did not settle");}
void gaussian_roi()
{
 Scene s;auto*l=gimp_filter_layer_new(s.image,256,192,"ROI Gaussian",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
 auto*args=gimp_value_array_new_from_types(nullptr,G_TYPE_INT,1,G_TYPE_INT,123,G_TYPE_INT,456,G_TYPE_DOUBLE,1.,G_TYPE_DOUBLE,1.,G_TYPE_INT,1,G_TYPE_NONE);
 auto*f=GIMP_FILTER_LAYER(l);g_assert_true(gimp_filter_layer_set_definition(f,"plug-in-gauss",nullptr,args,nullptr));gimp_value_array_unref(args);
 g_assert_true(gimp_image_add_layer(s.image,l,nullptr,0,FALSE));gimp_item_set_offset(GIMP_ITEM(l),17,23);filter_settle(f);s.warm(l);s.pixel(100,100,"blue");
 auto runs=gimp_filter_layer_get_run_count(f);GeglRectangle local={70,65,32,32};fill(s.source,&local,"red");gimp_drawable_update(GIMP_DRAWABLE(s.source),local.x,local.y,local.width,local.height);filter_settle(f);
 g_assert_cmpuint(gimp_filter_layer_get_run_count(f),==,runs+1);gimp_pickable_flush(GIMP_PICKABLE(s.image));show("filter-buffer",s.trace.buffer);show("filter-source",s.trace.source);show("filter-projection",s.trace.projection);s.pixel(100,100,"red");
 // Gaussian is nonlocal; its committed result invalidates the full cache,
 // unlike the exact local copy performed by Clone.
 g_assert_true(s.trace.buffer.empty());g_assert_true(s.watched_buffer!=gimp_drawable_get_buffer(GIMP_DRAWABLE(l)));guchar retained[4];gegl_buffer_get(s.watched_buffer,GEGL_RECTANGLE(83,77,1,1),1,babl_format("R'G'B'A u8"),retained,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);const guchar blue[4]={0,0,255,255};g_assert_cmpmem(retained,4,blue,4);contains_union(s.trace.source,{0,0,256,192},true);contains_union(s.trace.projection,quantized({17,23,256,192}),true);auto expected=quantized({17,23,256,192});for(auto r:s.trace.computed)g_assert_true(gegl_rectangle_contains(&expected,&r));g_assert_cmpuint(gimp_filter_layer_get_run_count(f),==,runs+1);
 auto*upper=layer(s.image,nullptr,400,300,16,16);gimp_pickable_flush(GIMP_PICKABLE(s.image));spin(20);runs=gimp_filter_layer_get_run_count(f);
 fill(upper,nullptr,"green");gimp_drawable_update(GIMP_DRAWABLE(upper),0,0,16,16);spin(100);g_assert_cmpuint(gimp_filter_layer_get_run_count(f),==,runs);
}

void hot_cache()
{
 Scene s;s.warm();s.pixel(88,89,"blue");s.trace.computed.clear();GeglRectangle local={70,65,7,9};fill(s.source,&local,"red");gimp_drawable_update(GIMP_DRAWABLE(s.source),70,65,7,9);gimp_pickable_flush(GIMP_PICKABLE(s.image));s.pixel(88,89,"red");g_assert_false(s.trace.computed.empty());contains_union(s.trace.projection,{64,64,32,64},true);
}
void negative_offset()
{
 Scene s;gimp_item_set_offset(GIMP_ITEM(s.source),-17,-23);s.warm();GeglRectangle local={70,65,7,9};fill(s.source,&local,"red");gimp_drawable_update(GIMP_DRAWABLE(s.source),70,65,7,9);gimp_pickable_flush(GIMP_PICKABLE(s.image));contains_union(s.trace.buffer,local,true);contains_union(s.trace.source,local,true);contains_union(s.trace.projection,{32,32,32,32},true);s.trace.computed.clear();s.pixel(200,150,"blue");g_assert_true(s.trace.computed.empty());s.pixel(54,43,"red");g_assert_false(s.trace.computed.empty());GeglRectangle expected={32,32,32,32};for(auto r:s.trace.computed)g_assert_true(gegl_rectangle_contains(&expected,&r));
}
#include "test-painter-graph-invalidation-ui.inc"
void ordinary_root(){ordinary(false);}void ordinary_group(){ordinary(true);}
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");bool gui=g_getenv("GIMP_GRAPH_UI")!=nullptr;if(gui){if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;gimp_test_utils_setup_menus_path();app=gimp_init_for_gui_testing(TRUE);}else app=gimp_init_for_testing();g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);if(gui){for(int i=0;i<3;i++){auto*path=g_strdup_printf("/graph-invalidation/ui/%d",i);g_test_add_data_func(path,GINT_TO_POINTER(i),graph_visible);g_free(path);}}else {g_test_add_func("/graph-invalidation/ordinary-root",ordinary_root);g_test_add_func("/graph-invalidation/ordinary-group",ordinary_group);g_test_add_func("/graph-invalidation/hot-cache",hot_cache);g_test_add_func("/graph-invalidation/negative-offset",negative_offset);g_test_add_func("/graph-invalidation/clone",clone_roi);g_test_add_func("/graph-invalidation/gaussian",gaussian_roi);}if(gui){g_application_run(app->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(app->app));g_application_quit(G_APPLICATION(app->app));g_clear_object(&app->app);return result;}int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);return result;}
