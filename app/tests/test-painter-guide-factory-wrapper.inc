/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "test-painter-guide-factory.h"
#include "painter/binding-store.hpp"
#include <new>
#include <stdexcept>
namespace GuideFactoryFault {
int kind=0, stage=0;
unsigned consumed=0, unwound=0, live=0;
GObject *target=nullptr;
static void finalized (gpointer, GObject *) noexcept { --live; }
static void inject () {
  const int current=kind; kind=0; ++consumed;
  struct Guard { ~Guard () noexcept { ++unwound; } } guard;
  switch (current) {
    case 1: throw GimpPainter::Error (GIMP_PAINTER_ERROR_INVALID_STATE, "guide typed fault");
    case 2: throw std::runtime_error ("guide standard fault");
    case 3: throw std::bad_alloc ();
    default: throw 19;
  }
}
}
extern "C" GimpPainter::BindingStore& __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *owner) {
  using namespace GuideFactoryFault;
  if (kind && g_strcmp0 (G_OBJECT_TYPE_NAME (owner), "GimpPerspectiveGuide")==0) {
    target=owner; ++live; g_object_weak_ref (owner, GuideFactoryFault::finalized, nullptr);
    if (stage==0) inject ();
  }
  return __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject (owner);
}
extern "C" void __real__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *store) {
  __real__ZN11GimpPainter12BindingStore8activateEv (store);
  using namespace GuideFactoryFault;
  if (kind && stage==1 && target && GimpPainter::BindingStore::find(target)==store) inject ();
}
void painter_guide_fault_arm (gint kind, gint stage) {
  g_assert_cmpuint (GuideFactoryFault::live,==,0); g_assert_cmpint (GuideFactoryFault::kind,==,0);
  GuideFactoryFault::kind=kind; GuideFactoryFault::stage=stage; GuideFactoryFault::target=nullptr;
  GuideFactoryFault::consumed=GuideFactoryFault::unwound=0;
}
gboolean painter_guide_fault_consumed () { return !GuideFactoryFault::kind && GuideFactoryFault::consumed==1 && GuideFactoryFault::unwound==1; }
guint painter_guide_fault_live () { return GuideFactoryFault::live; }
gboolean painter_guide_fault_closed (GObject *owner) {
  auto *store=GimpPainter::BindingStore::find(owner);
  return !store || store->state()==GimpPainter::BindingStore::State::closed;
}
void painter_guide_factory_handle_faults () {
  for (int stage=0;stage<2;++stage) for (int kind=1;kind<=4;++kind) {
    painter_guide_fault_arm(kind,stage);
    auto guide=GimpPainter::PerspectiveGuideRef::create(71);
    g_assert_false(bool(guide)); g_assert_true(painter_guide_fault_consumed());
    g_assert_cmpuint(painter_guide_fault_live(),==,0);
  }
}
