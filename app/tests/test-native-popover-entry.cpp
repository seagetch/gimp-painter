/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "widgets/popupper.hpp"
#include "widgets/popupper.hpp"
#include "painter/binding-store.hpp"
#include <new>
#include <stdexcept>
extern "C" void popover_cpp_handles(void);
extern "C" void popover_cpp_faults(void);
static void finalized_count(gpointer p){++*static_cast<unsigned*>(p);}
extern "C" GtkWidget *popover_c_factory(GtkWidget*);
extern "C" GtkWidget *popover_c_raw_new(void);
namespace Fault {
int kind=0,stage=0; unsigned consumed=0,unwound=0,live=0; GObject *target=nullptr;
static void finalized(gpointer,GObject*) noexcept {--live;}
static void inject(){const int k=kind;kind=0;++consumed;struct Guard{~Guard()noexcept{++unwound;}}guard;
  switch(k){case 1:throw GimpPainter::Error(GIMP_PAINTER_ERROR_INVALID_STATE,"popover typed fault");case 2:throw std::runtime_error("popover standard fault");case 3:throw std::bad_alloc();default:throw 19;}}
static void arm(int k,int s){g_assert_cmpuint(live,==,0);g_assert_cmpint(kind,==,0);kind=k;stage=s;target=nullptr;consumed=unwound=0;}
}
extern "C" GimpPainter::BindingStore& __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*owner){
  using namespace Fault;
  if(kind&&g_strcmp0(G_OBJECT_TYPE_NAME(owner),"GimpPopover2")==0){target=owner;++live;g_object_weak_ref(owner,finalized,nullptr);if(stage==0)inject();}
  return __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject(owner);
}
extern "C" void __real__ZN11GimpPainter12BindingStore8activateEv(GimpPainter::BindingStore*);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv(GimpPainter::BindingStore*);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv(GimpPainter::BindingStore*store){
  __real__ZN11GimpPainter12BindingStore8activateEv(store);using namespace Fault;
  if(kind&&stage==1&&target&&GimpPainter::BindingStore::find(target)==store)inject();
}
extern "C" void popover_cpp_faults(void){
  using namespace GimpPainter;
  for(int stage=0;stage<2;++stage)for(int kind=1;kind<=4;++kind){
    GtkWidget*view=gtk_entry_new();Fault::arm(kind,stage);
    g_assert_null(popover_c_factory(view));g_assert_cmpuint(Fault::consumed,==,1);g_assert_cmpuint(Fault::unwound,==,1);g_assert_cmpuint(Fault::live,==,0);
    g_assert_true(g_object_is_floating(view));g_assert_null(gtk_widget_get_parent(view));g_object_ref_sink(view);g_object_unref(view);
    Fault::arm(kind,stage);auto *raw=popover_c_raw_new();
    auto *store=BindingStore::find(G_OBJECT(raw));g_assert_true(!store||store->state()==BindingStore::State::closed);
    g_assert_cmpuint(Fault::consumed,==,1);g_assert_cmpuint(Fault::unwound,==,1);
    GdkEventKey key{};key.keyval=GDK_KEY_Escape;g_assert_false(GTK_WIDGET_GET_CLASS(raw)->key_press_event(raw,&key));
    GdkEventButton button{};g_assert_false(GTK_WIDGET_GET_CLASS(raw)->button_press_event(raw,&button));
    g_signal_emit_by_name(raw,"cancel");gtk_widget_destroy(raw);g_assert_cmpuint(Fault::live,==,0);
  }
}
extern "C" void popover_cpp_handles(void){
  using namespace GimpPainter;
  g_assert_false(PopoverRef::create(nullptr));g_assert_false(PopoverRef::retain(nullptr));
  auto owner=PopoverRef::create(gtk_entry_new());g_assert_true(bool(owner));
  unsigned finalized=0;auto*raw=owner.get();g_object_set_data_full(G_OBJECT(raw),"finalize-probe",&finalized,finalized_count);gpointer weak=raw;g_object_add_weak_pointer(G_OBJECT(raw),&weak);
  auto copy=owner;auto moved=std::move(copy);g_assert_false(bool(copy));g_assert_true(moved.get()==raw);
  owner=owner;owner.reset();g_assert_nonnull(weak);gtk_widget_destroy(GTK_WIDGET(raw));g_assert_null(weak);
  /* GTK destruction clears weak refs during dispose; retained object still lives. */
  g_assert_cmpuint(finalized,==,0);g_assert_true(GIMP_IS_POPOVER(moved.get()));
  moved.reset();g_assert_cmpuint(finalized,==,1);
  auto independent=PopoverRef::create(gtk_entry_new());raw=independent.get();weak=raw;
  g_object_add_weak_pointer(G_OBJECT(raw),&weak);independent.reset();g_assert_nonnull(weak);
  gtk_widget_destroy(GTK_WIDGET(raw));g_assert_null(weak);
}
