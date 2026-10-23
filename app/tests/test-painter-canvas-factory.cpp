/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "painter/binding-store.hpp"
#include "test-painter-canvas-factory.h"
#include <new>
#include <stdexcept>
namespace {
int armed_kind = 0, stage = 0;
GObject *target = nullptr;
unsigned consumed = 0, unwound = 0, live = 0;
struct ObservedConnection { GObject *emitter; gulong id; };
ObservedConnection observed[16];
unsigned n_connections = 0;
void finalized (gpointer, GObject *) noexcept { --live; }
void inject () {
  const int kind = armed_kind;
  armed_kind = 0; ++consumed;
  struct Guard { ~Guard () noexcept { ++unwound; } } guard;
  switch (kind) {
    case 1: throw GimpPainter::Error (GIMP_PAINTER_ERROR_INVALID_STATE, "canvas typed fault");
    case 2: throw std::runtime_error ("canvas standard fault");
    case 3: throw std::bad_alloc ();
    default: throw 17;
  }
}
}
extern "C" gulong __wrap_g_signal_connect_data (gpointer, const gchar *, GCallback, gpointer, GClosureNotify, GConnectFlags);
extern "C" gboolean __wrap_g_signal_parse_name (const gchar *, GType, guint *, GQuark *, gboolean);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *);
extern "C" GimpPainter::BindingStore& __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject (GObject *owner) {
  if (armed_kind && g_strcmp0 (G_OBJECT_TYPE_NAME (owner), "GimpCanvasPerspectiveGuide") == 0) {
    target = owner; ++live; g_object_weak_ref (owner, finalized, nullptr);
    if (stage == 0) inject ();
  }
  return __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject (owner);
}
extern "C" void __real__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *);
extern "C" void __wrap__ZN11GimpPainter12BindingStore8activateEv (GimpPainter::BindingStore *store) {
  __real__ZN11GimpPainter12BindingStore8activateEv (store);
  if (armed_kind && stage == 2 && target && GimpPainter::BindingStore::find (target) == store) inject ();
}
extern "C" gboolean __real_g_signal_parse_name (const gchar *, GType, guint *, GQuark *, gboolean);
extern "C" gboolean __wrap_g_signal_parse_name (const gchar *name, GType type, guint *id, GQuark *detail, gboolean force) {
  /* The shell notification is already connected; fail the next C++ Connection
   * before its ownership transfer. No exception crosses a GLib implementation. */
  if (armed_kind && stage == 1 && target && g_strcmp0 (name, "notify::image") == 0) inject ();
  return __real_g_signal_parse_name (name, type, id, detail, force);
}
extern "C" gulong __real_g_signal_connect_data (gpointer, const gchar *, GCallback, gpointer, GClosureNotify, GConnectFlags);
extern "C" gulong __wrap_g_signal_connect_data (gpointer emitter, const gchar *name, GCallback callback,
                                                gpointer data, GClosureNotify destroy, GConnectFlags flags) {
  const gulong id = __real_g_signal_connect_data (emitter, name, callback, data, destroy, flags);
  if (armed_kind && target) {
    g_assert_cmpuint (n_connections, <, G_N_ELEMENTS (observed));
    observed[n_connections++] = {G_OBJECT (emitter), id};
  }
  return id;
}
guint painter_canvas_fault_connections () { return n_connections; }
gboolean painter_canvas_fault_disconnected () {
  for (unsigned i = 0; i < n_connections; ++i)
    if (g_signal_handler_is_connected (observed[i].emitter, observed[i].id)) return FALSE;
  return TRUE;
}
void painter_canvas_fault_arm (gint kind, gint at_stage) {
  g_assert_cmpuint (live, ==, 0); g_assert_cmpint (armed_kind, ==, 0);
  armed_kind = kind; stage = at_stage; target = nullptr; consumed = unwound = n_connections = 0;
}
gboolean painter_canvas_fault_consumed () { return !armed_kind && consumed == 1 && unwound == 1; }
guint painter_canvas_fault_live () { return live; }
gboolean painter_canvas_store_closed (GObject *owner) {
  auto *store = GimpPainter::BindingStore::find (owner);
  return !store || store->state () == GimpPainter::BindingStore::State::closed;
}

#include <gegl.h>
#include "display/gimpcanvasperspectiveguide.hpp"
#include <type_traits>
using GimpPainter::CanvasPerspectiveGuideRef;
static_assert (std::is_nothrow_move_constructible<CanvasPerspectiveGuideRef>::value, "native owner move");
static_assert (std::is_nothrow_move_assignable<CanvasPerspectiveGuideRef>::value, "native owner move assignment");
void painter_canvas_handle_test (gpointer shell) {
  CanvasPerspectiveGuideRef empty;
  g_assert_false (bool (empty));
  g_assert_false (bool (CanvasPerspectiveGuideRef::retain (nullptr)));
  g_assert_false (bool (CanvasPerspectiveGuideRef::adopt (nullptr)));
  auto wrong = GimpPainter::ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  for (bool adopt : {false, true}) {
    bool caught = false;
    try {
      auto *p = reinterpret_cast<GimpCanvasPerspectiveGuide *> (wrong.get ());
      auto rejected = adopt ? CanvasPerspectiveGuideRef::adopt (p) : CanvasPerspectiveGuideRef::retain (p);
    } catch (const GimpPainter::Error& error) { caught = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
    g_assert_true (caught); g_assert_cmpuint (wrong.get ()->ref_count, ==, 1);
  }
  const guint shell_refs = G_OBJECT (shell)->ref_count;
  auto guide = CanvasPerspectiveGuideRef::create (static_cast<GimpDisplayShell *> (shell));
  g_assert_true (bool (guide)); g_assert_false (g_object_is_floating (guide.get ()));
  g_assert_cmpuint (G_OBJECT (shell)->ref_count, ==, shell_refs);
  gpointer weak = guide.get (); g_object_add_weak_pointer (G_OBJECT (guide.get ()), &weak);
  auto copy = guide;
  auto retained = CanvasPerspectiveGuideRef::retain (guide.get ());
  auto& same = guide; guide = same; guide = std::move (same);
  auto moved = std::move (guide); g_assert_false (bool (guide));
  guide = std::move (moved); g_assert_false (bool (moved));
  guide.reset (); copy.reset (); g_assert_nonnull (weak);
  retained.reset (); g_assert_null (weak);
  g_assert_cmpuint (G_OBJECT (shell)->ref_count, ==, shell_refs);
  for (int stage = 0; stage < 3; ++stage) {
    painter_canvas_fault_arm (3, stage);
    auto failed = CanvasPerspectiveGuideRef::create (static_cast<GimpDisplayShell *> (shell));
    g_assert_false (bool (failed)); g_assert_true (painter_canvas_fault_consumed ());
    g_assert_cmpuint (painter_canvas_fault_live (), ==, 0);
  }
}
