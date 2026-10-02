/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <vector>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimptempbuf.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-session.hpp"
#include "paint/painter-mypaint-surface/paint-core.hpp"
#include "painter/gimp-painter-binding.h"
using namespace GimpPainter;
static Gimp*gimp;
static GimpPainterMybrushOptions*options_new()
{
  auto*options=GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr));
  g_object_set(options,"dabs-per-second",40.,"radius-logarithmic",1.5,nullptr);return options;
}
static GimpLayer*layer_new(GimpImage**image)
{
  *image=gimp_image_new(gimp,64,48,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto*layer=gimp_layer_new(*image,64,48,babl_format("R'G'B'A u8"),"Session target",1,GIMP_LAYER_MODE_NORMAL);
  g_assert_true(gimp_image_add_layer(*image,layer,nullptr,0,FALSE));
  gegl_buffer_clear(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),nullptr);gimp_image_undo_free(*image);return layer;
}
static std::vector<guchar>pixels(GimpDrawable*drawable)
{
  std::vector<guchar>v(64*48*4);gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,64,48),1,babl_format("R'G'B'A u8"),v.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return v;
}
static void stroke(GimpPainterSession*session,GimpDrawable*drawable)
{
  GError*error=nullptr;GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;c.pressure=0;
  g_assert_true(gimp_painter_session_stroke_to(session,drawable,.01,&c,nullptr,&error));g_assert_no_error(error);
  c.pressure=.85;g_assert_true(gimp_painter_session_stroke_to(session,drawable,.05,&c,nullptr,&error));g_assert_no_error(error);
  for(int i=0;i<8;++i){c.x+=2;c.y+=1;g_assert_true(gimp_painter_session_stroke_to(session,drawable,.01,&c,nullptr,&error));g_assert_no_error(error);}
}
// Constant opacity intentionally has no pressure mapping. Hover must neither
// synthesize a press from GIMP_COORDS_DEFAULT_PRESSURE nor execute a dab at 0.
static void cold_hover_samples_without_writes()
{
  for(bool floating:{false,true})for(double pressure:{0.,1.}) {
    auto*options=options_new();g_object_set(options,"non-incremental",floating,"smudge",.7,"smudge-length",.5,nullptr);
    GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);const auto before=pixels(drawable);
    MyPaint::PaintCore core(GIMP_PAINT_OPTIONS(options),PainterOptionsRef::retain(options).snapshot());GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;c.pressure=pressure;
    for(int i=0;i<8;++i){c.x+=1;core.hover_to(drawable,.05,c);}
    g_assert_cmpuint(core.bytes_read(),>,0);g_assert_cmpuint(core.bytes_written(),==,0);g_assert_true(pixels(drawable)==before);
    g_assert_false(core.active());g_assert_false(gimp_viewable_preview_is_frozen(GIMP_VIEWABLE(layer)));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);
    core.finish();core.hover_to(drawable,.05,c);core.cancel();g_assert_true(pixels(drawable)==before);g_object_unref(options);g_object_unref(image);
  }
}
static void active_hover_preserves_logical_split()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);stroke(session,drawable);
  const auto painted=pixels(drawable);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=32;c.y=24;c.pressure=1;gboolean split=TRUE;GError*error=nullptr;
  // No dab is due at zero elapsed time and unchanged position. The engine
  // treats the sample as still painting, so there is no forced release split.
  g_assert_true(gimp_painter_session_hover_to(session,drawable,0,&c,&split,&error));g_assert_no_error(error);g_assert_false(split);g_assert_true(gimp_painter_session_is_active(session));
  g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,0);g_assert_true(pixels(drawable)==painted);
  c.x+=12;g_assert_true(gimp_painter_session_hover_to(session,drawable,.05,&c,&split,&error));g_assert_no_error(error);g_assert_true(split);
  g_assert_false(gimp_painter_session_is_active(session));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);g_assert_true(pixels(drawable)==painted);
  for(int i=0;i<4;++i){c.x-=1;g_assert_true(gimp_painter_session_hover_to(session,drawable,.1,&c,&split,&error));g_assert_no_error(error);}
  g_assert_false(gimp_painter_session_is_active(session));g_assert_cmpint(gimp_undo_stack_get_depth(gimp_image_get_undo_stack(image)),==,1);g_assert_true(pixels(drawable)==painted);
  g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
static void mark_gone(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void hover_releases_old_image()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;GError*error=nullptr;
  g_assert_true(gimp_painter_session_hover_to(session,GIMP_DRAWABLE(layer),.01,&c,nullptr,&error));g_assert_no_error(error);
  bool gone=false;g_object_weak_ref(G_OBJECT(image),mark_gone,&gone);g_object_unref(image);g_assert_true(gone);
  // A fresh image uses the same surviving session without retaining the old
  // target. Finish and subsequent hover cannot restart the ended transaction.
  layer=layer_new(&image);stroke(session,GIMP_DRAWABLE(layer));g_assert_true(gimp_painter_session_finish(session,&error));g_assert_no_error(error);
  const auto before=pixels(GIMP_DRAWABLE(layer));g_assert_true(gimp_painter_session_hover_to(session,GIMP_DRAWABLE(layer),.1,&c,nullptr,&error));g_assert_no_error(error);
  g_assert_false(gimp_painter_session_is_active(session));g_assert_true(pixels(GIMP_DRAWABLE(layer))==before);gone=false;g_object_weak_ref(G_OBJECT(image),mark_gone,&gone);g_object_unref(image);g_assert_true(gone);
  g_object_unref(session);g_object_unref(options);
}
static void close_rejects_hover()
{
  auto*options=options_new();auto*session=gimp_painter_session_new(options,nullptr);GimpImage*image;auto*layer=layer_new(&image);const auto before=pixels(GIMP_DRAWABLE(layer));GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;GError*error=nullptr;
  g_assert_true(gimp_painter_binding_close(G_OBJECT(session),&error));g_assert_no_error(error);g_assert_false(gimp_painter_session_hover_to(session,GIMP_DRAWABLE(layer),.1,&c,nullptr,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);g_assert_true(pixels(GIMP_DRAWABLE(layer))==before);
  g_object_unref(session);g_object_unref(options);g_object_unref(image);
}
typedef struct _HoverBrush {GimpBrush parent;MyPaint::PaintCore*core;GimpDrawable*drawable;bool fired;} HoverBrush;
typedef struct _HoverBrushClass {GimpBrushClass parent;} HoverBrushClass;
G_DEFINE_TYPE(HoverBrush,hover_brush,GIMP_TYPE_BRUSH)
static void hover_brush_begin(GimpBrush*brush)
{
  auto*parent=GIMP_BRUSH_CLASS(hover_brush_parent_class);if(parent->begin_use)parent->begin_use(brush);
  auto*self=reinterpret_cast<HoverBrush*>(brush);if(!self->core||self->fired)return;self->fired=true;bool rejected=false;
  try{self->core->configure(MyPaint::Resource());}catch(const std::logic_error&){rejected=true;}g_assert_true(rejected);
  rejected=false;GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;try{self->core->stroke_to(self->drawable,.1,c);}catch(const std::logic_error&){rejected=true;}g_assert_true(rejected);
  self->core->cancel();
}
static void hover_brush_class_init(HoverBrushClass*klass){GIMP_BRUSH_CLASS(klass)->begin_use=hover_brush_begin;}
static void hover_brush_init(HoverBrush*self){self->core=nullptr;self->drawable=nullptr;self->fired=false;}
static void resource_setup_reentry()
{
  auto*options=options_new();GimpImage*image;auto*layer=layer_new(&image);auto*drawable=GIMP_DRAWABLE(layer);const auto before=pixels(drawable);
  auto*brush=reinterpret_cast<HoverBrush*>(g_object_new(hover_brush_get_type(),"name","Reentrant hover mask",nullptr));
  GIMP_BRUSH(brush)->priv->mask=gimp_temp_buf_new(3,3,babl_format("Y u8"));gimp_context_set_brush(GIMP_CONTEXT(options),GIMP_BRUSH(brush));g_object_set(options,"use-gimp-brushmark",TRUE,"smudge",.7,nullptr);
  MyPaint::PaintCore core(GIMP_PAINT_OPTIONS(options),PainterOptionsRef::retain(options).snapshot());brush->core=&core;brush->drawable=drawable;GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.y=16;
  g_assert_false(core.hover_to(drawable,.1,c));g_assert_true(brush->fired);g_assert_false(core.active());g_assert_cmpuint(core.bytes_read(),==,0);g_assert_true(pixels(drawable)==before);
  brush->core=nullptr;g_object_unref(brush);g_object_unref(options);g_object_unref(image);
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-hover/cold-no-writes",cold_hover_samples_without_writes);
  g_test_add_func("/painter-hover/logical-idle-split",active_hover_preserves_logical_split);
  g_test_add_func("/painter-hover/release-image",hover_releases_old_image);
  g_test_add_func("/painter-hover/closed",close_rejects_hover);
  g_test_add_func("/painter-hover/resource-reentry",resource_setup_reentry);
  int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
