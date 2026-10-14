/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded native entry/handle contracts. These do not claim historical runner,
 * serialization, enum, class-layout or one-shot projection equivalence. */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimpimage.h"
#include "core/gimplayer-new.h"
#include "painter/gimp-painter-binding.h"
void gimp_test_filter_entry_cpp_register (Gimp *application);
}
#include "core/gimpfilterlayer-handle.hpp"
#include "painter/filter-lifetime.hpp"
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

using GimpPainter::FilterLayerRef;
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().get_procedure ()),
                           GimpPainter::String>::value,
               "The original-name handle getter must own its string");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().get_procedure_args ()),
                           GimpPainter::ArrayRef>::value,
               "The original argument entry must return an owning GArray handle");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().mark_as_loaded ()),
                           void>::value,
               "The loaded-cache handle preserves the original void result");

namespace {
Gimp *application;
using ImageOwner = std::unique_ptr<GimpImage, decltype (&g_object_unref)>;
using ValueArrayOwner = std::unique_ptr<GimpValueArray, decltype (&gimp_value_array_unref)>;
using Bytes = std::vector<guchar>;
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
gint create_calls, name_calls, args_calls, loaded_calls;
#endif
}

#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
/* Linker probes count calls and forward to the real C providers. */
extern "C" GimpLayer *__real_gimp_filter_layer_new (GimpImage *, gint, gint,
                                                     const gchar *, gdouble, GimpLayerMode);
extern "C" const gchar *__real_gimp_filter_layer_get_procedure (GimpFilterLayer *);
extern "C" GArray *__real_gimp_filter_layer_get_procedure_args (GimpFilterLayer *);
extern "C" void __real_gimp_filter_layer_mark_as_loaded (GimpFilterLayer *);
extern "C" GimpLayer *__wrap_gimp_filter_layer_new (GimpImage *image, gint width, gint height,
                                                     const gchar *name, gdouble opacity, GimpLayerMode mode)
{
  g_atomic_int_inc (&create_calls);
  return __real_gimp_filter_layer_new (image, width, height, name, opacity, mode);
}
extern "C" const gchar *__wrap_gimp_filter_layer_get_procedure (GimpFilterLayer *filter)
{
  g_atomic_int_inc (&name_calls);
  return __real_gimp_filter_layer_get_procedure (filter);
}
extern "C" GArray *__wrap_gimp_filter_layer_get_procedure_args (GimpFilterLayer *filter)
{
  g_atomic_int_inc (&args_calls);
  return __real_gimp_filter_layer_get_procedure_args (filter);
}
extern "C" void __wrap_gimp_filter_layer_mark_as_loaded (GimpFilterLayer *filter)
{
  g_atomic_int_inc (&loaded_calls);
  __real_gimp_filter_layer_mark_as_loaded (filter);
}
#endif

namespace {
ImageOwner image_new (gint side = 8)
{
  return ImageOwner (gimp_image_new (application, side, side, GIMP_RGB,
                                    GIMP_PRECISION_U8_NON_LINEAR), g_object_unref);
}

FilterLayerRef filter_new (GimpImage *image, gint side = 8)
{
  return FilterLayerRef::create (image, side, side, "entry handle", 0.75,
                                  GIMP_LAYER_MODE_NORMAL_LEGACY);
}

void finalized (gpointer data, GObject *)
{ ++*static_cast<guint *> (data); }

template<class Function>
void expect_error (GimpPainterError code, Function operation)
{
  try { operation (); }
  catch (const GimpPainter::Error& error)
    { g_assert_cmpint (error.code (), ==, code); return; }
  catch (...) { g_error ("Unexpected exception from FilterLayer handle"); }
  g_error ("FilterLayer handle did not report expected error %d", code);
}

void spin_ms (gint milliseconds)
{
  const gint64 deadline = g_get_monotonic_time () + milliseconds * 1000;
  do { g_main_context_iteration (nullptr, FALSE); g_usleep (100); }
  while (g_get_monotonic_time () < deadline);
}

template<class Predicate>
void wait_until (Predicate done, const char *description)
{
  const gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (!done () && g_get_monotonic_time () < deadline)
    { g_main_context_iteration (nullptr, FALSE); g_usleep (100); }
  if (!done ()) g_error ("Timed out waiting for %s", description);
}

void drain_workers ()
{
  wait_until ([] { return GimpPainter::FilterLifetime::pending () == 0; },
              "bounded filter workers to drain");
}

void settle (const FilterLayerRef& filter)
{
  wait_until ([&] {
    if (filter.state () == GIMP_FILTER_LAYER_FAILED)
      {
        GimpPainter::String message (gimp_filter_layer_dup_error (filter.get ()));
        g_error ("Filter failed: %s", message.get ());
      }
    return filter.state () == GIMP_FILTER_LAYER_CLEAN;
  }, "a clean native filter cache");
  drain_workers ();
}

void fill (GimpLayer *layer, const guchar (&rgba)[4])
{
  auto *color = gegl_color_new (nullptr);
  gegl_color_set_pixel (color, babl_format ("R'G'B'A u8"), rgba);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), nullptr, color);
  g_object_unref (color);
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0,
                        gimp_item_get_width (GIMP_ITEM (layer)),
                        gimp_item_get_height (GIMP_ITEM (layer)));
}

Bytes pixels (GimpFilterLayer *layer)
{
  const gint width = gimp_item_get_width (GIMP_ITEM (layer));
  const gint height = gimp_item_get_height (GIMP_ITEM (layer));
  const GeglRectangle rect = {0, 0, width, height};
  Bytes result (gsize (width) * height * 4);
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), &rect,
                   1.0, babl_format ("R'G'B'A u8"), result.data (),
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  return result;
}

void assert_pixels (GimpFilterLayer *layer, const Bytes& expected)
{
  const auto actual = pixels (layer);
  g_assert_cmpmem (actual.data (), actual.size (), expected.data (), expected.size ());
}

void assert_black_result (GimpFilterLayer *layer)
{
  const auto actual = pixels (layer);
  for (gsize i = 0; i < actual.size (); i += 4)
    {
      g_assert_cmpuint (actual[i], ==, 0);
      g_assert_cmpuint (actual[i + 1], ==, 0);
      g_assert_cmpuint (actual[i + 2], ==, 0);
      g_assert_cmpuint (actual[i + 3], ==, 255);
    }
}

ValueArrayOwner edge_args ()
{
  return ValueArrayOwner (gimp_value_array_new_from_types (nullptr,
    G_TYPE_INT, 1, G_TYPE_INT, 123, G_TYPE_INT, 456, G_TYPE_DOUBLE, 2.0,
    G_TYPE_INT, 2, G_TYPE_INT, 0, G_TYPE_NONE), gimp_value_array_unref);
}

void attach_source (GimpImage *image, gint side)
{
  auto *source = gimp_layer_new (image, side, side, babl_format ("R'G'B'A u8"),
                                "entry source", 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (image, source, nullptr, 0, FALSE));
  const guchar source_rgba[] = {42, 93, 177, 255};
  fill (source, source_rgba);
}

void define_edge (const FilterLayerRef& filter)
{
  const auto args = edge_args ();
  filter.set_definition ("plug-in-edge", nullptr, args.get ());
}

void original_entry_routes ()
{
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  auto image = image_new ();
  g_atomic_int_set (&create_calls, 0);
  auto filter = filter_new (image.get ());
  g_assert_cmpint (g_atomic_int_get (&create_calls), ==, 1);
  define_edge (filter);
  g_atomic_int_set (&name_calls, 0);
  auto name = filter.get_procedure ();
  g_assert_cmpint (g_atomic_int_get (&name_calls), ==, 1);
  g_assert_cmpstr (name.get (), ==, "plug-in-edge");
  g_atomic_int_set (&args_calls, 0);
  auto args = filter.get_procedure_args ();
  g_assert_cmpint (g_atomic_int_get (&args_calls), ==, 1);
  g_assert_nonnull (args.get ());
  g_assert_cmpuint (args.get ()->len, ==, 6);
  g_atomic_int_set (&loaded_calls, 0);
  filter.mark_as_loaded ();
  g_assert_cmpint (g_atomic_int_get (&loaded_calls), ==, 1);
  g_assert_cmpint (filter.state (), ==, GIMP_FILTER_LAYER_CLEAN);
#else
  g_test_skip ("Original-entry route probe requires supported linker --wrap");
#endif
}

void native_type_factory_and_copy ()
{
  auto image = image_new ();
  g_assert_cmpuint (GimpPainter::TypeTraits<GimpFilterLayer>::type (), ==,
                    gimp_filter_layer_get_type ());
  g_assert_cmpuint (g_type_parent (GimpPainter::TypeTraits<GimpFilterLayer>::type ()),
                    ==, GIMP_TYPE_LAYER);
  auto *floating = gimp_filter_layer_new (image.get (), 3, 5, "floating C", 0.25,
                                          GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_nonnull (floating);
  g_assert_true (g_object_is_floating (floating));
  guint c_finalized = 0;
  g_object_weak_ref (G_OBJECT (floating), finalized, &c_finalized);
  {
    auto sunk = FilterLayerRef::sink (GIMP_FILTER_LAYER (floating));
    g_assert_false (g_object_is_floating (sunk.get ()));
  }
  g_assert_cmpuint (c_finalized, ==, 1);
  guint owner_finalized = 0;
  {
    auto filter = filter_new (image.get ());
    g_object_weak_ref (G_OBJECT (filter.get ()), finalized, &owner_finalized);
    g_assert_false (g_object_is_floating (filter.get ()));
    g_assert_cmpuint (G_OBJECT_TYPE (filter.get ()), ==, GIMP_TYPE_FILTER_LAYER);
    g_assert_cmpfloat (gimp_layer_get_opacity (GIMP_LAYER (filter.get ())), ==, 0.75);
    g_assert_cmpint (gimp_layer_get_mode (GIMP_LAYER (filter.get ())), ==,
                     GIMP_LAYER_MODE_NORMAL_LEGACY);
    g_assert_cmpstr (gimp_object_get_name (filter.get ()), ==, "entry handle");
    auto copy = filter;
    auto assigned = filter_new (image.get ());
    assigned = copy;
    auto moved = std::move (assigned);
    g_assert_null (assigned.get ());
    g_assert_true (moved.get () == filter.get ());
    g_assert_true (copy.get () == filter.get ());
    g_assert_cmpuint (owner_finalized, ==, 0);
  }
  g_assert_cmpuint (owner_finalized, ==, 1);
  auto fallback = FilterLayerRef::create (image.get (), 0, -1, "native fallback", 1.0,
                                          GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (fallback.get ())), ==, 8);
  g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (fallback.get ())), ==, 8);
  expect_error (GIMP_PAINTER_ERROR_WRONG_TYPE, [] { FilterLayerRef::adopt (nullptr); });
  auto *wrong = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  expect_error (GIMP_PAINTER_ERROR_WRONG_TYPE, [&] {
    FilterLayerRef::retain (reinterpret_cast<GimpFilterLayer *> (wrong));
  });
  g_object_unref (wrong);
  g_test_expect_message (nullptr, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_IMAGE*failed*");
  expect_error (GIMP_PAINTER_ERROR_INVALID_STATE, [] {
    FilterLayerRef::create (nullptr, 8, 8, "invalid image", 1.0,
                            GIMP_LAYER_MODE_NORMAL_LEGACY);
  });
  g_test_assert_expected_messages ();
}

void independent_image_handle_owners ()
{
  for (bool image_first : {false, true})
    {
      guint image_finalized = 0, filter_finalized = 0;
      auto image = image_new ();
      g_object_weak_ref (G_OBJECT (image.get ()), finalized, &image_finalized);
      {
        auto filter = filter_new (image.get ());
        g_object_weak_ref (G_OBJECT (filter.get ()), finalized, &filter_finalized);
        g_assert_true (gimp_image_add_layer (image.get (), GIMP_LAYER (filter.get ()),
                                            nullptr, 0, FALSE));
        g_assert_false (g_object_is_floating (filter.get ()));
        if (image_first)
          {
            image.reset ();
            g_assert_cmpuint (image_finalized, ==, 1);
            g_assert_cmpuint (filter_finalized, ==, 0);
            g_assert_null (gimp_item_get_image (GIMP_ITEM (filter.get ())));
            g_assert_false (gimp_item_is_attached (GIMP_ITEM (filter.get ())));
            g_assert_cmpint (filter.state (), ==, GIMP_FILTER_LAYER_CLOSED);
          }
      }
      if (!image_first)
        {
          g_assert_cmpuint (image_finalized, ==, 0);
          g_assert_cmpuint (filter_finalized, ==, 0);
          image.reset ();
        }
      g_assert_cmpuint (image_finalized, ==, 1);
      g_assert_cmpuint (filter_finalized, ==, 1);
    }
}

void owned_queries_outlive_definition_and_owner ()
{
  GimpPainter::String saved_name;
  GimpPainter::ArrayRef saved_args, shared_args;
  guint filter_finalized = 0;
  {
    auto image = image_new ();
    auto filter = filter_new (image.get ());
    g_object_weak_ref (G_OBJECT (filter.get ()), finalized, &filter_finalized);
    ValueArrayOwner input (gimp_value_array_new_from_types (nullptr,
      G_TYPE_STRING, "independent value", G_TYPE_INT, 37, G_TYPE_NONE),
      gimp_value_array_unref);
    filter.set_definition ("future exact-name", nullptr, input.get ());
    saved_name = filter.get_procedure ();
    saved_args = filter.get_procedure_args ();
    auto other = filter.get_procedure_args ();
    g_assert_nonnull (saved_args.get ());
    g_assert_nonnull (other.get ());
    g_assert_true (saved_args.get () != other.get ());
    g_assert_true (saved_args.get ()->data != other.get ()->data);
    g_value_set_string (&g_array_index (other.get (), GValue, 0), "other caller edit");
    g_value_set_string (gimp_value_array_index (input.get (), 0), "input edit");
    input.reset ();
    shared_args = saved_args;
    g_assert_true (shared_args.get () == saved_args.get ());
    filter.set_definition ("replacement", nullptr, nullptr);
    auto modern = filter.procedure ();
    auto named = filter.get_procedure ();
    g_assert_cmpstr (modern.get (), ==, "replacement");
    g_assert_cmpstr (named.get (), ==, "replacement");
    g_assert_null (filter.get_procedure_args ().get ());
    g_assert_true (gimp_painter_binding_close (G_OBJECT (filter.get ()), nullptr));
    auto closed_name = filter.get_procedure ();
    g_assert_cmpstr (closed_name.get (), ==, "replacement");
    g_assert_cmpstr (saved_name.get (), ==, "future exact-name");
  }
  g_assert_cmpuint (filter_finalized, ==, 1);
  g_assert_cmpstr (saved_name.get (), ==, "future exact-name");
  g_assert_cmpuint (saved_args.get ()->len, ==, 2);
  g_assert_cmpstr (g_value_get_string (&g_array_index (saved_args.get (), GValue, 0)),
                   ==, "independent value");
  saved_args.reset ();
  g_assert_cmpint (g_value_get_int (&g_array_index (shared_args.get (), GValue, 1)), ==, 37);
  shared_args.reset ();
}

void definition_mark_idle_then_rerun ()
{
  auto image = image_new ();
  attach_source (image.get (), 8);
  auto filter = filter_new (image.get ());
  g_assert_true (gimp_image_add_layer (image.get (), GIMP_LAYER (filter.get ()), nullptr, 0, FALSE));
  define_edge (filter);
  const auto revision = gimp_filter_layer_get_definition_revision (filter.get ());
  const guchar cached_rgba[] = {17, 29, 53, 123};
  fill (GIMP_LAYER (filter.get ()), cached_rgba);
  const auto saved = pixels (filter.get ());
  auto *cache = gimp_drawable_get_buffer (GIMP_DRAWABLE (filter.get ()));
  filter.mark_as_loaded ();
  const auto generation = gimp_filter_layer_get_generation (filter.get ());
  filter.mark_as_loaded ();
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter.get ()), >, generation);
  spin_ms (50);
  assert_pixels (filter.get (), saved);
  g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter.get ())));
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, 0);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter.get ()), ==, revision);
  GimpFilterLayerSnapshot snapshot = {};
  g_assert_true (gimp_filter_layer_get_snapshot_state (filter.get (), &snapshot));
  g_assert_true (snapshot.cache_complete);
  g_assert_cmpuint (snapshot.generation, ==, snapshot.cache_generation);
  gimp_filter_layer_invalidate (filter.get ());
  settle (filter);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, 1);
  assert_black_result (filter.get ());
  filter.mark_as_loaded ();
  spin_ms (30);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, 1);
}

struct PhaseMark
{
  const FilterLayerRef *filter;
  GimpFilterLayerState phase;
  bool called = false;
  guint64 runs = 0;
  GimpFilterLayerSnapshot marked = {};
};

void mark_in_phase (GimpFilterLayer *layer, gpointer data)
{
  auto& state = *static_cast<PhaseMark *> (data);
  if (state.called || state.filter->state () != state.phase) return;
  state.called = true;
  state.runs = gimp_filter_layer_get_run_count (layer);
  state.filter->mark_as_loaded ();
  g_assert_true (gimp_filter_layer_get_snapshot_state (layer, &state.marked));
}

void count_update (GimpDrawable *, gint, gint, gint, gint, gpointer data)
{ ++*static_cast<guint *> (data); }

void mark_cancels_all_native_phases ()
{
  const GimpFilterLayerState phases[] = {GIMP_FILTER_LAYER_WAITING,
    GIMP_FILTER_LAYER_PREPARING, GIMP_FILTER_LAYER_RUNNING, GIMP_FILTER_LAYER_IMPORTING};
  for (auto phase : phases)
    {
      /* 256 KiB input, no spill. Two or more bounded import quanta ensure
       * IMPORTING is observable before the completed buffer is published. */
      auto image = image_new (256);
      attach_source (image.get (), 256);
      auto filter = filter_new (image.get (), 256);
      g_assert_true (gimp_image_add_layer (image.get (), GIMP_LAYER (filter.get ()), nullptr, 0, FALSE));
      define_edge (filter);
      const guchar cached_rgba[] = {17, 29, 53, 123};
      fill (GIMP_LAYER (filter.get ()), cached_rgba);
      filter.mark_as_loaded ();
      const auto saved = pixels (filter.get ());
      auto *cache = gimp_drawable_get_buffer (GIMP_DRAWABLE (filter.get ()));
      guint updates = 0;
      PhaseMark state {&filter, phase};
      g_signal_connect (filter.get (), "filter-state-changed", G_CALLBACK (mark_in_phase), &state);
      g_signal_connect (filter.get (), "update", G_CALLBACK (count_update), &updates);
      gimp_filter_layer_invalidate (filter.get ());
      /* WAITING is also checked explicitly, independent of signal timing. */
      mark_in_phase (filter.get (), &state);
      wait_until ([&] { return state.called; }, "the requested native mark phase");
      g_test_message ("mark phase=%d starts=%" G_GUINT64_FORMAT, phase, state.runs);
      g_assert_true (state.marked.cache_complete);
      g_assert_cmpuint (state.marked.generation, ==, state.marked.cache_generation);
      settle (filter);
      spin_ms (30);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, state.runs);
      g_assert_cmpuint (updates, ==, 0);
      g_assert_true (cache == gimp_drawable_get_buffer (GIMP_DRAWABLE (filter.get ())));
      assert_pixels (filter.get (), saved);
      g_signal_handlers_disconnect_by_data (filter.get (), &state);
      g_signal_handlers_disconnect_by_data (filter.get (), &updates);
      gimp_filter_layer_invalidate (filter.get ());
      settle (filter);
      g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, state.runs + 1);
      assert_black_result (filter.get ());
    }
}

struct WrongThreadCall
{
  const FilterLayerRef *filter;
  bool rejected = false;
};

gpointer wrong_thread_entries (gpointer data)
{
  auto& call = *static_cast<WrongThreadCall *> (data);
  /* Only checked entry calls here. No main-loop pumping, GTK/GEGL access,
   * construction, ref release or finalization occurs on the worker. */
  try
    {
      auto name = call.filter->get_procedure ();
      auto args = call.filter->get_procedure_args ();
      call.filter->mark_as_loaded ();
      call.rejected = !name && !args.get ();
    }
  catch (...) { call.rejected = false; }
  return nullptr;
}

void closed_and_wrong_thread_containment ()
{
  auto image = image_new ();
  auto filter = filter_new (image.get ());
  define_edge (filter);
  const auto generation = gimp_filter_layer_get_generation (filter.get ());
  const auto revision = gimp_filter_layer_get_definition_revision (filter.get ());
  WrongThreadCall call {&filter};
  auto *thread = g_thread_new ("filter-entry-rejection", wrong_thread_entries, &call);
  g_thread_join (thread);
  g_assert_true (call.rejected);
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter.get ()), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (filter.get ()), ==, revision);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (filter.get ()), nullptr));
  const auto closed_generation = gimp_filter_layer_get_generation (filter.get ());
  filter.mark_as_loaded ();
  filter.mark_as_loaded ();
  g_assert_cmpint (filter.state (), ==, GIMP_FILTER_LAYER_CLOSED);
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter.get ()), ==, closed_generation);
  auto closed_name = filter.get_procedure ();
  g_assert_cmpstr (closed_name.get (), ==, "plug-in-edge");
  auto args = filter.get_procedure_args ();
  g_assert_nonnull (args.get ());
  g_assert_cmpuint (args.get ()->len, ==, 6);
  spin_ms (30);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, 0);
}

void running_owner_close_keeps_retained_cache ()
{
  auto image = image_new (256);
  attach_source (image.get (), 256);
  auto filter = filter_new (image.get (), 256);
  g_assert_true (gimp_image_add_layer (image.get (), GIMP_LAYER (filter.get ()), nullptr, 0, FALSE));
  define_edge (filter);
  const guchar cached_rgba[] = {11, 22, 33, 255};
  fill (GIMP_LAYER (filter.get ()), cached_rgba);
  filter.mark_as_loaded ();
  const auto saved = pixels (filter.get ());
  guint updates = 0;
  g_signal_connect (filter.get (), "update", G_CALLBACK (count_update), &updates);
  gimp_filter_layer_invalidate (filter.get ());
  wait_until ([&] { return filter.state () == GIMP_FILTER_LAYER_RUNNING; },
              "a running bounded filter before image close");
  const auto runs = gimp_filter_layer_get_run_count (filter.get ());
  image.reset ();
  g_assert_null (gimp_item_get_image (GIMP_ITEM (filter.get ())));
  g_assert_cmpint (filter.state (), ==, GIMP_FILTER_LAYER_CLOSED);
  const auto after_close = updates;
  const auto generation = gimp_filter_layer_get_generation (filter.get ());
  filter.mark_as_loaded ();
  drain_workers ();
  spin_ms (30);
  g_assert_cmpuint (updates, ==, after_close);
  g_assert_cmpuint (gimp_filter_layer_get_generation (filter.get ()), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (filter.get ()), ==, runs);
  auto closed_name = filter.get_procedure ();
  g_assert_cmpstr (closed_name.get (), ==, "plug-in-edge");
  assert_pixels (filter.get (), saved);
  g_signal_handlers_disconnect_by_data (filter.get (), &updates);
}
}

extern "C" void gimp_test_filter_entry_cpp_register (Gimp *gimp)
{
  application = gimp;
  g_test_add_func ("/filter-entry/cpp/original_entry_routes", original_entry_routes);
  g_test_add_func ("/filter-entry/cpp/native_type_factory_and_copy", native_type_factory_and_copy);
  g_test_add_func ("/filter-entry/cpp/independent_image_handle_owners", independent_image_handle_owners);
  g_test_add_func ("/filter-entry/cpp/owned_queries_outlive_definition_and_owner", owned_queries_outlive_definition_and_owner);
  g_test_add_func ("/filter-entry/cpp/definition_mark_idle_then_rerun", definition_mark_idle_then_rerun);
  g_test_add_func ("/filter-entry/cpp/mark_cancels_all_native_phases", mark_cancels_all_native_phases);
  g_test_add_func ("/filter-entry/cpp/closed_and_wrong_thread_containment", closed_and_wrong_thread_containment);
  g_test_add_func ("/filter-entry/cpp/running_owner_close_keeps_retained_cache", running_owner_close_keeps_retained_cache);
}
