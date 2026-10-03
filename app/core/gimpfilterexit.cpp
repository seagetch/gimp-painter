/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gimpfilterexit.h"
#include "painter/binding-store.hpp"
#include "painter/filter-lifetime.hpp"
#include "painter/source.hpp"
namespace GimpPainter {
template<> struct TypeTraits<GApplication> { static GType type () noexcept { return G_TYPE_APPLICATION; } };
namespace {
struct Exit {
  Source source;
  WeakRef<GApplication> application;
  gint64 started = 0;
  bool held = false, requested = false;
  explicit Exit (GApplication *app) : application (ObjectRef<GApplication>::retain (app)) {}
  void close () noexcept {
    source.close ();
    if (held) { held = false; auto app = application.lock (); if (app) g_application_release (app.get ()); }
    application.reset ();
  }
  bool poll () {
    auto app = application.lock ();
    if (!app) return false;
    if (FilterLifetime::pending () && g_get_monotonic_time () - started < 5 * G_USEC_PER_SEC) return true;
    if (FilterLifetime::pending ())
      g_printerr ("Filter cleanup exceeded the 5 second exit deadline; leaving through the explicit fallback\n");
    if (held) { held = false; g_application_release (app.get ()); }
    /* Keep the batch/native exit status untouched. */
    g_application_quit (app.get ()); return false;
  }
};
struct ExitSlot : SlotSpec<GApplication, Exit> {};
}
}
using namespace GimpPainter;
void gimp_filter_exit_init (GApplication *app)
{
  boundary_void (nullptr, [&] {
    auto& store = BindingStore::ensure (G_OBJECT (app));
    store.emplace<ExitSlot> (app); store.activate ();
  });
}
void gimp_filter_exit_quit (GApplication *app)
{
  FilterLifetime::stop_all ();
  try {
    auto& store = BindingStore::require (G_OBJECT (app));
    const auto generation = store.generation ();
    store.with<ExitSlot> ([&] (Exit& exit) {
      if (exit.requested) return;
      exit.requested = true; exit.started = g_get_monotonic_time ();
      if (!FilterLifetime::pending ()) { g_application_quit (app); return; }
      g_application_hold (app); exit.held = true;
      auto weak = std::make_shared<WeakRef<GApplication>> (ObjectRef<GApplication>::retain (app));
      exit.source = Source::timeout (nullptr, 10, G_PRIORITY_DEFAULT, [weak, generation] {
        auto app = weak->lock (); if (!app) return false;
        auto *store = BindingStore::find (G_OBJECT (app.get ()));
        if (!store || !store->accepts (generation)) return false;
        return store->with<ExitSlot> ([] (Exit& exit) { return exit.poll (); });
      });
    });
  } catch (const std::exception& error) {
    g_printerr ("Cannot schedule Filter cleanup drain: %s\n", error.what ());
    g_application_quit (app);
  } catch (...) {
    g_printerr ("Cannot schedule Filter cleanup drain\n"); g_application_quit (app);
  }
}

gboolean gimp_filter_exit_is_requested (GApplication *app)
{
  return boundary<gboolean> (nullptr, FALSE, [&] {
    return BindingStore::require (G_OBJECT (app)).read<ExitSlot> (
      [] (const Exit& exit) { return exit.requested; });
  });
}
