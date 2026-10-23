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
 void warm(GimpLayer*watched=nullptr){if(!watched)watched=source;gimp_pickable_flush(GIMP_PICKABLE(image));proj=gimp_image_get_projection(image);projection=gimp_pickable_get_buffer(GIMP_PICKABLE(image));std::vector<guchar>all(512*384*4);gegl_buffer_get(projection,GEGL_RECTANGLE(0,0,512,384),1,babl_format("R'G'B'A u8"),all.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);auto*handler=gimp_tile_handler_validate_get_assigned(projection);g_assert_nonnull(handler);trace.graph=handler->graph;g_assert_false(handler->whole_tile);node=gimp_drawable_get_source_node(GIMP_DRAWABLE(watched));node_id=g_signal_connect(node,"invalidated",G_CALLBACK(rect_signal),&trace.source);watched_buffer=GEGL_BUFFER(g_object_ref(gimp_drawable_get_buffer(GIMP_DRAWABLE(watched))));buffer_id=gegl_buffer_signal_connect(watched_buffer,"changed",G_CALLBACK(rect_signal),&trace.buffer);projection_id=g_signal_connect(proj,"update",G_CALLBACK(projection_signal),&trace.projection);active=&trace;}
 void pixel(int x,int y,const char*color){guchar actual[4],expected[4];auto*c=gegl_color_new(color);gegl_color_get_pixel(c,babl_format("R'G'B'A u8"),expected);g_object_unref(c);gegl_buffer_get(projection,GEGL_RECTANGLE(x,y,1,1),1,babl_format("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert_cmpmem(actual,4,expected,4);}
 ~Scene(){active=nullptr;if(node_id)g_signal_handler_disconnect(node,node_id);if(projection_id)g_signal_handler_disconnect(proj,projection_id);if(buffer_id)g_signal_handler_disconnect(watched_buffer,buffer_id);g_clear_object(&watched_buffer);g_object_unref(image);}
};
GeglRectangle quantized(GeglRectangle r){int right=(r.x+r.width+31)/32*32,bottom=(r.y+r.height+31)/32*32;r.x=r.x/32*32;r.y=r.y/32*32;r.width=right-r.x;r.height=bottom-r.y;return r;}
void contains_union(const std::vector<GeglRectangle>&rows,GeglRectangle expected,bool exact){auto*region=cairo_region_create();for(auto r:rows){cairo_rectangle_int_t q={r.x,r.y,r.width,r.height};cairo_region_union_rectangle(region,&q);}cairo_rectangle_int_t e={expected.x,expected.y,expected.width,expected.height};g_assert_cmpint(cairo_region_contains_rectangle(region,&e),==,CAIRO_REGION_OVERLAP_IN);if(exact){auto*want=cairo_region_create_rectangle(&e);g_assert_true(cairo_region_equal(region,want));cairo_region_destroy(want);}cairo_region_destroy(region);}
void ordinary(bool group){Scene s(group);s.warm();GeglRectangle local={70,65,7,9},image={87,88,7,9};fill(s.source,&local,"red");gimp_drawable_update(GIMP_DRAWABLE(s.source),local.x,local.y,local.width,local.height);gimp_pickable_flush(GIMP_PICKABLE(s.image));show("buffer",s.trace.buffer);show("source",s.trace.source);show("projection",s.trace.projection);contains_union(s.trace.buffer,local,true);contains_union(s.trace.source,local,true);contains_union(s.trace.projection,quantized(image),true);s.trace.computed.clear();s.pixel(20,25,"blue");g_assert_true(s.trace.computed.empty());s.pixel(88,89,"red");show("computed",s.trace.computed);g_assert_false(s.trace.computed.empty());auto expected=quantized(image);for(auto r:s.trace.computed)g_assert_true(gegl_rectangle_contains(&expected,&r));s.trace.computed.clear();s.pixel(88,89,"red");g_assert_true(s.trace.computed.empty());}
void ordinary_root(){ordinary(false);}void ordinary_group(){ordinary(true);}
}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");app=gimp_init_for_testing();g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);gboolean enabled=FALSE;g_object_get(app->config,"painter-canvas-ui",&enabled,nullptr);g_assert_true(enabled);g_test_add_func("/graph-invalidation/ordinary-root",ordinary_root);g_test_add_func("/graph-invalidation/ordinary-group",ordinary_group);int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);return result;}
