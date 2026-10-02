/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimppattern.h"
#include "core/gimppaintermybrush.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-settings-data.h"
#include <algorithm>
using namespace GimpPainter;
using MyPaint::Resource;
static Gimp*gimp;
static GimpPainterMybrushOptions*options_new()
{return GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr));}
static std::string name(const char*n){std::string s(n);std::replace(s.begin(),s.end(),'_','-');return s;}
static void generated_properties()
{
  auto*options=options_new();g_assert_false(options->binding_failed);auto*klass=G_OBJECT_GET_CLASS(options);
  for(const auto&s:painter_mypaint_settings){auto*p=g_object_class_find_property(klass,name(s.internal_name).c_str());g_assert_true(G_IS_PARAM_SPEC_DOUBLE(p));g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(p)->minimum,==,s.minimum);g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(p)->maximum,==,s.maximum);}
  for(const auto&s:painter_mypaint_switches)g_assert_true(G_IS_PARAM_SPEC_BOOLEAN(g_object_class_find_property(klass,name(s.internal_name).c_str())));
  for(const auto&s:painter_mypaint_texts)g_assert_true(G_IS_PARAM_SPEC_STRING(g_object_class_find_property(klass,name(s.internal_name).c_str())));
  auto*again=gimp_painter_mybrush_options_ref_for_context(GIMP_CONTEXT(options),nullptr);g_assert_true(again==options);g_object_unref(again);
  GError*error=nullptr;g_assert_null(gimp_painter_mybrush_options_ref_for_context(nullptr,&error));g_assert_nonnull(error);g_clear_error(&error);g_object_unref(options);
}
static void edit_curve_commit_and_conflict()
{
  auto*options=options_new();auto brush=PainterMybrushRef::create("draft-source");
  auto initial=Resource::decode("{\"version\":3,\"settings\":{},\"future_metadata\":{\"keep\":true}}");brush.replace(initial);gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),brush.get());
  g_object_set(options,"stroke-opacity",.37,"non-incremental",TRUE,"brushmark-name","missing-shape-kept",nullptr);
  const GimpVector2 points[]={{0,0},{.5,.7},{1,1}};GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_curve(options,BRUSH_OPAQUE_MULTIPLY,INPUT_PRESSURE,points,3,&error));g_assert_no_error(error);
  auto draft=PainterOptionsRef::retain(options).snapshot();g_assert_true(draft.switch_value(BRUSH_NON_INCREMENTAL));g_assert_cmpfloat_with_epsilon(draft.base_value(BRUSH_STROKE_OPACITY),.37,1e-6);
  g_assert_true(draft.text_value(BRUSH_BRUSHMARK_NAME)=="missing-shape-kept");g_assert_cmpuint(draft.curve(BRUSH_OPAQUE_MULTIPLY,INPUT_PRESSURE).size(),==,3);
  g_assert_true(brush.snapshot().encode()==initial.encode());g_assert_true(gimp_painter_mybrush_options_commit(options,&error));g_assert_no_error(error);
  g_assert_true(brush.snapshot().encode()==draft.encode());gboolean dirty,conflict;g_object_get(options,"painter-dirty",&dirty,"painter-conflict",&conflict,nullptr);g_assert_false(dirty);g_assert_false(conflict);
  g_object_set(options,"stroke-opacity",.6,nullptr);auto retained=PainterOptionsRef::retain(options).snapshot().encode();
  auto external=brush.snapshot();external.set_base_value(BRUSH_STROKE_OPACITY,.8);brush.replace(external);
  g_object_get(options,"painter-conflict",&conflict,nullptr);g_assert_true(conflict);g_assert_false(gimp_painter_mybrush_options_commit(options,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==retained);g_assert_true(brush.snapshot().encode()==external.encode());g_object_unref(options);
}
static void history_and_config_roundtrip()
{
  auto*options=options_new();auto a=PainterMybrushRef::create("history-a"),b=PainterMybrushRef::create("history-b");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),a.get());GError*error=nullptr;
  const char*json="{\"version\":3,\"settings\":{\"opaque\":{\"base_value\":0.7,\"inputs\":{\"pressure\":[[0,0],[1,0.4]]}}},\"switches\":{\"non_incremental\":true},\"texts\":{\"texture_name\":\"absent-paper\"},\"unknown\":{\"remember\":23}}";
  g_assert_true(gimp_painter_mybrush_options_set_json(options,json,&error));g_assert_no_error(error);auto before=PainterOptionsRef::retain(options).snapshot().encode();
  const guint start=gimp_painter_mybrush_options_history_size(options);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),b.get());g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,start+1);
  g_assert_true(gimp_painter_mybrush_options_restore_history(options,start,&error));g_assert_no_error(error);g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==before);
  String config(gimp_config_serialize_to_string(GIMP_CONFIG(options),nullptr));g_assert_nonnull(config.get());auto*copy=options_new();
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(copy),config.get(),-1,nullptr,&error));g_assert_no_error(error);g_assert_true(PainterOptionsRef::retain(copy).snapshot().encode()==before);
  auto*duplicate=GIMP_PAINTER_MYBRUSH_OPTIONS(gimp_config_duplicate(GIMP_CONFIG(options)));g_assert_nonnull(duplicate);g_assert_true(PainterOptionsRef::retain(duplicate).snapshot().encode()==before);
  g_assert_true(gimp_config_copy(GIMP_CONFIG(options),GIMP_CONFIG(copy),GParamFlags(0)));g_assert_true(PainterOptionsRef::retain(copy).snapshot().encode()==before);
  g_object_unref(duplicate);g_object_unref(copy);g_object_unref(options);
}
static void drop_options(GObject*,GParamSpec*,gpointer data){auto**owner=static_cast<GimpPainterMybrushOptions**>(data);g_clear_object(owner);}
static void gone(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void notification_releases_caller()
{
  auto*options=options_new();bool finalized=false;g_object_weak_ref(G_OBJECT(options),gone,&finalized);
  g_signal_connect(options,"notify::opaque",G_CALLBACK(drop_options),&options);GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_json(options,"{\"version\":3,\"settings\":{}}",&error));g_assert_no_error(error);g_assert_null(options);g_assert_true(finalized);
}
static void closed_and_invalid_curves()
{
  auto*options=options_new();const auto before=PainterOptionsRef::retain(options).snapshot().encode();GError*error=nullptr;
  const GimpVector2 invalid[]={{1,0},{0,1}};g_assert_false(gimp_painter_mybrush_options_set_curve(options,BRUSH_OPAQUE,INPUT_PRESSURE,invalid,2,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==before);g_assert_true(gimp_painter_binding_close(G_OBJECT(options),&error));g_assert_no_error(error);
  g_assert_null(gimp_painter_mybrush_options_dup_json(options,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,0);
  g_assert_null(gimp_painter_mybrush_options_history_name(options,0));
  g_assert_false(gimp_painter_mybrush_options_restore_history(options,0,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);g_object_unref(options);
}
static void named_resources()
{
  auto*options=options_new();auto*context=GIMP_CONTEXT(options);
  auto*brush=GIMP_BRUSH(gimp_brush_generated_new("options-shape",GIMP_BRUSH_GENERATED_CIRCLE,5,2,.7,1,0));
  auto*other=GIMP_BRUSH(gimp_brush_generated_new("options-other",GIMP_BRUSH_GENERATED_CIRCLE,3,2,.5,1,0));
  auto*pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","options-paper",nullptr));
  auto*brushes=gimp_data_factory_get_container(gimp->brush_factory);auto*patterns=gimp_data_factory_get_container(gimp->pattern_factory);
  gimp_data_make_internal(GIMP_DATA(brush),"options-shape");gimp_data_make_internal(GIMP_DATA(other),"options-other");gimp_data_make_internal(GIMP_DATA(pattern),"options-paper");
  gimp_container_add(brushes,GIMP_OBJECT(brush));gimp_container_add(brushes,GIMP_OBJECT(other));gimp_container_add(patterns,GIMP_OBJECT(pattern));
  gimp_context_set_brush(context,brush);gimp_context_set_pattern(context,pattern);
  g_object_set(options,"use-gimp-brushmark",TRUE,"brushmark-specified",TRUE,"use-gimp-texture",TRUE,"texture-specified",TRUE,nullptr);
  auto draft=PainterOptionsRef::retain(options).snapshot();g_assert_true(draft.text_value(BRUSH_BRUSHMARK_NAME)=="options-shape");g_assert_true(draft.text_value(BRUSH_TEXTURE_NAME)=="options-paper");
  g_object_set(options,"brushmark-name","options-other",nullptr);g_assert_true(gimp_context_get_brush(context)==other);
  gimp_context_set_brush(context,brush);g_assert_true(PainterOptionsRef::retain(options).snapshot().text_value(BRUSH_BRUSHMARK_NAME)=="options-shape");
  g_object_set(options,"brushmark-name","absent-shape","texture-name","absent-paper",nullptr);
  g_assert_true(gimp_context_get_brush(context)==brush);g_assert_true(gimp_context_get_pattern(context)==pattern);
  draft=PainterOptionsRef::retain(options).snapshot();g_assert_true(draft.text_value(BRUSH_BRUSHMARK_NAME)=="absent-shape");g_assert_true(draft.text_value(BRUSH_TEXTURE_NAME)=="absent-paper");
  g_object_set(options,"brushmark-specified",FALSE,nullptr);gimp_context_set_brush(context,other);g_assert_true(PainterOptionsRef::retain(options).snapshot().text_value(BRUSH_BRUSHMARK_NAME)=="absent-shape");
  g_object_unref(options);gimp_container_remove(brushes,GIMP_OBJECT(brush));gimp_container_remove(brushes,GIMP_OBJECT(other));gimp_container_remove(patterns,GIMP_OBJECT(pattern));
  g_object_unref(brush);g_object_unref(other);g_object_unref(pattern);
}
struct Reenter {GimpPainterMybrushOptions*options;GimpPainterMybrush*next;bool fired=false;};
static void choose_again(gpointer data,GObject*){auto&s=*static_cast<Reenter*>(data);s.fired=true;gimp_context_set_painter_mybrush(GIMP_CONTEXT(s.options),s.next);}
static void selection_reentry()
{
  auto*options=options_new();auto first=PainterMybrushRef::create("first"),last=PainterMybrushRef::create("last");
  auto*old=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_new(nullptr,"old"));
  auto value=last.snapshot();value.set_base_value(BRUSH_STROKE_OPACITY,.234);last.replace(value);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),old);Reenter state{options,last.get()};g_object_weak_ref(G_OBJECT(old),choose_again,&state);g_object_unref(old);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),first.get());g_assert_true(state.fired);g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==last.get());
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==value.encode());g_object_unref(options);
}
static void close_during_commit(GimpPainterMybrush*,gpointer data){gimp_painter_binding_close(G_OBJECT(data),nullptr);}
static void commit_closes_options()
{
  auto*options=options_new();auto source=PainterMybrushRef::create("close-commit");gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),source.get());g_object_set(options,"opaque",.42,nullptr);
  const auto expected=PainterOptionsRef::retain(options).snapshot().encode();auto id=g_signal_connect(source.get(),"settings-changed",G_CALLBACK(close_during_commit),options);GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_commit(options,&error));g_assert_no_error(error);g_assert_true(source.snapshot().encode()==expected);g_signal_handler_disconnect(source.get(),id);g_object_unref(options);
}
static void shared_application_history()
{
  auto*first=options_new();auto*second=options_new();
  auto a=PainterMybrushRef::create("cross-options-source"),b=PainterMybrushRef::create("cross-options-next");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),a.get());
  const auto draft=Resource::decode("{\"version\":3,\"unknown\":{\"cross_options\":true},\"settings\":{\"texture_grain\":{\"base_value\":0,\"inputs\":{\"custom\":[[-1,0],[0,0.5],[1,1]]}}},\"texts\":{\"texture_name\":\"missing cross-options paper\"}}");
  g_assert_true(gimp_painter_mybrush_options_set_json(first,draft.encode().c_str(),nullptr));
  const auto before=gimp_painter_mybrush_options_history_size(second);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),b.get());
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(second),==,before+1);
  guint notifications=0;
  g_signal_connect(second,"history-changed",G_CALLBACK(+[](GimpPainterMybrushOptions*,gpointer p){++*static_cast<guint*>(p);}),&notifications);
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  g_assert_cmpuint(notifications,>,0);
  g_object_unref(first); // history is application-owned, not first-view-owned
  GError*error=nullptr;g_assert_true(gimp_painter_mybrush_options_restore_history(second,before,&error));g_assert_no_error(error);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(second))==a.get());
  g_assert_true(PainterOptionsRef::retain(second).snapshot().encode()==draft.encode());
  auto*ordinary=gimp_context_new(gimp,"cross-options-context",nullptr);
  gimp_context_set_painter_mybrush(ordinary,b.get());
  auto*third=options_new();gimp_context_set_parent(GIMP_CONTEXT(third),ordinary);
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(third),==,gimp_painter_mybrush_options_history_size(second));
  g_assert_true(gimp_painter_mybrush_options_restore_history(third,before,&error));g_assert_no_error(error);
  g_assert_true(PainterOptionsRef::retain(third).snapshot().encode()==draft.encode());
  g_object_unref(third);g_object_unref(ordinary);g_object_unref(second);
}
static void shared_history_application_close()
{
  auto*first=options_new();auto*second=options_new();
  GObject*weak=nullptr;
  {
    auto source=PainterMybrushRef::create("history-close-owned");weak=G_OBJECT(source.get());
    g_object_add_weak_pointer(weak,reinterpret_cast<gpointer*>(&weak));
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),source.get());g_object_set(first,"opaque",.147,nullptr);
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(GIMP_CONTEXT(first))));
  }
  g_object_unref(first);g_assert_nonnull(weak);
  const auto count=gimp_painter_mybrush_options_history_size(second);g_assert_cmpuint(count,>,0);
  g_assert_true(gimp_painter_binding_close(G_OBJECT(gimp),nullptr));
  g_assert_null(weak);g_assert_cmpuint(gimp_painter_mybrush_options_history_size(second),==,0);
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  g_object_unref(second);g_assert_true(gimp_painter_binding_close(G_OBJECT(gimp),nullptr));
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-options/generated-properties",generated_properties);
  g_test_add_func("/painter-options/edit-curve-commit-conflict",edit_curve_commit_and_conflict);
  g_test_add_func("/painter-options/history-config",history_and_config_roundtrip);
  g_test_add_func("/painter-options/notification-last-ref",notification_releases_caller);
  g_test_add_func("/painter-options/closed-invalid-curve",closed_and_invalid_curves);
  g_test_add_func("/painter-options/named-resources",named_resources);
  g_test_add_func("/painter-options/selection-reentry",selection_reentry);
  g_test_add_func("/painter-options/commit-close",commit_closes_options);
  g_test_add_func("/painter-options/cross-options-history",shared_application_history);
  g_test_add_func("/painter-options/application-history-close",shared_history_application_close);
  int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
