/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included once by the C++ selector suite, outside any namespace or extern C.
 * These cases exercise only the public immutable history-preview query. */
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimppaintermybrush.h"
}
#include "core/gimppaintermybrush-handle.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
#include <memory>
#include <string>
#include <vector>

namespace SelectorHistoryTest {
using namespace GimpPainter;
using MyPaint::Resource;
static Gimp *application;

struct PixbufUnref {
  void operator()(GdkPixbuf *image) const noexcept {if(image)g_object_unref(image);}
};
using Pixbuf=std::unique_ptr<GdkPixbuf,PixbufUnref>;

static ObjectRef<GimpPainterMybrushOptions> options_new(Gimp *app=application)
{
  // Filtered GUI cases must finish deferred device setup before normal exit.
  gimp_set_focused_once(app);
  auto options=ObjectRef<GimpPainterMybrushOptions>::adopt(
    GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",app,nullptr)));
  g_assert_nonnull(options.get());
  g_assert_false(options.get()->binding_failed);
  return options;
}

static Pixbuf pixels(guint32 rgba)
{
  auto result=Pixbuf(gdk_pixbuf_new(GDK_COLORSPACE_RGB,TRUE,8,3,5));
  g_assert_nonnull(result.get());
  gdk_pixbuf_fill(result.get(),rgba);
  return result;
}

static Resource preview_resource(guint32 rgba)
{
  auto icon=pixels(rgba);
  gchar *png=nullptr;gsize length=0;GError *error=nullptr;
  g_assert_true(gdk_pixbuf_save_to_buffer(icon.get(),&png,&length,"png",&error,nullptr));
  g_assert_no_error(error);
  String owned_png(png),encoded(g_base64_encode(reinterpret_cast<const guchar*>(png),length));
  Resource resource;
  resource.set_preview_png_base64(encoded.get());
  return resource;
}

static guint append(GimpPainterMybrushOptions *options,const Resource& resource,
                    const char *name="Saved selector history")
{
  auto source=PainterMybrushRef::create("selector-history-source");
  auto next=PainterMybrushRef::create("selector-history-next");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),source.get());
  GError *error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_json(options,resource.encode().c_str(),&error));
  g_assert_no_error(error);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,name,&error));
  g_assert_no_error(error);
  const guint index=gimp_painter_mybrush_options_history_size(options);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),next.get());
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,index+1);
  return index;
}

static Pixbuf query(GimpPainterMybrushOptions *options,guint index)
{
  GError *error=nullptr;
  auto result=Pixbuf(gimp_painter_mybrush_options_history_preview(options,index,&error));
  g_assert_no_error(error);g_assert_nonnull(result.get());
  return result;
}

static void assert_pixels(GdkPixbuf *image,guint32 rgba)
{
  g_assert_nonnull(image);
  g_assert_cmpint(gdk_pixbuf_get_width(image),==,3);
  g_assert_cmpint(gdk_pixbuf_get_height(image),==,5);
  g_assert_cmpint(gdk_pixbuf_get_n_channels(image),==,4);
  g_assert_cmpint(gdk_pixbuf_get_bits_per_sample(image),==,8);
  g_assert_true(gdk_pixbuf_get_has_alpha(image));
  for(int y=0;y<5;++y)for(int x=0;x<3;++x) {
    const auto *pixel=gdk_pixbuf_get_pixels(image)+y*gdk_pixbuf_get_rowstride(image)+x*4;
    for(int channel=0;channel<4;++channel)
      g_assert_cmpuint(pixel[channel],==,(rgba>>(24-channel*8))&0xff);
  }
}

static void empty_and_invalid()
{
  auto options=options_new();
  const guint before=gimp_painter_mybrush_options_history_size(options.get());
  GError *error=nullptr;
  g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),before,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_EXCEPTION);g_clear_error(&error);
  g_assert_null(gimp_painter_mybrush_options_history_preview(nullptr,0,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_WRONG_TYPE);g_clear_error(&error);
  auto wrong=ObjectRef<GObject>::adopt(G_OBJECT(g_object_new(G_TYPE_OBJECT,nullptr)));
  g_assert_null(BindingStore::find(wrong.get()));
  g_assert_null(gimp_painter_mybrush_options_history_preview(
    reinterpret_cast<GimpPainterMybrushOptions*>(wrong.get()),0,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_WRONG_TYPE);g_clear_error(&error);
  g_assert_null(BindingStore::find(wrong.get()));
  const guint index=append(options.get(),Resource());
  g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),index,&error));
  g_assert_no_error(error);
  g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),index,nullptr));
  for(guint invalid:{index+1,G_MAXUINT}) {
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),invalid,&error));
    g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_EXCEPTION);g_clear_error(&error);
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),invalid,nullptr));
  }
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options.get()),==,before+1);
}

static void malformed()
{
  auto options=options_new();
  const auto valid=preview_resource(0x359b74ff);
  std::vector<Resource> resources;
  // A JSON value of the wrong type is retained by the settings parser; the
  // preview query must contain its validation error without corrupting history.
  resources.push_back(Resource::decode("{\"version\":3,\"painter_preview_png\":17}"));
  for(const std::string& encoded:{std::string("%%%"),std::string("   "),
      std::string("bm90IGEgcGljdHVyZQ=="),std::string("iVBORw0KGgo="),
      valid.preview_png_base64()+"!",valid.preview_png_base64().substr(1)}) {
    Resource value;value.set_preview_png_base64(encoded);resources.push_back(std::move(value));
  }
  for(const auto& resource:resources) {
    const guint index=append(options.get(),resource,"Malformed saved preview");
    const auto before=PainterOptionsRef::retain(options.get()).snapshot().encode();
    const guint count=gimp_painter_mybrush_options_history_size(options.get());
    GError *error=nullptr;
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),index,&error));
    g_assert_nonnull(error);g_clear_error(&error);
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),index,nullptr));
    g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options.get()),==,count);
    g_assert_true(PainterOptionsRef::retain(options.get()).snapshot().encode()==before);
    String name(gimp_painter_mybrush_options_history_name(options.get(),index));
    g_assert_cmpstr(name.get(),==,"Malformed saved preview");
  }
  auto whitespace=valid;
  whitespace.set_preview_png_base64("\n "+valid.preview_png_base64()+"\t\r\n");
  auto image=query(options.get(),append(options.get(),whitespace));
  assert_pixels(image.get(),0x359b74ff);
}

static void independent_owned_pixels()
{
  auto options=options_new();
  const guint index=append(options.get(),preview_resource(0x8357ab9d));
  auto first=query(options.get(),index),second=query(options.get(),index);
  g_assert_true(first.get()!=second.get());
  g_assert_true(gdk_pixbuf_get_pixels(first.get())!=gdk_pixbuf_get_pixels(second.get()));
  assert_pixels(first.get(),0x8357ab9d);assert_pixels(second.get(),0x8357ab9d);
  gdk_pixbuf_fill(first.get(),0x01020304);
  assert_pixels(second.get(),0x8357ab9d);
  auto third=query(options.get(),index);assert_pixels(third.get(),0x8357ab9d);
  bool finalized=false;
  g_object_weak_ref(G_OBJECT(first.get()),+[](gpointer data,GObject*){*static_cast<bool*>(data)=true;},&finalized);
  first.reset();g_assert_true(finalized);
  options.reset();
  assert_pixels(second.get(),0x8357ab9d);assert_pixels(third.get(),0x8357ab9d);
}

static void source_mutation()
{
  auto options=options_new();
  auto source=PainterMybrushRef::create("history-source-original-name");
  auto next=PainterMybrushRef::create("history-source-next");
  auto source_icon=pixels(0x2318afff);GError *error=nullptr;
  g_assert_true(gimp_painter_mybrush_set_icon(source.get(),source_icon.get(),&error));g_assert_no_error(error);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options.get()),source.get());
  const auto saved=preview_resource(0xf047219b);
  g_assert_true(gimp_painter_mybrush_options_set_json(options.get(),saved.encode().c_str(),&error));g_assert_no_error(error);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options.get(),"Immutable history name",&error));g_assert_no_error(error);
  const guint index=gimp_painter_mybrush_options_history_size(options.get());
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options.get()),next.get());
  auto image=query(options.get(),index);assert_pixels(image.get(),0xf047219b);
  // Exercise both in-place pixel mutation and native icon replacement. Neither
  // should change the draft snapshot's independently embedded preview.
  gdk_pixbuf_fill(source_icon.get(),0x000000ff);
  auto after_pixels=query(options.get(),index);assert_pixels(after_pixels.get(),0xf047219b);
  auto replacement=pixels(0x81d034ff);
  g_assert_true(gimp_painter_mybrush_set_icon(source.get(),replacement.get(),&error));g_assert_no_error(error);
  gimp_object_set_name(GIMP_OBJECT(source.get()),"history-source-renamed");
  auto after_replace=query(options.get(),index);assert_pixels(after_replace.get(),0xf047219b);
  String name(gimp_painter_mybrush_options_history_name(options.get(),index));
  g_assert_cmpstr(name.get(),==,"Immutable history name");
  g_assert_true(gimp_painter_binding_close(G_OBJECT(source.get()),&error));g_assert_no_error(error);
  auto after_close=query(options.get(),index);assert_pixels(after_close.get(),0xf047219b);
}

static void producer_lifetime()
{
  // The parent side of this subprocess case never creates options, but still
  // runs GUI teardown. Initialize its current device as well as the child's.
  gimp_set_focused_once(application);
  // Closing application history is terminal. Isolate that check so the shared
  // application used by the remaining selector cases stays active.
  if(!g_test_subprocess()) {
    g_test_trap_subprocess(nullptr,60*G_TIME_SPAN_SECOND,G_TEST_SUBPROCESS_DEFAULT);
    g_test_trap_assert_passed();
    return;
  }
  auto producer=options_new(),consumer=options_new();
  const guint index=append(producer.get(),preview_resource(0x174fc3d9));
  auto owned=query(producer.get(),index);
  bool finalized=false;
  g_object_weak_ref(G_OBJECT(producer.get()),+[](gpointer data,GObject*){*static_cast<bool*>(data)=true;},&finalized);
  producer.reset();g_assert_true(finalized);
  auto later=query(consumer.get(),index);assert_pixels(later.get(),0x174fc3d9);
  g_assert_true(gimp_painter_binding_close(G_OBJECT(application),nullptr));
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(consumer.get()),==,0);
  GError *error=nullptr;
  g_assert_null(gimp_painter_mybrush_options_history_preview(consumer.get(),index,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  consumer.reset();
  assert_pixels(owned.get(),0x174fc3d9);assert_pixels(later.get(),0x174fc3d9);
}

static void closed_and_thread()
{
  auto options=options_new();
  const guint index=append(options.get(),preview_resource(0xa1b2c3d4));
  struct ThreadRead {GimpPainterMybrushOptions *options;guint index;} request{options.get(),index};
  // Only the guarded C query runs off-thread. The owning thread retains the
  // model and waits; GTK, mutation and owner destruction remain on this thread.
  auto *thread=g_thread_new("history-preview-query",+[](gpointer data)->gpointer {
    const auto& request=*static_cast<ThreadRead*>(data);GError *error=nullptr;
    g_assert_null(gimp_painter_mybrush_options_history_preview(request.options,request.index,&error));
    g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_WRONG_THREAD);g_clear_error(&error);
    return nullptr;
  },&request);
  g_thread_join(thread);
  auto image=query(options.get(),index);assert_pixels(image.get(),0xa1b2c3d4);
  GError *error=nullptr;
  g_assert_true(gimp_painter_binding_close(G_OBJECT(options.get()),&error));g_assert_no_error(error);
  auto *binding=BindingStore::find(G_OBJECT(options.get()));
  const auto generation=binding->generation();
  g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),index,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),index,nullptr));
  g_assert_true(BindingStore::find(G_OBJECT(options.get()))==binding);
  g_assert_cmpuint(binding->generation(),==,generation);
  g_assert_true(binding->state()==BindingStore::State::closed);
  auto disposed=options_new();g_object_run_dispose(G_OBJECT(disposed.get()));
  g_assert_null(gimp_painter_mybrush_options_history_preview(disposed.get(),index,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  assert_pixels(image.get(),0xa1b2c3d4);
}

static void read_only()
{
  auto options=options_new();
  const guint index=append(options.get(),preview_resource(0x09ac37ff));
  const guint missing=append(options.get(),Resource(),"History without preview");
  Resource invalid_preview;invalid_preview.set_preview_png_base64("bm90IGEgcGljdHVyZQ==");
  const guint malformed=append(options.get(),invalid_preview,"History with malformed preview");
  auto source=ObjectRef<GimpPainterMybrush>::retain(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options.get())));
  g_object_set(options.get(),"opaque",.271,nullptr);
  GError *error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options.get(),"Keep unsaved draft",&error));g_assert_no_error(error);
  // Drain the append notifications before counting: a read cannot schedule
  // new ones, and should not be blamed for an already queued history change.
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  const auto before=PainterOptionsRef::retain(options.get()).snapshot().encode();
  const auto source_before=PainterMybrushRef::retain(source.get()).snapshot().encode();
  const guint count=gimp_painter_mybrush_options_history_size(options.get());
  auto *binding=BindingStore::find(G_OBJECT(options.get()));const auto generation=binding->generation();
  std::vector<std::string> names;
  for(guint i=0;i<count;++i) {
    String name(gimp_painter_mybrush_options_history_name(options.get(),i));names.emplace_back(name.get());
  }
  guint notifications=0;
  const auto changed=+[](GObject*,gpointer data){++*static_cast<guint*>(data);};
  const auto notified=+[](GObject*,GParamSpec*,gpointer data){++*static_cast<guint*>(data);};
  g_signal_connect(options.get(),"notify",G_CALLBACK(notified),&notifications);
  g_signal_connect(options.get(),"settings-changed",G_CALLBACK(changed),&notifications);
  g_signal_connect(options.get(),"history-changed",G_CALLBACK(changed),&notifications);
  g_signal_connect(options.get(),"painter-mybrush-changed",G_CALLBACK(+[](GObject*,GObject*,gpointer data){++*static_cast<guint*>(data);}),&notifications);
  g_signal_connect(source.get(),"notify",G_CALLBACK(notified),&notifications);
  g_signal_connect(source.get(),"settings-changed",G_CALLBACK(changed),&notifications);
  for(int repeat=0;repeat<4;++repeat) {
    auto image=query(options.get(),index);assert_pixels(image.get(),0x09ac37ff);
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),missing,&error));g_assert_no_error(error);
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),malformed,&error));
    g_assert_nonnull(error);g_clear_error(&error);
    g_assert_null(gimp_painter_mybrush_options_history_preview(options.get(),count,&error));
    g_assert_nonnull(error);g_clear_error(&error);
  }
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  g_assert_cmpuint(notifications,==,0);
  g_signal_handlers_disconnect_by_data(options.get(),&notifications);
  g_signal_handlers_disconnect_by_data(source.get(),&notifications);
  g_assert_true(PainterOptionsRef::retain(options.get()).snapshot().encode()==before);
  g_assert_true(PainterMybrushRef::retain(source.get()).snapshot().encode()==source_before);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options.get()))==source.get());
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options.get()),==,count);
  for(guint i=0;i<count;++i) {
    String name(gimp_painter_mybrush_options_history_name(options.get(),i));g_assert_cmpstr(name.get(),==,names[i].c_str());
  }
  gboolean dirty=FALSE,name_dirty=FALSE,conflict=TRUE;
  g_object_get(options.get(),"painter-dirty",&dirty,"painter-name-dirty",&name_dirty,"painter-conflict",&conflict,nullptr);
  g_assert_true(dirty);g_assert_true(name_dirty);g_assert_false(conflict);
  String draft_name(gimp_painter_mybrush_options_dup_draft_name(options.get(),&error));g_assert_no_error(error);
  g_assert_cmpstr(draft_name.get(),==,"Keep unsaved draft");
  g_assert_true(BindingStore::find(G_OBJECT(options.get()))==binding);
  g_assert_cmpuint(binding->generation(),==,generation);
}
} // namespace SelectorHistoryTest

static void register_selector_history_tests(Gimp *application)
{
  g_assert_true(GIMP_IS_GIMP(application));
  SelectorHistoryTest::application=application;
  g_test_add_func("/mypaint-selector/history/empty-and-invalid",SelectorHistoryTest::empty_and_invalid);
  g_test_add_func("/mypaint-selector/history/malformed",SelectorHistoryTest::malformed);
  g_test_add_func("/mypaint-selector/history/independent-owned-pixels",SelectorHistoryTest::independent_owned_pixels);
  g_test_add_func("/mypaint-selector/history/source-mutation",SelectorHistoryTest::source_mutation);
  g_test_add_func("/mypaint-selector/history/producer-lifetime",SelectorHistoryTest::producer_lifetime);
  g_test_add_func("/mypaint-selector/history/closed-and-thread",SelectorHistoryTest::closed_and_thread);
  g_test_add_func("/mypaint-selector/history/read-only",SelectorHistoryTest::read_only);
}
