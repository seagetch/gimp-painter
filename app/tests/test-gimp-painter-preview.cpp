/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <vector>
#include <cstring>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpbrushpipe.h"
#include "core/gimppattern.h"
#include "core/gimptempbuf.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
using namespace GimpPainter::MyPaint;
static Gimp*gimp;
static GimpBrushPipe*pipe_new(PipeSelectModes mode)
{
  auto*pipe=GIMP_BRUSH_PIPE(g_object_new(GIMP_TYPE_BRUSH_PIPE,"name","Preview pipe",nullptr));
  pipe->n_brushes=4;pipe->brushes=g_new0(GimpBrush*,4);
  for(int i=0;i<4;++i){auto*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","Preview cell",nullptr));b->priv->mask=gimp_temp_buf_new(3,3,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(b->priv->mask),40+70*i,9);pipe->brushes[i]=b;}
  g_assert_true(gimp_brush_pipe_set_params(pipe,nullptr));pipe->select[0]=mode;pipe->index[0]=2;pipe->current=pipe->brushes[2];
  GIMP_BRUSH(pipe)->priv->mask=pipe->brushes[0]->priv->mask;return pipe;
}
static Resource settings(bool paper=false)
{Resource r;r.set_switch(BRUSH_USE_GIMP_BRUSHMARK,true);r.set_switch(BRUSH_USE_GIMP_TEXTURE,paper);return r;}
static std::vector<guchar>render(GimpResources&resources,int count=12)
{
  auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,96,16),babl_format("R'G'B'A u8"));GeglSurface surface(buffer);resources.attach(surface);surface.begin_session();
  for(int i=0;i<count;++i){GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=4+8*i;c.y=8;c.pressure=double(i%4)/3;c.velocity=double(i%4)/4;c.direction=double(i%4)/4;c.xtilt=-1+double(i%4)/2;c.ytilt=1-double(i%4)/2;resources.set_coords(c);surface.draw_dab(c.x,c.y,1.5,1,0,0,1,1);}
  surface.end_session();std::vector<guchar>pixels(96*16*4);gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,96,16),1,babl_format("R'G'B'A u8"),pixels.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_object_unref(buffer);return pixels;
}
static void pipe_modes_private_deterministic()
{
  for(auto mode:{PIPE_SELECT_CONSTANT,PIPE_SELECT_INCREMENTAL,PIPE_SELECT_ANGULAR,PIPE_SELECT_VELOCITY,PIPE_SELECT_RANDOM,PIPE_SELECT_PRESSURE,PIPE_SELECT_TILT_X,PIPE_SELECT_TILT_Y}) {
    auto*pipe=pipe_new(mode);auto*context=gimp_context_new(gimp,"preview modes",nullptr);gimp_context_set_brush(context,GIMP_BRUSH(pipe));
    auto*reference_pipe=pipe_new(mode);reference_pipe->index[0]=0;reference_pipe->current=reference_pipe->brushes[0];
    auto*reference_context=gimp_context_new(gimp,"native mode reference",nullptr);gimp_context_set_brush(reference_context,GIMP_BRUSH(reference_pipe));g_random_set_seed(12345);
    std::vector<guchar>native_pixels;{GimpResources native(reference_context,settings());native_pixels=render(native);}
    g_object_unref(reference_context);g_object_unref(reference_pipe);
    g_random_set_seed(99871);auto*expected=g_rand_new_with_seed(99871);
    std::vector<guchar>first;
    {GimpResources preview(context,settings(),GimpResources::Purpose::Preview);first=render(preview);g_assert_true(first==native_pixels);}
    g_assert_cmpint(pipe->index[0],==,2);g_assert_true(pipe->current==pipe->brushes[2]);g_assert_cmpint(GIMP_BRUSH(pipe)->priv->use_count,==,0);
    for(int i=0;i<4;++i)g_assert_cmpint(pipe->brushes[i]->priv->use_count,==,0);
    // Painting the original pipe changes its state, but a new preview starts
    // from canonical private indices and its own random stream.
    pipe->index[0]=1;pipe->current=pipe->brushes[1];
    {GimpResources preview(context,settings(),GimpResources::Purpose::Preview);g_assert_true(render(preview)==first);}
    g_assert_cmpint(pipe->index[0],==,1);g_assert_true(pipe->current==pipe->brushes[1]);
    for(int i=0;i<8;++i)g_assert_cmpuint(g_random_int(),==,g_rand_int(expected));
    g_rand_free(expected);
    bool painted=false;for(std::size_t i=3;i<first.size();i+=4)painted|=first[i]>0;g_assert_true(painted);
    g_object_unref(context);g_object_unref(pipe);
  }
}
static void mark_gone(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void snapshot_child_and_paper()
{
  auto*pipe=pipe_new(PIPE_SELECT_INCREMENTAL);auto*context=gimp_context_new(gimp,"preview snapshot",nullptr);gimp_context_set_brush(context,GIMP_BRUSH(pipe));
  auto*paper=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","Preview paper",nullptr));paper->mask=gimp_temp_buf_new(1,1,babl_format("R'G'B' u8"));std::memset(gimp_temp_buf_get_data(paper->mask),120,3);gimp_context_set_pattern(context,paper);
  GimpResources before(context,settings(true),GimpResources::Purpose::Preview),snapshot(context,settings(true),GimpResources::Purpose::Preview);
  const auto reference=render(before);
  for(int i=0;i<4;++i){std::memset(gimp_temp_buf_get_data(pipe->brushes[i]->priv->mask),0,9);gimp_data_dirty(GIMP_DATA(pipe->brushes[i]));}
  std::memset(gimp_temp_buf_get_data(paper->mask),0,3);gimp_data_dirty(GIMP_DATA(paper));
  g_assert_true(render(snapshot)==reference); // not a lazy copy on first dab
  GimpResources fresh(context,settings(true),GimpResources::Purpose::Preview);g_assert_true(render(fresh)!=reference);
  bool brush_gone=false,paper_gone=false;g_object_weak_ref(G_OBJECT(pipe),mark_gone,&brush_gone);g_object_weak_ref(G_OBJECT(paper),mark_gone,&paper_gone);
  g_object_unref(context);g_object_unref(pipe);g_object_unref(paper);g_assert_true(brush_gone);g_assert_true(paper_gone);
  g_assert_true(render(fresh)!=reference); // adapters keep only their private snapshots
}
static void stroke_selection_remains_native()
{
  auto*pipe=pipe_new(PIPE_SELECT_INCREMENTAL);auto*context=gimp_context_new(gimp,"stroke selector",nullptr);gimp_context_set_brush(context,GIMP_BRUSH(pipe));
  {GimpResources stroke(context,settings());g_assert_cmpint(GIMP_BRUSH(pipe)->priv->use_count,==,1);render(stroke,11);g_assert_cmpint(pipe->index[0],==,1);g_assert_true(pipe->current==pipe->brushes[1]);}
  g_assert_cmpint(GIMP_BRUSH(pipe)->priv->use_count,==,0);g_object_unref(context);g_object_unref(pipe);
}
typedef struct {GimpBrush parent;} PreviewCustomBrush;
typedef struct {GimpBrushClass parent;} PreviewCustomBrushClass;
G_DEFINE_TYPE(PreviewCustomBrush,preview_custom_brush,GIMP_TYPE_BRUSH)
static void preview_custom_brush_class_init(PreviewCustomBrushClass*){}
static void preview_custom_brush_init(PreviewCustomBrush*){}
static void rejects_unisolated_or_invalid()
{
  auto*context=gimp_context_new(gimp,"preview invalid",nullptr);
  auto*custom=GIMP_BRUSH(g_object_new(preview_custom_brush_get_type(),"name","Unknown selector",nullptr));
  gimp_context_set_brush(context,custom);bool rejected=false;
  try{GimpResources preview(context,settings(),GimpResources::Purpose::Preview);}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);g_assert_cmpint(custom->priv->use_count,==,0);g_object_unref(custom);
  auto*pipe=pipe_new(PIPE_SELECT_RANDOM);pipe->rank[0]=0;gimp_context_set_brush(context,GIMP_BRUSH(pipe));rejected=false;
  try{GimpResources preview(context,settings(),GimpResources::Purpose::Preview);}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);g_assert_cmpint(GIMP_BRUSH(pipe)->priv->use_count,==,0);
  pipe->rank[0]=G_MAXINT;pipe->stride[0]=G_MAXINT;rejected=false;
  try{GimpResources preview(context,settings(),GimpResources::Purpose::Preview);}catch(const std::invalid_argument&){rejected=true;}
  g_assert_true(rejected);g_object_unref(pipe);g_object_unref(context);
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-preview/pipe-modes-private",pipe_modes_private_deterministic);
  g_test_add_func("/painter-preview/snapshot-child-paper",snapshot_child_and_paper);
  g_test_add_func("/painter-preview/stroke-native",stroke_selection_remains_native);
  g_test_add_func("/painter-preview/reject-unisolated-invalid",rejects_unisolated_or_invalid);
  const int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
