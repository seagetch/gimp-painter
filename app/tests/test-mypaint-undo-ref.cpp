/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
extern "C" {
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage-new.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "paint/paint-types.h"
#include "widgets/widgets-types.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/gimpmypaintcoreundo.hpp"
#include "paint/gimpmypaintcoreundo.hpp"
using namespace GimpPainter;
static Gimp *app;
struct GIMP_PAINTER_PRIVATE State:MyPaint::StrokeState {
 bool prepare(GimpUndoMode)noexcept override{return true;}
 void apply(GimpUndoMode)noexcept override{}
 void finish(GimpUndoMode)noexcept override{}
 std::size_t memsize()const noexcept override{return sizeof(*this);}
};
static void empty_type(){MypaintCoreUndoRef p;g_assert_false(bool(p));p.reset();g_assert_null(p.release());g_assert_false(bool(MypaintCoreUndoRef::retain(nullptr)));g_assert_false(bool(MypaintCoreUndoRef::adopt(nullptr)));g_assert_true(TypeTraits<GimpMypaintCoreUndo>::type()==gimp_mypaint_core_undo_get_type());g_assert_true(g_type_is_a(TypeTraits<GimpMypaintCoreUndo>::type(),GIMP_TYPE_DRAWABLE_UNDO));}
static void wrong_type(){auto*p=g_object_new(G_TYPE_OBJECT,nullptr);for(bool adopt:{false,true}){bool rejected=false;try{auto h=adopt?MypaintCoreUndoRef::adopt(reinterpret_cast<GimpMypaintCoreUndo*>(p)):MypaintCoreUndoRef::retain(reinterpret_cast<GimpMypaintCoreUndo*>(p));}catch(const Error&e){rejected=e.code()==GIMP_PAINTER_ERROR_WRONG_TYPE;}g_assert_true(rejected);g_assert_cmpuint(G_OBJECT(p)->ref_count,==,1);}g_object_unref(p);}
static void ownership(){auto*image=gimp_image_new(app,8,8,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);auto*layer=gimp_layer_new(image,8,8,babl_format("R'G'B'A u8"),"ref",1,GIMP_LAYER_MODE_NORMAL);g_assert_true(gimp_image_add_layer(image,layer,nullptr,0,FALSE));auto*buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,8,8),babl_format("R'G'B'A u8"));auto state=std::make_shared<State>();std::weak_ptr<State>weak=state;GError*error=nullptr;auto*raw=MyPaint::create_stroke_undo(image,GIMP_DRAWABLE(layer),buffer,0,0,"ownership",state,&error);g_assert_no_error(error);g_assert_nonnull(raw);gpointer witness=raw;g_object_add_weak_pointer(G_OBJECT(raw),&witness);auto a=MypaintCoreUndoRef::adopt(GIMP_MYPAINT_CORE_UNDO(raw));state.reset();g_assert_false(weak.expired());{auto b=MypaintCoreUndoRef::retain(a.get());auto c=b;auto d=std::move(c);g_assert_false(bool(c));a.reset();b.reset();g_assert_nonnull(witness);auto*r=d.release();g_assert_false(bool(d));auto e=MypaintCoreUndoRef::adopt(r);e=e;g_assert_true(e.get()==r);auto f=std::move(e);g_assert_false(bool(e));g_assert_nonnull(witness);}g_assert_null(witness);g_assert_true(weak.expired());g_object_unref(buffer);g_object_unref(image);}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");app=gimp_init_for_testing();g_object_set(app->config,"painter-canvas-ui",TRUE,nullptr);g_test_add_func("/mypaint-undo-ref/empty-type",empty_type);g_test_add_func("/mypaint-undo-ref/wrong-type",wrong_type);g_test_add_func("/mypaint-undo-ref/ownership",ownership);int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(app,TRUE);return result;}
