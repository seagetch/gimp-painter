/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "painter/binding-store.hpp"
#include "painter/boundary.hpp"
#include "test-painter-layer-initializer.h"
#include <new>
#include <stdexcept>
namespace {
int armed_kind = 0;
bool late = false;
GObject *target = nullptr;
unsigned consumed = 0, unwound = 0, observed = 0, live = 0;
void finalized (gpointer, GObject *) noexcept { --live; }
void observe (GtkWidget *widget) {
  ++observed; ++live;
  g_object_weak_ref (G_OBJECT (widget), finalized, nullptr);
  if (GTK_IS_CONTAINER (widget)) {
    GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
    for (GList *p = children; p; p = p->next) observe (GTK_WIDGET (p->data));
    g_list_free (children);
  }
}
void inject () {
  const int kind = armed_kind;
  armed_kind = 0; ++consumed;
  observe (GTK_WIDGET (target));
  struct Guard { ~Guard () noexcept { ++unwound; } } guard;
  switch (kind) {
    case 1: throw GimpPainter::Error (GIMP_PAINTER_ERROR_INVALID_STATE, "initializer typed fault");
    case 2: throw std::runtime_error ("initializer standard fault");
    case 3: throw std::bad_alloc ();
    default: throw 17;
  }
}
}
extern "C" GimpPainter::BindingStore& __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *);
extern "C" void __real__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *);
extern "C" GimpPainter::BindingStore&
__wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *owner) {
  if (armed_kind && g_strcmp0 (G_OBJECT_TYPE_NAME (owner), "GimpPainterLayerTiles") == 0) {
    target = owner;
    if (!late) inject ();
  }
  return __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject (owner);
}
extern "C" void
__wrap__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *store) {
  __real__ZN11GimpPainter12BindingStore8activateEv (store);
  if (armed_kind && late && target && GimpPainter::BindingStore::find (target) == store) inject ();
}
void painter_layer_init_fault_arm (gint kind, gboolean after_activation) {
  g_assert_cmpuint (live, ==, 0);
  g_assert_cmpint (armed_kind, ==, 0);
  armed_kind = kind; late = after_activation; target = nullptr;
  consumed = unwound = observed = 0;
}
gboolean painter_layer_init_fault_consumed () { return !armed_kind && consumed == 1 && unwound == 1; }
guint painter_layer_init_observed () { return observed; }
guint painter_layer_init_live () { return live; }
gboolean painter_layer_init_store_closed (GObject *owner) {
  auto *store = GimpPainter::BindingStore::find (owner);
  return !store || store->state () == GimpPainter::BindingStore::State::closed;
}
