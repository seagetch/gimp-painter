/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpgrouplayer.h"
#include "painter/gimp-painter-binding.h"
#include "core/gimplayer-new.h"
}
#include "core/gimpclonelayer-handle.hpp"
#include "core/gimpclonelayerundo.h"
#include "painter/binding-store.hpp"
#include <cstddef>
#include <type_traits>

namespace {
Gimp *clone_test_gimp;
using ImageOwner = std::unique_ptr<GimpImage, decltype (&g_object_unref)>;

GimpImage *new_handle_image ()
{
  return gimp_image_new (clone_test_gimp, 100, 100, GIMP_RGB,
                         GIMP_PRECISION_U8_NON_LINEAR);
}

GimpLayer *new_handle_source (GimpImage *image, const char *name,
                             GimpLayer *parent = nullptr, gint position = 0)
{
  auto *source = gimp_layer_new (image, 16, 16, babl_format ("R'G'B'A u8"),
                                 name, 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (image, source, parent, position, FALSE));
  return source;
}

GimpPainter::CloneLayerRef new_handle_clone (GimpImage *image,
                                            GimpLayer *source = nullptr)
{
  auto clone = GimpPainter::CloneLayerRef::create (image, source, 16, 16,
                                                 "handle clone", 1.0,
                                                 GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (image, GIMP_LAYER (clone.get ()),
                                      nullptr, 0, FALSE));
  return clone;
}

#ifdef GIMP_TEST_CLONE_WRAP_SETTERS
gint source_entry_calls;
gint name_entry_calls;
#endif
}

#ifdef GIMP_TEST_CLONE_WRAP_SETTERS
/* These probes count the public C entry, then execute the real provider. They
 * neither replace its implementation nor infer routing from matching output. */
extern "C" void __real_gimp_clone_layer_set_source (GimpCloneLayer *, GimpLayer *);
extern "C" void __real_gimp_clone_layer_set_source_by_name (GimpCloneLayer *, const gchar *);
extern "C" void __wrap_gimp_clone_layer_set_source (GimpCloneLayer *clone, GimpLayer *source)
{
  g_atomic_int_inc (&source_entry_calls);
  __real_gimp_clone_layer_set_source (clone, source);
}
extern "C" void __wrap_gimp_clone_layer_set_source_by_name (GimpCloneLayer *clone, const gchar *name)
{
  g_atomic_int_inc (&name_entry_calls);
  __real_gimp_clone_layer_set_source_by_name (clone, name);
}
#endif

namespace {
void handle_original_set_source_route ()
{
#ifdef GIMP_TEST_CLONE_WRAP_SETTERS
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "route source");
  auto clone = new_handle_clone (image.get ());
  g_atomic_int_set (&source_entry_calls, 0);
  clone.set_source (source);
  const auto calls = g_atomic_int_get (&source_entry_calls);
  g_test_message ("original set_source entry calls: expected=1 actual=%d", calls);
  g_assert_cmpint (calls, ==, 1);
  g_assert_true (clone.source ().get () == source);
#else
  g_test_skip ("Original-entry route probe requires supported linker --wrap");
#endif
}

void handle_original_set_source_name_route ()
{
#ifdef GIMP_TEST_CLONE_WRAP_SETTERS
  ImageOwner image (new_handle_image (), g_object_unref);
  auto clone = new_handle_clone (image.get ());
  g_atomic_int_set (&name_entry_calls, 0);
  clone.set_source_name ("unresolved route name");
  const auto calls = g_atomic_int_get (&name_entry_calls);
  g_test_message ("original set_source_by_name entry calls: expected=1 actual=%d", calls);
  g_assert_cmpint (calls, ==, 1);
  auto name = clone.source_name ();
  g_assert_cmpstr (name.get (), ==, "unresolved route name");
#else
  g_test_skip ("Original-entry route probe requires supported linker --wrap");
#endif
}
}

namespace {
using CloneHandle = GimpPainter::CloneLayerRef;

void assert_source_name (const CloneHandle& clone, const char *expected)
{
  auto name = clone.source_name ();
  g_assert_cmpstr (name.get (), ==, expected);
}

void assert_reference (const CloneHandle& clone, GimpLayer *source,
                       const char *pending, GimpCloneSourceState state)
{
  auto reference = clone.reference ();
  g_assert_true (reference->source == source);
  g_assert_cmpstr (reference->pending_name, ==, pending);
  g_assert_cmpint (reference->state, ==, state);
}

void assert_same_reference (const GimpCloneLayerReference& before,
                            const GimpCloneLayerReference& after)
{
  g_assert_true (after.source == before.source);
  g_assert_cmpstr (after.pending_name, ==, before.pending_name);
  g_assert_cmpstr (after.source_name, ==, before.source_name);
  g_assert_cmpint (after.state, ==, before.state);
  g_assert_cmpint (after.allow_name_lookup, ==, before.allow_name_lookup);
  g_assert_cmpint (after.source_expired, ==, before.source_expired);
}

template<class Function>
void expect_handle_error (GimpPainterError code, Function&& operation)
{
  try { operation (); }
  catch (const GimpPainter::Error& error)
    {
      g_assert_cmpint (error.code (), ==, code);
      g_assert_cmpstr (error.what (), !=, "");
      return;
    }
  catch (...) { g_error ("Unexpected non-Painter exception from clone handle"); }
  g_error ("Clone handle did not report expected error %d", code);
}

template<class Function>
void expect_no_handle_error (Function&& operation)
{
  try { operation (); }
  catch (const std::exception& error)
    { g_error ("Original-entry handle call leaked an exception: %s", error.what ()); }
  catch (...) { g_error ("Original-entry handle call leaked an unknown exception"); }
}

void fill_handle_source (GimpLayer *layer, guchar r, guchar g, guchar b)
{
  guchar pixels[16 * 16 * 4];
  for (guint i = 0; i < 16 * 16; ++i)
    {
      pixels[4 * i] = r; pixels[4 * i + 1] = g;
      pixels[4 * i + 2] = b; pixels[4 * i + 3] = 255;
    }
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)),
                    GEGL_RECTANGLE (0, 0, 16, 16), 0,
                    babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE);
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, 16, 16);
}

void assert_handle_pixel (const CloneHandle& clone, guchar r, guchar g, guchar b)
{
  const guchar expected[] = { r, g, b, 255 };
  guchar actual[4];
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (clone.get ())),
                    GEGL_RECTANGLE (0, 0, 1, 1), 1.0,
                    babl_format ("R'G'B'A u8"), actual, GEGL_AUTO_ROWSTRIDE,
                    GEGL_ABYSS_NONE);
  g_assert_cmpmem (actual, sizeof actual, expected, sizeof expected);
}

void handle_factory_and_release_orders ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "factory source");
  gimp_item_set_offset (GIMP_ITEM (source), 5, 7);
  GimpCloneLayer *weak_clone = nullptr;
  {
    auto clone = CloneHandle::create (image.get (), source, 0, 0, "factory clone",
                                      0.75, GIMP_LAYER_MODE_NORMAL);
    weak_clone = clone.get ();
    g_object_add_weak_pointer (G_OBJECT (clone.get ()),
                               reinterpret_cast<gpointer *> (&weak_clone));
    g_assert_false (g_object_is_floating (clone.get ()));
    g_assert_false (gimp_item_is_attached (GIMP_ITEM (clone.get ())));
    g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (clone.get ())), ==, 16);
    g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (clone.get ())), ==, 16);
    g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (clone.get ())), ==, 5);
    g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (clone.get ())), ==, 7);
    g_assert_cmpfloat (gimp_layer_get_opacity (GIMP_LAYER (clone.get ())), ==, 0.75);
    g_assert_cmpint (gimp_layer_get_mode (GIMP_LAYER (clone.get ())), ==,
                     GIMP_LAYER_MODE_NORMAL);
    {
      auto retained = CloneHandle::retain (clone.get ());
      auto adopted = CloneHandle::adopt (
        static_cast<GimpCloneLayer *> (g_object_ref (clone.get ())));
      auto sunk = CloneHandle::sink (clone.get ());
      auto copied = retained;
      auto moved = std::move (copied);
      g_assert_true (retained.get () == clone.get ());
      g_assert_true (adopted.get () == clone.get ());
      g_assert_true (sunk.get () == clone.get ());
      g_assert_true (moved.get () == clone.get ());
      g_assert_null (copied.get ());
    }
    g_assert_nonnull (weak_clone);
  }
  g_assert_null (weak_clone);
  expect_handle_error (GIMP_PAINTER_ERROR_WRONG_TYPE,
                        [] { CloneHandle::retain (nullptr); });
  expect_handle_error (GIMP_PAINTER_ERROR_WRONG_TYPE,
                        [] { CloneHandle::adopt (nullptr); });
  expect_handle_error (GIMP_PAINTER_ERROR_WRONG_TYPE,
                        [] { CloneHandle::sink (nullptr); });

  for (gint image_first = 0; image_first < 2; ++image_first)
    {
      ImageOwner owner (new_handle_image (), g_object_unref);
      {
        auto clone = new_handle_clone (owner.get ());
        weak_clone = clone.get ();
        g_object_add_weak_pointer (G_OBJECT (clone.get ()),
                                   reinterpret_cast<gpointer *> (&weak_clone));
        if (image_first)
          {
            owner.reset ();
            g_assert_nonnull (weak_clone);
            g_assert_null (gimp_item_get_image (GIMP_ITEM (clone.get ())));
            g_assert_false (static_cast<bool> (clone.source ()));
          }
      }
      if (!image_first)
        {
          g_assert_nonnull (weak_clone); // image container still owns the clone
          owner.reset ();
        }
      g_assert_null (weak_clone);
    }
}

void handle_source_reference_independence ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "retained handle source");
  auto clone = new_handle_clone (image.get (), source);
  auto held = clone.source ();
  auto copied = held;
  auto *weak_source = source;
  g_object_add_weak_pointer (G_OBJECT (source), reinterpret_cast<gpointer *> (&weak_source));
  clone.set_source (nullptr);
  assert_reference (clone, nullptr, nullptr, GIMP_CLONE_SOURCE_NONE);
  gimp_image_remove_layer (image.get (), source, FALSE, nullptr);
  image.reset ();
  g_assert_nonnull (weak_source);
  g_assert_true (held.get () == source);
  g_assert_cmpstr (gimp_object_get_name (held.get ()), ==, "retained handle source");
  g_assert_null (gimp_item_get_image (GIMP_ITEM (held.get ())));
  g_assert_false (static_cast<bool> (clone.source ()));
  held.reset ();
  g_assert_nonnull (weak_source);
  copied.reset ();
  g_assert_null (weak_source);
}

void handle_pending_null_empty_and_rename ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "live source");
  auto *other = new_handle_source (image.get (), "other source");
  auto clone = new_handle_clone (image.get (), source);
  clone.set_source_name ("missing name");
  assert_reference (clone, source, "missing name", GIMP_CLONE_SOURCE_PENDING);
  g_assert_true (clone.source ().get () == source);
  assert_reference (clone, source, "missing name", GIMP_CLONE_SOURCE_PENDING);
  clone.set_source (other);
  assert_reference (clone, other, "missing name", GIMP_CLONE_SOURCE_PENDING);
  clone.set_source_name ("");
  assert_reference (clone, other, "", GIMP_CLONE_SOURCE_PENDING);
  assert_source_name (clone, "");
  g_assert_true (clone.source ().get () == other);
  assert_reference (clone, other, "", GIMP_CLONE_SOURCE_PENDING);
  clone.set_source_name (nullptr);
  assert_reference (clone, other, nullptr, GIMP_CLONE_SOURCE_LIVE);
  gimp_object_set_name (GIMP_OBJECT (other), "renamed handle source");
  g_assert_true (clone.source ().get () == other);
  assert_source_name (clone, "renamed handle source");
  clone.set_source (nullptr);
  assert_reference (clone, nullptr, nullptr, GIMP_CLONE_SOURCE_NONE);
  g_assert_false (static_cast<bool> (clone.source ()));
  assert_source_name (clone, "renamed handle source");
}

void handle_recursive_first_match ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *group = gimp_group_layer_new (image.get ());
  gimp_object_set_name (GIMP_OBJECT (group), "group");
  g_assert_true (gimp_image_add_layer (image.get (), group, nullptr, 0, FALSE));
  auto *inside = new_handle_source (image.get (), "duplicate", group);
  new_handle_source (image.get (), "duplicate", nullptr, 1);
  auto clone = new_handle_clone (image.get ());
  clone.set_source_name ("duplicate");
  assert_reference (clone, nullptr, "duplicate", GIMP_CLONE_SOURCE_PENDING);
  g_assert_true (clone.source ().get () == inside);
  assert_reference (clone, inside, nullptr, GIMP_CLONE_SOURCE_LIVE);
  gimp_object_set_name (GIMP_OBJECT (group), "duplicate");
  clone.set_source_name ("duplicate");
  g_assert_true (clone.source ().get () == group);
  assert_reference (clone, group, nullptr, GIMP_CLONE_SOURCE_LIVE);
}

void count_handle_source_update (GimpDrawable *, gint, gint, gint, gint, gpointer data)
{ ++*static_cast<guint *> (data); }

void handle_updates_and_lazy_resolution ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "direct source");
  auto clone = new_handle_clone (image.get ());
  fill_handle_source (source, 255, 0, 0);
  guint source_updates = 0, later_updates = 0;
  const auto source_id = g_signal_connect (source, "update",
    G_CALLBACK (count_handle_source_update), &source_updates);
  clone.set_source (source);
  g_assert_cmpuint (source_updates, ==, 1);
  assert_handle_pixel (clone, 255, 0, 0);
  clone.set_source (source);
  g_assert_cmpuint (source_updates, ==, 2);
  clone.set_source_name ("later target");
  g_assert_cmpuint (source_updates, ==, 2);
  clone.set_source (nullptr);
  g_assert_cmpuint (source_updates, ==, 2);
  assert_handle_pixel (clone, 255, 0, 0); // detach keeps the projected cache
  clone.set_source (source);
  g_assert_cmpuint (source_updates, ==, 3);
  assert_reference (clone, source, "later target", GIMP_CLONE_SOURCE_PENDING);
  auto *later = new_handle_source (image.get (), "later target");
  fill_handle_source (later, 0, 255, 0);
  const auto later_id = g_signal_connect (later, "update",
    G_CALLBACK (count_handle_source_update), &later_updates);
  /* Snapshot must remain nonmutating even after the target becomes available. */
  assert_reference (clone, source, "later target", GIMP_CLONE_SOURCE_PENDING);
  g_assert_cmpuint (later_updates, ==, 0);
  g_assert_true (clone.source ().get () == later);
  g_assert_cmpuint (later_updates, ==, 1);
  assert_handle_pixel (clone, 0, 255, 0);
  clone.set_source (source);
  g_assert_cmpuint (source_updates, ==, 4);
  clone.set_source_name ("later target");
  clone.set_source (source);
  g_assert_cmpuint (source_updates, ==, 5);
  g_assert_cmpuint (later_updates, ==, 2);
  /* Preserve the accepted stale-callback repair, not legacy stale overwrite. */
  assert_reference (clone, later, nullptr, GIMP_CLONE_SOURCE_LIVE);
  assert_handle_pixel (clone, 0, 255, 0);
  g_signal_handler_disconnect (source, source_id);
  g_signal_handler_disconnect (later, later_id);
}

struct HandleThawAction
{
  const CloneHandle *clone;
  GimpLayer *replacement;
  bool close;
  bool fired = false;
};

void handle_on_thaw (GObject *object, GParamSpec *, gpointer data)
{
  auto& action = *static_cast<HandleThawAction *> (data);
  if (action.fired || gimp_viewable_preview_is_frozen (GIMP_VIEWABLE (object))) return;
  action.fired = true;
  if (action.close) g_assert_true (gimp_painter_binding_close (object, nullptr));
  else expect_no_handle_error ([&] { action.clone->set_source (action.replacement); });
}

void handle_reentrant_replace_and_close ()
{
  for (gint close = 0; close < 2; ++close)
    {
      ImageOwner image (new_handle_image (), g_object_unref);
      auto *old = new_handle_source (image.get (), "old source");
      auto *outer = new_handle_source (image.get (), "outer source");
      auto *inner = new_handle_source (image.get (), "inner source");
      fill_handle_source (old, 0, 0, 255);
      fill_handle_source (outer, 255, 0, 0);
      fill_handle_source (inner, 0, 255, 0);
      auto clone = new_handle_clone (image.get (), old);
      gimp_viewable_preview_freeze (GIMP_VIEWABLE (old));
      HandleThawAction action { &clone, inner, close != 0 };
      const auto id = g_signal_connect (clone.get (), "notify::frozen",
                                        G_CALLBACK (handle_on_thaw), &action);
      expect_no_handle_error ([&] { clone.set_source (outer); });
      g_assert_true (action.fired);
      g_signal_handler_disconnect (clone.get (), id);
      if (close)
        {
          assert_handle_pixel (clone, 0, 0, 255);
          auto closed_reference = clone.reference ();
          g_assert_null (closed_reference->source);
          expect_no_handle_error ([&] { clone.set_source (inner); });
          expect_no_handle_error ([&] { clone.set_source_name ("ignored after close"); });
          assert_same_reference (*closed_reference, *clone.reference ());
          assert_handle_pixel (clone, 0, 0, 255);
        }
      else
        {
          assert_reference (clone, inner, nullptr, GIMP_CLONE_SOURCE_LIVE);
          assert_handle_pixel (clone, 0, 255, 0);
          fill_handle_source (outer, 255, 255, 0);
          assert_reference (clone, inner, nullptr, GIMP_CLONE_SOURCE_LIVE);
          assert_handle_pixel (clone, 0, 255, 0);
        }
      gimp_viewable_preview_thaw (GIMP_VIEWABLE (old));
    }
}

void handle_contained_invalid_and_closed ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "valid source");
  auto clone = new_handle_clone (image.get (), source);
  clone.set_source_name ("preserved pending");
  auto wrong = GimpPainter::ObjectRef<GObject>::adopt (
    G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  g_assert_null (GimpPainter::BindingStore::find (wrong.get ()));
  expect_no_handle_error ([&] {
    clone.set_source (reinterpret_cast<GimpLayer *> (wrong.get ()));
    gimp_clone_layer_set_source (reinterpret_cast<GimpCloneLayer *> (wrong.get ()), source);
    gimp_clone_layer_set_source_by_name (reinterpret_cast<GimpCloneLayer *> (wrong.get ()), "ignored");
  });
  g_assert_null (GimpPainter::BindingStore::find (wrong.get ()));
  assert_reference (clone, source, "preserved pending", GIMP_CLONE_SOURCE_PENDING);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (clone.get ()), nullptr));
  auto closed_reference = clone.reference ();
  g_assert_null (closed_reference->source);
  expect_no_handle_error ([&] { clone.set_source (nullptr); });
  expect_no_handle_error ([&] { clone.set_source_name ("ignored"); });
  g_assert_false (static_cast<bool> (clone.source ()));
  assert_source_name (clone, "preserved pending");
  assert_same_reference (*closed_reference, *clone.reference ());
}

void handle_checked_error_contract ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "checked source");
  auto clone = new_handle_clone (image.get ());
#ifdef GIMP_TEST_CLONE_WRAP_SETTERS
  g_atomic_int_set (&source_entry_calls, 0);
  g_atomic_int_set (&name_entry_calls, 0);
#endif
  clone.set_source_checked (source);
  clone.set_source_name_checked ("checked pending");
  assert_reference (clone, source, "checked pending", GIMP_CLONE_SOURCE_PENDING);
  auto wrong = GimpPainter::ObjectRef<GObject>::adopt (
    G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  expect_handle_error (GIMP_PAINTER_ERROR_WRONG_TYPE, [&] {
    clone.set_source_checked (reinterpret_cast<GimpLayer *> (wrong.get ()));
  });
  assert_reference (clone, source, "checked pending", GIMP_CLONE_SOURCE_PENDING);
  clone.set_source_name_checked (nullptr);
  assert_reference (clone, source, nullptr, GIMP_CLONE_SOURCE_LIVE);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (clone.get ()), nullptr));
  expect_handle_error (GIMP_PAINTER_ERROR_CLOSED,
                        [&] { clone.set_source_checked (source); });
  expect_handle_error (GIMP_PAINTER_ERROR_CLOSED,
                        [&] { clone.set_source_name_checked ("ignored"); });
#ifdef GIMP_TEST_CLONE_WRAP_SETTERS
  g_assert_cmpint (g_atomic_int_get (&source_entry_calls), ==, 0);
  g_assert_cmpint (g_atomic_int_get (&name_entry_calls), ==, 0);
#endif
}

struct HandleThreadProbe
{
  const CloneHandle *clone;
  GimpLayer *source;
  bool escaped = false;
  bool getter_empty = false;
  gint source_error = -1;
  gint name_error = -1;
};

gpointer handle_wrong_thread_worker (gpointer data)
{
  auto& probe = *static_cast<HandleThreadProbe *> (data);
  // The main thread retains every object and owns the handle for this join.
  // Failed boundary operations never reach drawable, image, or GTK mutations.
  try
    {
      probe.clone->set_source (probe.source);
      probe.clone->set_source_name ("wrong-thread pending");
      probe.getter_empty = !probe.clone->source ();
    }
  catch (...) { probe.escaped = true; }
  try { probe.clone->set_source_checked (probe.source); }
  catch (const GimpPainter::Error& error) { probe.source_error = error.code (); }
  catch (...) { probe.escaped = true; }
  try { probe.clone->set_source_name_checked ("wrong-thread checked pending"); }
  catch (const GimpPainter::Error& error) { probe.name_error = error.code (); }
  catch (...) { probe.escaped = true; }
  return nullptr;
}

void handle_wrong_thread_containment ()
{
  ImageOwner image (new_handle_image (), g_object_unref);
  auto *source = new_handle_source (image.get (), "owner-thread source");
  auto *other = new_handle_source (image.get (), "wrong-thread source");
  auto clone = new_handle_clone (image.get (), source);
  clone.set_source_name ("owner-thread pending");
  auto before = clone.reference ();
  HandleThreadProbe probe { &clone, other };
  auto *worker = g_thread_new ("clone-entry-probe", handle_wrong_thread_worker, &probe);
  g_thread_join (worker);
  g_assert_false (probe.escaped);
  g_assert_true (probe.getter_empty);
  g_assert_cmpint (probe.source_error, ==, GIMP_PAINTER_ERROR_WRONG_THREAD);
  g_assert_cmpint (probe.name_error, ==, GIMP_PAINTER_ERROR_WRONG_THREAD);
  assert_reference (clone, source, "owner-thread pending", GIMP_CLONE_SOURCE_PENDING);
  assert_same_reference (*before, *clone.reference ());
}
}

extern "C" void gimp_test_clone_register_cpp_tests (Gimp *application);
extern "C" void gimp_test_clone_register_cpp_tests (Gimp *application)
{
  clone_test_gimp = application;
#define ADD_HANDLE(name) g_test_add_func ("/gimp-clone-layer/" #name, name)
  ADD_HANDLE (handle_original_set_source_route);
  ADD_HANDLE (handle_original_set_source_name_route);
  ADD_HANDLE (handle_factory_and_release_orders);
  ADD_HANDLE (handle_source_reference_independence);
  ADD_HANDLE (handle_pending_null_empty_and_rename);
  ADD_HANDLE (handle_recursive_first_match);
  ADD_HANDLE (handle_updates_and_lazy_resolution);
  ADD_HANDLE (handle_reentrant_replace_and_close);
  ADD_HANDLE (handle_contained_invalid_and_closed);
  ADD_HANDLE (handle_checked_error_contract);
  ADD_HANDLE (handle_wrong_thread_containment);
#undef ADD_HANDLE
}

extern "C" void gimp_test_clone_undo_cpp_type (GimpCloneLayerUndo *undo);
extern "C" void gimp_test_clone_undo_cpp_type (GimpCloneLayerUndo *undo)
{
  auto *klass = GIMP_CLONE_LAYER_UNDO_GET_CLASS (undo);
  g_assert_true (GIMP_IS_CLONE_LAYER_UNDO_CLASS (klass));
  g_assert_true (GIMP_CLONE_LAYER_UNDO_CLASS (G_OBJECT_GET_CLASS (undo)) == klass);
  GTypeQuery query {};
  g_type_query (GIMP_TYPE_CLONE_LAYER_UNDO, &query);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpCloneLayerUndo));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpCloneLayerUndoClass));
}
extern "C" void gimp_test_clone_cpp_layout (gsize size, gsize offset, GimpCloneLayer *layer);
extern "C" void gimp_test_clone_cpp_layout (gsize size, gsize offset, GimpCloneLayer *layer)
{
  g_assert_cmpuint (sizeof (GimpDrawable), ==, size);
  g_assert_cmpuint (offsetof (GimpDrawable, priv), ==, offset);
  auto handle = GimpPainter::CloneLayerRef::retain (layer);
  auto created = GimpPainter::CloneLayerRef::create (gimp_item_get_image (GIMP_ITEM (layer)),
    nullptr, 2, 2, "C++ factory", 1.0, GIMP_LAYER_MODE_NORMAL);
  g_assert_true (GIMP_IS_CLONE_LAYER (created.get ()));
  g_assert_false (g_object_is_floating (created.get ()));
  auto adopted = GimpPainter::CloneLayerRef::adopt (GIMP_CLONE_LAYER (g_object_ref (layer)));
  auto sunk = GimpPainter::CloneLayerRef::sink (layer);
  static_assert (std::is_same<decltype (handle.source ()), GimpPainter::ObjectRef<GimpLayer>>::value,
                 "Clone source must have typed owning Layer semantics");
  g_assert_true (adopted.get () == layer);
  g_assert_true (sunk.get () == layer);
  handle.set_source_name ("unresolved via C++ handle");
  auto name = handle.source_name ();
  g_assert_cmpstr (name.get (), ==, "unresolved via C++ handle");
  g_assert_false (static_cast<bool> (handle.source ()));
  auto reference = handle.reference ();
  g_assert_cmpint (reference->state, ==, GIMP_CLONE_SOURCE_PENDING);
  handle.restore_reference (*reference);
  handle.set_source_name_with_undo ("undoable C++ name");
  g_assert_true (gimp_image_undo (gimp_item_get_image (GIMP_ITEM (layer))));
  name = handle.source_name ();
  g_assert_cmpstr (name.get (), ==, "unresolved via C++ handle");
}

extern "C" void gimp_test_clone_retained_image (GimpImage *image, GimpCloneLayer *clone);
extern "C" void gimp_test_clone_retained_image (GimpImage *image, GimpCloneLayer *clone)
{
  auto handle = GimpPainter::CloneLayerRef::retain (clone);
  auto source = handle.source ();
  g_assert_true (static_cast<bool> (source));
  g_object_unref (image);
  g_assert_null (gimp_item_get_image (GIMP_ITEM (handle.get ())));
  g_assert_null (gimp_item_get_image (GIMP_ITEM (source.get ())));
  g_assert_false (static_cast<bool> (handle.source ()));
  GError *error = nullptr;
  g_assert_false (gimp_clone_layer_set_source_full (handle.get (), nullptr, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
}
