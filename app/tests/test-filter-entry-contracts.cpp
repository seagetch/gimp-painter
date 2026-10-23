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
#include "core/gimp.h"
#include "core/gimp-contexts.h"
#include "gegl/gimp-gegl.h"
#include "core/gimpparamspecs.h"
#include "pdb/gimppdb.h"
#include "pdb/gimppdbcontext.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimpplugindef.h"
#include "plug-in/gimppluginmanager.h"
#include "plug-in/gimppluginprocedure.h"
#include "core/gimpfilterparametereditor.h"
#include "core/gimplayer-new.h"
#include "painter/gimp-painter-binding.h"
void gimp_test_filter_entry_cpp_register (Gimp *application);
void gimp_test_filter_entry_assert_blinds_provider (Gimp *application);
void gimp_test_filter_entry_provider_rejections (Gimp *application, GimpFilterLayer *filter);
void gimp_test_filter_entry_assert_legacy_rejected (GimpFilterLayer *filter);
}
#include "core/gimpfilterlayer-handle.hpp"
#include "painter/filter-lifetime.hpp"
#include "painter/binding-store.hpp"
#include "core/gimpfilterparametereditor.hpp"
#include "core/gimpfilterpaths.hpp"
#include <cstring>
#include <cstdlib>
#include <memory>
#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

using GimpPainter::FilterLayerRef;
static_assert (std::is_same<decltype (&gimp_filter_layer_set_legacy_procedure),
                           gboolean (*) (GimpFilterLayer *, const gchar *, const GArray *, GError **)>::value,
               "The bounded replacement has one checked C entry");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().try_set_legacy_procedure (
                             nullptr, nullptr, nullptr)), gboolean>::value,
               "The named try method preserves gboolean");
static_assert (noexcept (std::declval<const FilterLayerRef&> ().try_set_legacy_procedure (
                          nullptr, nullptr, nullptr)), "The try method cannot throw");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().set_legacy_procedure (
                             nullptr, nullptr)), void>::value,
               "The throwing named replacement has a void result");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().get_procedure ()),
                           GimpPainter::String>::value,
               "The original-name handle getter must own its string");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().get_procedure_args ()),
                           GimpPainter::ArrayRef>::value,
               "The original argument entry must return an owning GArray handle");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().mark_as_loaded ()),
                           void>::value,
               "The loaded-cache handle preserves the original void result");

static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().procedure_prefix (128)),
                           GimpPainter::String>::value, "The bounded prefix owns its bytes");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().definition ()),
                           FilterLayerRef::Definition>::value, "Definition reads own a native GBytes reference");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().snapshot_arguments ()),
                           FilterLayerRef::ArgumentsSnapshot>::value, "Snapshots use their native owning handle");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().arguments ()),
                           FilterLayerRef::Arguments>::value, "Materialized arguments own GimpValueArray, not GArray");
static_assert (std::is_same<decltype (std::declval<const FilterLayerRef&> ().definition_revision ()),
                           guint64>::value, "Revision reads preserve the native integer type");
static_assert (!std::is_copy_constructible<FilterLayerRef::ArgumentsSnapshot>::value,
               "Native snapshots must not be shallow-copied");

namespace {
Gimp *application;
using ImageOwner = std::unique_ptr<GimpImage, decltype (&g_object_unref)>;
using ValueArrayOwner = std::unique_ptr<GimpValueArray, decltype (&gimp_value_array_unref)>;
using Bytes = std::vector<guchar>;
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
gint create_calls, name_calls, args_calls, loaded_calls, legacy_calls;
#endif
}

#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
/* Linker probes count calls and forward to the real C providers. */
extern "C" GimpLayer *__real_gimp_filter_layer_new (GimpImage *, gint, gint,
                                                     const gchar *, gdouble, GimpLayerMode);
extern "C" const gchar *__real_gimp_filter_layer_get_procedure (GimpFilterLayer *);
extern "C" GArray *__real_gimp_filter_layer_get_procedure_args (GimpFilterLayer *);
extern "C" void __real_gimp_filter_layer_mark_as_loaded (GimpFilterLayer *);
extern "C" gboolean __real_gimp_filter_layer_set_legacy_procedure (GimpFilterLayer *, const gchar *, const GArray *, GError **);
extern "C" gboolean __wrap_gimp_filter_layer_set_legacy_procedure (GimpFilterLayer *filter,
  const gchar *name, const GArray *args, GError **error)
{
  g_atomic_int_inc (&legacy_calls);
  return __real_gimp_filter_layer_set_legacy_procedure (filter, name, args, error);
}
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

void retire_startup_plugins ()
{
  /* Successful query children can be closed yet still own the restore PDB
   * context through their low-priority child-watch source. Its stroke options
   * also borrow Gimp. Retire those unrelated startup owners before testing the
   * synchronous surrender of every application reference inside an entry.
   * Observe destruction without taking references or dispatching in the entry. */
  guint observed = 0, retired = 0;
  for (GList *list = application->context_list; list; list = list->next)
    if (GIMP_IS_PDB_CONTEXT (list->data))
      {
        auto *context = GIMP_PDB_CONTEXT (list->data);
        g_assert_nonnull (context->stroke_options);
        g_object_weak_ref (G_OBJECT (context), finalized, &retired);
        g_object_weak_ref (G_OBJECT (context->stroke_options), finalized, &retired);
        observed += 2;
      }
  gimp_plug_in_manager_exit (application->plug_in_manager);
  g_assert_null (application->plug_in_manager->open_plug_ins);
  wait_until ([&] { return retired == observed; },
              "startup plug-in contexts and stroke options to retire");
  g_assert_null (application->plug_in_manager->open_plug_ins);
  for (GList *list = application->context_list; list; list = list->next)
    g_assert_false (GIMP_IS_PDB_CONTEXT (list->data));
  gimp_test_filter_entry_assert_blinds_provider (application);
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

void bounded_dialog_read_owners ()
{
  FilterLayerRef::Definition saved_raw;
  FilterLayerRef::ArgumentsSnapshot saved_snapshot;
  FilterLayerRef::Arguments saved_args;
  GimpPainter::String saved_prefix;
  guint filter_finalized = 0;
  const std::string name = std::string (127, 'x') + "\xe2\x82\xac" + std::string (5000, 'y');
  {
    auto image = image_new ();
    auto filter = filter_new (image.get ());
    g_object_weak_ref (G_OBJECT (filter.get ()), finalized, &filter_finalized);
    g_assert_null (filter.definition ().get ());
    g_assert_null (filter.arguments ().get ());
    g_assert_null (filter.snapshot_arguments ().get ());
    FilterLayerRef::Definition raw (g_bytes_new_static ("raw\0bytes", 9));
    ValueArrayOwner input (gimp_value_array_new_from_types (nullptr,
      G_TYPE_STRING, "saved scalar", G_TYPE_INT, 37, G_TYPE_NONE), gimp_value_array_unref);
    const auto before = filter.definition_revision ();
    filter.set_definition (name.c_str (), raw.get (), input.get ());
    g_assert_cmpuint (filter.definition_revision (), >, before);
    const auto revision = filter.definition_revision ();
    gboolean truncated = FALSE;
    saved_prefix = filter.procedure_prefix (128, &truncated);
    g_assert_true (truncated);
    g_assert_cmpuint (strlen (saved_prefix.get ()), ==, 128);
    g_assert_cmpmem (saved_prefix.get (), 128, name.data (), 128);
    g_assert_false (g_utf8_validate (saved_prefix.get (), -1, nullptr));
    for (gsize limit : {gsize (0), gsize (127), gsize (4096), name.size (), name.size () + 1})
      {
        auto prefix = filter.procedure_prefix (limit, &truncated);
        const auto size = std::min (limit, name.size ());
        g_assert_cmpuint (strlen (prefix.get ()), ==, size);
        g_assert_cmpint (truncated, ==, limit < name.size ());
        g_assert_cmpmem (prefix.get (), size, name.data (), size);
      }
    saved_raw = filter.definition ();
    saved_snapshot = filter.snapshot_arguments ();
    saved_args = filter.arguments ();
    auto independent = filter.arguments ();
    g_assert_true (independent.get () != saved_args.get ());
    g_value_set_string (gimp_value_array_index (independent.get (), 0), "caller edit");
    g_assert_cmpstr (g_value_get_string (gimp_filter_arguments_snapshot_peek_value (saved_snapshot.get (), 0)), ==, "saved scalar");
    g_assert_cmpuint (filter.definition_revision (), ==, revision);
    FilterLayerRef::Definition empty_raw (g_bytes_new_static ("", 0));
    ValueArrayOwner empty_args (gimp_value_array_new (0), gimp_value_array_unref);
    filter.set_definition ("", empty_raw.get (), empty_args.get ());
    g_assert_nonnull (filter.definition ().get ());
    g_assert_cmpuint (g_bytes_get_size (filter.definition ().get ()), ==, 0);
    g_assert_nonnull (filter.arguments ().get ());
    g_assert_cmpuint (gimp_value_array_length (filter.arguments ().get ()), ==, 0);
    g_assert_nonnull (filter.snapshot_arguments ().get ());
    g_assert_cmpuint (gimp_filter_arguments_snapshot_count (filter.snapshot_arguments ().get ()), ==, 0);
    auto empty_name = filter.procedure_prefix (0, &truncated);
    g_assert_nonnull (empty_name.get ());
    g_assert_cmpstr (empty_name.get (), ==, "");
    g_assert_false (truncated);
    filter.set_definition ("replacement", nullptr, nullptr);
    g_assert_null (filter.definition ().get ());
    g_assert_null (filter.arguments ().get ());
    g_assert_null (filter.snapshot_arguments ().get ());
    filter.set_definition (name.c_str (), saved_raw.get (), saved_args.get ());
    const auto closed_revision = filter.definition_revision ();
    g_assert_true (gimp_painter_binding_close (G_OBJECT (filter.get ()), nullptr));
    g_assert_cmpuint (filter.definition_revision (), ==, closed_revision);
    auto closed_prefix = filter.procedure_prefix (128);
    g_assert_cmpmem (closed_prefix.get (), 128, name.data (), 128);
    g_assert_true (g_bytes_equal (filter.definition ().get (), saved_raw.get ()));
    g_assert_cmpuint (gimp_filter_arguments_snapshot_count (filter.snapshot_arguments ().get ()), ==, 2);
    g_assert_cmpuint (gimp_value_array_length (filter.arguments ().get ()), ==, 2);
  }
  g_assert_cmpuint (filter_finalized, ==, 1);
  g_assert_cmpmem (saved_prefix.get (), 128, name.data (), 128);
  g_assert_cmpmem (g_bytes_get_data (saved_raw.get (), nullptr), 9, "raw\0bytes", 9);
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (saved_snapshot.get ()), ==, 2);
  g_assert_cmpstr (g_value_get_string (gimp_filter_arguments_snapshot_peek_value (saved_snapshot.get (), 0)), ==, "saved scalar");
  g_assert_cmpstr (g_value_get_string (gimp_value_array_index (saved_args.get (), 0)), ==, "saved scalar");
  g_assert_cmpint (g_value_get_int (gimp_value_array_index (saved_args.get (), 1)), ==, 37);
  auto moved = std::move (saved_snapshot);
  g_assert_null (saved_snapshot.get ());
  g_assert_cmpuint (gimp_filter_arguments_snapshot_count (moved.get ()), ==, 2);
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
/* Reuse the registered-only fixture pattern from test-filter-editor-metadata.
 * All substitutions are synchronous and restored before any loop dispatch. */
using Object = GimpPainter::ObjectRef<GObject>;
struct Registry {
  explicit Registry (Gimp *app)
    : app (app), original_manager (app->plug_in_manager), original_pdb (app->pdb),
      manager (Object::adopt (G_OBJECT (gimp_plug_in_manager_new (app)))),
      pdb (Object::adopt (G_OBJECT (gimp_pdb_new (app))))
    { app->plug_in_manager = GIMP_PLUG_IN_MANAGER (manager.get ()); app->pdb = GIMP_PDB (pdb.get ()); }
  ~Registry () { app->plug_in_manager = original_manager; app->pdb = original_pdb; }
  Gimp *app;
  GimpPlugInManager *original_manager;
  GimpPDB *original_pdb;
  Object manager, pdb;
  void attach (GimpProcedure *procedure)
  {
    auto *definition = gimp_plug_in_def_new (GIMP_PLUG_IN_PROCEDURE (procedure)->file);
    gimp_plug_in_def_add_procedure (definition, GIMP_PLUG_IN_PROCEDURE (procedure));
    app->plug_in_manager->plug_in_defs = g_slist_prepend (nullptr, definition);
    gimp_plug_in_manager_add_procedure (app->plug_in_manager, GIMP_PLUG_IN_PROCEDURE (procedure));
    gimp_filter_parameter_editor_capture (app->plug_in_manager);
    g_slist_free_full (app->plug_in_manager->plug_in_defs, g_object_unref);
    app->plug_in_manager->plug_in_defs = nullptr;
    gimp_pdb_register_procedure (app->pdb, procedure);
  }
  void remove (GimpProcedure *procedure)
  {
    gimp_pdb_unregister_procedure (app->pdb, procedure);
    app->plug_in_manager->plug_in_procedures = g_slist_remove (app->plug_in_manager->plug_in_procedures, procedure);
    g_object_unref (procedure);
  }
};

Object procedure (const char *path = nullptr)
{
  constexpr auto flags = static_cast<GParamFlags> (G_PARAM_READWRITE);
  const auto trusted = GimpPainter::filter_registered_plugin_path (GimpPainter::FilterProcedure::blinds);
  auto file = Object::adopt (G_OBJECT (g_file_new_for_path (path ? path : trusted.c_str ())));
  auto result = Object::adopt (G_OBJECT (gimp_plug_in_procedure_new (GIMP_PDB_PROC_TYPE_PLUGIN, G_FILE (file.get ()))));
  auto *p = GIMP_PROCEDURE (result.get ());
  gimp_object_set_static_name (GIMP_OBJECT (p), GimpPainter::FilterLegacy::blinds.execution);
  gimp_procedure_add_argument (p, g_param_spec_enum ("run-mode", "run-mode", "run-mode",
    GIMP_TYPE_RUN_MODE, GIMP_RUN_NONINTERACTIVE, flags));
  gimp_procedure_add_argument (p, gimp_param_spec_image ("image", "image", "image", FALSE, flags));
  gimp_procedure_add_argument (p, gimp_param_spec_core_object_array ("drawables", "drawables", "drawables",
    GIMP_TYPE_DRAWABLE, flags));
  gimp_procedure_add_argument (p, g_param_spec_int ("angle-displacement", "angle", "angle", 0, 90, 30, flags));
  gimp_procedure_add_argument (p, g_param_spec_int ("num-segments", "segments", "segments", 1, 1024, 3, flags));
  auto *choices = gimp_choice_new ();
  gimp_choice_add (choices, "horizontal", GIMP_ORIENTATION_HORIZONTAL, "Horizontal", "Horizontal");
  gimp_choice_add (choices, "vertical", GIMP_ORIENTATION_VERTICAL, "Vertical", "Vertical");
  gimp_procedure_add_argument (p, gimp_param_spec_choice ("orientation", "orientation", "orientation",
    choices, "horizontal", flags));
  gimp_procedure_add_argument (p, g_param_spec_boolean ("bg-transparent", "transparent", "transparent", FALSE, flags));
  return result;
}

void checked_entry_routes ()
{
  auto image = image_new ();
  auto filter = filter_new (image.get ());
  GError *error = nullptr;
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_atomic_int_set (&legacy_calls, 0);
#endif
  g_assert_true (filter.try_set_legacy_procedure ("plug-in-blinds", nullptr, &error));
  g_assert_no_error (error);
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_assert_cmpint (g_atomic_int_get (&legacy_calls), ==, 1);
  g_atomic_int_set (&legacy_calls, 0);
#endif
  filter.set_legacy_procedure ("plug-in-blinds", nullptr);
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_assert_cmpint (g_atomic_int_get (&legacy_calls), ==, 1);
  g_atomic_int_set (&legacy_calls, 0);
#endif
  g_assert_false (filter.try_set_legacy_procedure ("unsupported named handle", nullptr, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_clear_error (&error);
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_assert_cmpint (g_atomic_int_get (&legacy_calls), ==, 1);
  g_atomic_int_set (&legacy_calls, 0);
#endif
  expect_error (GIMP_PAINTER_ERROR_INVALID_STATE, [&] {
    filter.set_legacy_procedure ("unsupported named handle", nullptr);
  });
#ifdef GIMP_TEST_FILTER_WRAP_ENTRIES
  g_assert_cmpint (g_atomic_int_get (&legacy_calls), ==, 1);
#endif
}

void clear_legacy_value (gpointer data)
{
  auto *value = static_cast<GValue *> (data);
  if (G_IS_VALUE (value)) g_value_unset (value);
}
GimpPainter::ArrayRef checked_arguments ()
{
  auto result = GimpPainter::ArrayRef::adopt (g_array_sized_new (FALSE, TRUE, sizeof (GValue), 7));
  g_array_set_clear_func (result.get (), clear_legacy_value);
  g_array_set_size (result.get (), 7);
  for (guint i = 0; i < 7; ++i)
    {
      auto *value = &g_array_index (result.get (), GValue, i);
      g_value_init (value, G_TYPE_INT);
      g_value_set_int (value, i == 4 ? 3 : i == 0 ? 42 : 0);
    }
  return result;
}

struct NotifyReentry {
  GimpFilterLayer *filter;
  bool replace;
  bool entered = false;
  guint late = 0;
};
void notify_count (GObject *, GParamSpec *, gpointer data)
{
  auto& state = *static_cast<NotifyReentry *> (data);
  if (state.entered) ++state.late;
}
void notify_checked_reentry (GObject *, GParamSpec *, gpointer data)
{
  auto& state = *static_cast<NotifyReentry *> (data);
  if (state.entered) return;
  state.entered = true;
  if (state.replace)
    {
      auto newer = ValueArrayOwner (gimp_value_array_new_from_types (nullptr,
        G_TYPE_INT, 313, G_TYPE_NONE), gimp_value_array_unref);
      g_assert_true (gimp_filter_layer_set_definition (state.filter, "newer notification definition", nullptr, newer.get (), nullptr));
    }
  else g_assert_true (gimp_painter_binding_close (G_OBJECT (state.filter), nullptr));
}
void checked_notify_reentry (bool replace)
{
  auto image = image_new ();
  auto filter = filter_new (image.get ());
  NotifyReentry state {filter.get (), replace};
  const auto revision = filter.definition_revision ();
  GError *error = nullptr;
  for (const char *signal : {"notify::filter-procedure", "notify::filter-arguments",
                             "notify::filter-original-definition", "notify::filter-opaque-arguments"})
    g_signal_connect (filter.get (), signal, G_CALLBACK (notify_count), &state);
  g_signal_connect (filter.get (), "notify::filter-procedure", G_CALLBACK (notify_checked_reentry), &state);
  g_assert_false (filter.try_set_legacy_procedure ("plug-in-blinds", nullptr, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_assert_nonnull (std::strstr (error->message, "notification"));
  g_clear_error (&error);
  g_assert_true (state.entered);
  if (replace)
    {
      g_assert_cmpuint (filter.definition_revision (), ==, revision + 2);
      auto name = filter.get_procedure ();
      g_assert_cmpstr (name.get (), ==, "newer notification definition");
      auto args = filter.get_procedure_args ();
      g_assert_cmpuint (args.get ()->len, ==, 1);
      g_assert_cmpint (g_value_get_int (&g_array_index (args.get (), GValue, 0)), ==, 313);
    }
  else
    {
      g_assert_cmpuint (state.late, ==, 0);
      g_assert_cmpint (filter.state (), ==, GIMP_FILTER_LAYER_CLOSED);
      auto name = filter.get_procedure ();
      g_assert_cmpstr (name.get (), ==, "plug-in-blinds");
    }
  g_signal_handlers_disconnect_by_data (filter.get (), &state);
}
void checked_notify_close () { checked_notify_reentry (false); }
void checked_notify_newer () { checked_notify_reentry (true); }

enum class TransformAction { staged, replace, close, manager, pdb, provider, corrupt, drop_refs, drop_active_refs };
struct TransformState {
  TransformAction action;
  GimpFilterLayer *filter;
  GimpImage *image;
  GArray *input;
  char *name;
  Registry *registry = nullptr;
  GimpProcedure *old_provider = nullptr;
  GimpProcedure *new_provider = nullptr;
  GimpPlugInManager *new_manager = nullptr;
  GimpPDB *new_pdb = nullptr;
  guint calls = 0, filter_finalized = 0, image_finalized = 0, app_finalized = 0;
};
TransformState *transform_state;
void reentrant_double_to_int (const GValue *source, GValue *destination)
{
  auto& state = *transform_state;
  ++state.calls;
  g_value_set_int (destination, static_cast<gint> (g_value_get_double (source)));
  g_assert_cmpuint (state.calls, ==, 1);
  /* These remain real live inputs throughout the call. Only their scalar
   * contents change, after the adapter was required to stage all five slots. */
  g_value_set_int (&g_array_index (state.input, GValue, 4), 99);
  std::strcpy (state.name, "caller changed");
  switch (state.action)
    {
    case TransformAction::staged: break;
    case TransformAction::replace:
      g_assert_true (gimp_filter_layer_set_definition (state.filter, "reentered transform definition", nullptr, nullptr, nullptr));
      break;
    case TransformAction::close:
      g_assert_true (gimp_painter_binding_close (G_OBJECT (state.filter), nullptr));
      break;
    case TransformAction::manager: application->plug_in_manager = state.new_manager; break;
    case TransformAction::pdb: application->pdb = state.new_pdb; break;
    case TransformAction::provider:
      state.registry->remove (state.old_provider);
      state.registry->attach (state.new_provider);
      break;
    case TransformAction::corrupt:
      g_value_unset (destination);
      g_value_init (destination, G_TYPE_STRING);
      g_value_set_static_string (destination, "wrong destination type");
      break;
    case TransformAction::drop_refs:
    case TransformAction::drop_active_refs:
      if (state.action == TransformAction::drop_refs) g_assert_true (gimp_painter_binding_close (G_OBJECT (state.filter), nullptr));
      if (state.action == TransformAction::drop_refs)
        { gimp_gegl_exit (application); gimp_contexts_exit (application); }
      g_object_unref (state.filter);
      g_object_unref (state.image);
      g_object_unref (application);
      g_assert_cmpuint (state.filter_finalized, ==, 0);
      g_assert_cmpuint (state.image_finalized, ==, 0);
      g_assert_cmpuint (state.app_finalized, ==, 0);
      break;
    }
}
void forbidden_enum_transform (const GValue *, GValue *)
{ g_error ("An unapproved or ignored source reached its registered transform"); }

void checked_transform_reentry ()
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (nullptr, 60 * G_TIME_SPAN_SECOND, G_TEST_SUBPROCESS_DEFAULT);
      g_test_trap_assert_passed ();
      return;
    }
  retire_startup_plugins ();
  g_value_register_transform_func (G_TYPE_DOUBLE, G_TYPE_INT, reentrant_double_to_int);
  g_value_register_transform_func (GIMP_TYPE_ORIENTATION_TYPE, G_TYPE_INT, forbidden_enum_transform);
  g_value_register_transform_func (G_TYPE_OBJECT, G_TYPE_INT, forbidden_enum_transform);
  g_value_register_transform_func (G_TYPE_BYTES, G_TYPE_INT, forbidden_enum_transform);
  for (auto action : {TransformAction::staged, TransformAction::replace, TransformAction::close,
                      TransformAction::manager, TransformAction::pdb, TransformAction::provider,
                      TransformAction::corrupt})
    {
      auto image = image_new ();
      auto filter = filter_new (image.get ());
      auto args = checked_arguments ();
      auto *first = &g_array_index (args.get (), GValue, 0);
      g_value_unset (first); g_value_init (first, G_TYPE_DOUBLE); g_value_set_double (first, 12.75);
      guint context_finalized = 0;
      if (action == TransformAction::staged)
        {
          auto *context = &g_array_index (args.get (), GValue, 1);
          auto *object = g_object_new (G_TYPE_OBJECT, nullptr);
          g_object_weak_ref (G_OBJECT (object), finalized, &context_finalized);
          g_value_unset (context); g_value_init (context, G_TYPE_OBJECT); g_value_take_object (context, object);
          context = &g_array_index (args.get (), GValue, 2);
          g_value_unset (context); g_value_init (context, G_TYPE_BYTES);
          g_value_take_boxed (context, g_bytes_new_static ("ignored context", 15));
        }
      char name[32] = "plug-in-blinds";
      Registry registry (application);
      auto owner = procedure ();
      registry.attach (GIMP_PROCEDURE (owner.get ()));
      auto replacement = procedure ();
      auto manager = Object::adopt (G_OBJECT (gimp_plug_in_manager_new (application)));
      auto pdb = Object::adopt (G_OBJECT (gimp_pdb_new (application)));
      filter.set_definition ("before transform", nullptr, nullptr);
      filter.mark_as_loaded ();
      const auto revision = filter.definition_revision ();
      const auto generation = gimp_filter_layer_get_generation (filter.get ());
      const auto binding_generation = GimpPainter::BindingStore::require (G_OBJECT (filter.get ())).generation ();
      TransformState state {action, filter.get (), image.get (), args.get (), name};
      state.registry = &registry;
      state.old_provider = GIMP_PROCEDURE (owner.get ());
      state.new_provider = GIMP_PROCEDURE (replacement.get ());
      state.new_manager = GIMP_PLUG_IN_MANAGER (manager.get ());
      state.new_pdb = GIMP_PDB (pdb.get ());
      transform_state = &state;
      GError *error = nullptr;
      const gboolean result = gimp_filter_layer_set_legacy_procedure (filter.get (), name, args.get (), &error);
      /* Restore registry identity before fixture teardown; never execute it. */
      application->plug_in_manager = GIMP_PLUG_IN_MANAGER (registry.manager.get ());
      application->pdb = GIMP_PDB (registry.pdb.get ());
      g_assert_cmpuint (state.calls, ==, 1);
      if (action == TransformAction::staged)
        {
          g_assert_true (result); g_assert_no_error (error);
          auto saved_name = filter.get_procedure ();
          g_assert_cmpstr (saved_name.get (), ==, "plug-in-blinds");
          auto saved = filter.get_procedure_args ();
          g_assert_cmpint (g_value_get_int (&g_array_index (saved.get (), GValue, 0)), ==, 12);
          g_assert_cmpint (g_value_get_int (&g_array_index (saved.get (), GValue, 4)), ==, 3);
        }
      else
        {
          g_assert_false (result);
          g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
          g_clear_error (&error);
          auto saved_name = filter.get_procedure ();
          if (action == TransformAction::replace)
            {
              g_assert_cmpstr (saved_name.get (), ==, "reentered transform definition");
              g_assert_cmpuint (filter.definition_revision (), ==, revision + 1);
            }
          else
            {
              g_assert_cmpstr (saved_name.get (), ==, "before transform");
              if (action == TransformAction::close)
                g_assert_cmpint (filter.state (), ==, GIMP_FILTER_LAYER_CLOSED);
              else
                {
                  g_assert_cmpuint (filter.definition_revision (), ==, revision);
                  g_assert_cmpuint (gimp_filter_layer_get_generation (filter.get ()), ==, generation);
                  g_assert_cmpuint (GimpPainter::BindingStore::require (G_OBJECT (filter.get ())).generation (), ==, binding_generation);
                }
            }
        }
      /* Even a registered custom conversion must not broaden the whitelist. */
      if (action == TransformAction::staged)
        {
          auto *value = &g_array_index (args.get (), GValue, 0);
          g_value_unset (value); g_value_init (value, GIMP_TYPE_ORIENTATION_TYPE);
          g_value_set_enum (value, GIMP_ORIENTATION_HORIZONTAL);
          g_assert_false (filter.try_set_legacy_procedure ("plug-in-blinds", args.get (), &error));
          g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
          g_clear_error (&error);
          args.reset ();
          g_assert_cmpuint (context_finalized, ==, 1);
        }
    }
  /* Last, use the actual restored owner and surrender every caller reference.
   * The subprocess exits before main can reuse its transferred application. */
  auto *image = image_new ().release ();
  auto *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image, 8, 8, "drop refs", 1.0, GIMP_LAYER_MODE_NORMAL));
  g_object_ref_sink (filter);
  auto args = checked_arguments ();
  auto *first = &g_array_index (args.get (), GValue, 0);
  g_value_unset (first); g_value_init (first, G_TYPE_DOUBLE); g_value_set_double (first, 12.75);
  char name[32] = "plug-in-blinds";
  TransformState state {TransformAction::drop_refs, filter, image, args.get (), name};
  g_object_weak_ref (G_OBJECT (filter), finalized, &state.filter_finalized);
  g_object_weak_ref (G_OBJECT (image), finalized, &state.image_finalized);
  g_object_weak_ref (G_OBJECT (application), finalized, &state.app_finalized);
  transform_state = &state;
  GError *error = nullptr;
  g_assert_false (gimp_filter_layer_set_legacy_procedure (filter, name, args.get (), &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_clear_error (&error);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_cmpuint (state.filter_finalized, ==, 1);
  g_assert_cmpuint (state.image_finalized, ==, 1);
  g_assert_cmpuint (state.app_finalized, ==, 1);
  args.reset ();
  std::exit (0);
}

struct RetiredReentry {
  GimpFilterLayer *filter;
  GimpImage *image;
  guint calls = 0, filter_finalized = 0, image_finalized = 0, app_finalized = 0;
};
void retired_checked_bytes (gpointer data)
{
  auto& state = *static_cast<RetiredReentry *> (data);
  ++state.calls;
  g_assert_cmpstr (gimp_filter_layer_get_procedure (state.filter), ==, "plug-in-blinds");
  g_assert_null (gimp_filter_layer_ref_definition (state.filter));
  g_assert_null (gimp_filter_layer_ref_opaque_arguments (state.filter));
  auto saved = GimpPainter::ArrayRef::adopt (gimp_filter_layer_get_procedure_args (state.filter));
  g_assert_cmpuint (saved.get ()->len, ==, 7);
  for (guint i = 0; i < 7; ++i)
    g_assert_cmpint (g_value_get_int (&g_array_index (saved.get (), GValue, i)), ==, 0);
  const auto revision = gimp_filter_layer_get_definition_revision (state.filter);
  g_assert_true (gimp_filter_layer_set_definition (state.filter, "retired newer definition", nullptr, nullptr, nullptr));
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (state.filter), ==, revision + 1);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (state.filter), ==, "retired newer definition");
  gimp_gegl_exit (application);
  gimp_contexts_exit (application);
  g_object_unref (state.filter);
  g_object_unref (state.image);
  g_object_unref (application);
  g_assert_cmpuint (state.filter_finalized, ==, 0);
  g_assert_cmpuint (state.image_finalized, ==, 0);
  g_assert_cmpuint (state.app_finalized, ==, 0);
}
void checked_retired_bytes_reentry ()
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (nullptr, 60 * G_TIME_SPAN_SECOND, G_TEST_SUBPROCESS_DEFAULT);
      g_test_trap_assert_passed ();
      return;
    }
  retire_startup_plugins ();
  auto *image = image_new ().release ();
  auto *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image, 8, 8, "retired refs", 1.0, GIMP_LAYER_MODE_NORMAL));
  g_object_ref_sink (filter);
  RetiredReentry state {filter, image};
  g_object_weak_ref (G_OBJECT (filter), finalized, &state.filter_finalized);
  g_object_weak_ref (G_OBJECT (image), finalized, &state.image_finalized);
  g_object_weak_ref (G_OBJECT (application), finalized, &state.app_finalized);
  auto *raw = g_bytes_new_with_free_func ("retired", 7, retired_checked_bytes, &state);
  auto *opaque = g_bytes_new_static ("old opaque", 10);
  g_assert_true (gimp_filter_layer_set_definition_with_opaque_arguments (filter, "old opaque", raw, opaque, nullptr));
  g_bytes_unref (raw); g_bytes_unref (opaque);
  g_assert_cmpuint (state.calls, ==, 0);
  GError *error = nullptr;
  /* Retirement follows publication and its notification result. Reentry here
   * may install newer data without changing the completed TRUE result. */
  g_assert_true (gimp_filter_layer_set_legacy_procedure (filter, "plug-in-blinds", nullptr, &error));
  g_assert_no_error (error);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_cmpuint (state.filter_finalized, ==, 1);
  g_assert_cmpuint (state.image_finalized, ==, 1);
  g_assert_cmpuint (state.app_finalized, ==, 1);
  std::exit (0);
}

struct RetiredModelReentry {
  TransformState *transform;
  guint calls = 0;
};
void retired_checked_model (gpointer data)
{
  auto& state = *static_cast<RetiredModelReentry *> (data);
  auto& refs = *state.transform;
  ++state.calls;
  /* The scalar callback already surrendered the caller's three references.
   * The captured old model must retire before any of those leases end. */
  g_assert_cmpuint (refs.calls, ==, 1);
  g_assert_cmpuint (refs.filter_finalized, ==, 0);
  g_assert_cmpuint (refs.image_finalized, ==, 0);
  g_assert_cmpuint (refs.app_finalized, ==, 0);
  g_assert_true (gimp_item_get_image (GIMP_ITEM (refs.filter)) == refs.image);
  g_assert_true (refs.image->gimp == application);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (refs.filter), ==, "plug-in-blinds");
  auto saved = GimpPainter::ArrayRef::adopt (gimp_filter_layer_get_procedure_args (refs.filter));
  g_assert_cmpuint (saved.get ()->len, ==, 7);
  g_assert_cmpint (g_value_get_int (&g_array_index (saved.get (), GValue, 0)), ==, 12);
  g_assert_cmpint (g_value_get_int (&g_array_index (saved.get (), GValue, 4)), ==, 3);
  g_assert_null (gimp_filter_layer_ref_definition (refs.filter));
  g_assert_null (gimp_filter_layer_ref_opaque_arguments (refs.filter));
  const auto revision = gimp_filter_layer_get_definition_revision (refs.filter);
  g_assert_true (gimp_filter_layer_set_definition (refs.filter, "retired model newer", nullptr, nullptr, nullptr));
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (refs.filter), ==, revision + 1);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (refs.filter), ==, "retired model newer");
  gimp_gegl_exit (application);
  gimp_contexts_exit (application);
}
void checked_retired_model_after_transform ()
{
  if (!g_test_subprocess ())
    {
      g_test_trap_subprocess (nullptr, 60 * G_TIME_SPAN_SECOND, G_TEST_SUBPROCESS_DEFAULT);
      g_test_trap_assert_passed ();
      return;
    }
  retire_startup_plugins ();
  auto *image = image_new ().release ();
  auto *filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image, 8, 8, "model refs", 1.0, GIMP_LAYER_MODE_NORMAL));
  g_object_ref_sink (filter);
  auto args = checked_arguments ();
  auto *first = &g_array_index (args.get (), GValue, 0);
  g_value_unset (first); g_value_init (first, G_TYPE_DOUBLE); g_value_set_double (first, 12.75);
  char name[32] = "plug-in-blinds";
  TransformState refs {TransformAction::drop_active_refs, filter, image, args.get (), name};
  RetiredModelReentry state {&refs};
  g_object_weak_ref (G_OBJECT (filter), finalized, &refs.filter_finalized);
  g_object_weak_ref (G_OBJECT (image), finalized, &refs.image_finalized);
  g_object_weak_ref (G_OBJECT (application), finalized, &refs.app_finalized);
  auto *bytes = g_bytes_new_with_free_func ("old model", 9, retired_checked_model, &state);
  auto *old = gimp_value_array_new_from_types (nullptr, G_TYPE_BYTES, bytes, G_TYPE_NONE);
  g_assert_true (gimp_filter_layer_set_definition (filter, "old boxed model", nullptr, old, nullptr));
  gimp_value_array_unref (old);
  g_bytes_unref (bytes);
  g_assert_cmpuint (state.calls, ==, 0);
  transform_state = &refs;
  g_value_register_transform_func (G_TYPE_DOUBLE, G_TYPE_INT, reentrant_double_to_int);
  GError *error = nullptr;
  g_assert_true (gimp_filter_layer_set_legacy_procedure (filter, name, args.get (), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (refs.calls, ==, 1);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_cmpuint (refs.filter_finalized, ==, 1);
  g_assert_cmpuint (refs.image_finalized, ==, 1);
  g_assert_cmpuint (refs.app_finalized, ==, 1);
  args.reset ();
  std::exit (0);
}

void checked_model_api_separation ()
{
  auto image = image_new ();
  auto filter = filter_new (image.get ());
  GError *error = nullptr;
  filter.set_definition ("model-only unknown", nullptr, nullptr);
  g_assert_null (gimp_filter_layer_get_procedure_args (filter.get ()));
  auto empty = ValueArrayOwner (gimp_value_array_new (0), gimp_value_array_unref);
  g_assert_true (gimp_filter_layer_set_definition (filter.get (), "model-only unknown", nullptr, empty.get (), nullptr));
  auto saved_empty = filter.get_procedure_args ();
  g_assert_nonnull (saved_empty.get ());
  g_assert_cmpuint (saved_empty.get ()->len, ==, 0);
  auto args = ValueArrayOwner (gimp_value_array_new_from_types (nullptr,
    G_TYPE_DOUBLE, 42.5, G_TYPE_STRING, "model context", G_TYPE_BYTES, nullptr,
    G_TYPE_INT, 0, G_TYPE_INT, 3, G_TYPE_INT, -19, G_TYPE_INT, -7, G_TYPE_NONE), gimp_value_array_unref);
  filter.set_definition ("plug-in-blinds", nullptr, args.get ());
  const auto revision = filter.definition_revision ();
  auto saved = filter.get_procedure_args ();
  g_assert_cmpfloat (g_value_get_double (&g_array_index (saved.get (), GValue, 0)), ==, 42.5);
  g_assert_cmpstr (g_value_get_string (&g_array_index (saved.get (), GValue, 1)), ==, "model context");
  g_assert_true (G_VALUE_TYPE (&g_array_index (saved.get (), GValue, 2)) == G_TYPE_BYTES);
  g_assert_false (filter.try_set_legacy_procedure ("model-only unknown", saved.get (), &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_clear_error (&error);
  g_assert_cmpuint (filter.definition_revision (), ==, revision);
  auto unchanged = filter.get_procedure_args ();
  g_assert_cmpfloat (g_value_get_double (&g_array_index (unchanged.get (), GValue, 0)), ==, 42.5);
  g_assert_cmpstr (g_value_get_string (&g_array_index (unchanged.get (), GValue, 1)), ==, "model context");
  /* Both languages see the checked output, while retained model reads survive. */
  g_assert_true (gimp_filter_layer_set_legacy_procedure (filter.get (), "plug-in-blinds", nullptr, &error));
  g_assert_no_error (error);
  auto checked = filter.get_procedure_args ();
  g_assert_true (G_VALUE_TYPE (&g_array_index (checked.get (), GValue, 0)) == G_TYPE_INT);
  g_assert_cmpint (g_value_get_int (&g_array_index (checked.get (), GValue, 0)), ==, 0);
  g_assert_cmpstr (g_value_get_string (&g_array_index (saved.get (), GValue, 1)), ==, "model context");
}

#include "test-filter-small-tiles-entry-wrapper.inc"

}

extern "C" void gimp_test_filter_entry_cpp_register (Gimp *gimp)
{
  application = gimp;
  g_test_add_func ("/filter-entry/cpp/small-tiles-handles", tiles_checked_handles);
  g_test_add_func ("/filter-entry/cpp/small-tiles-reentry", tiles_checked_reentry);
  g_test_add_func ("/filter-entry/cpp/checked_entry_routes", checked_entry_routes);
  g_test_add_func ("/filter-entry/cpp/checked_transform_reentry", checked_transform_reentry);
  g_test_add_func ("/filter-entry/cpp/checked_notify_close", checked_notify_close);
  g_test_add_func ("/filter-entry/cpp/checked_notify_newer", checked_notify_newer);
  g_test_add_func ("/filter-entry/cpp/checked_retired_bytes_reentry", checked_retired_bytes_reentry);
  g_test_add_func ("/filter-entry/cpp/checked_retired_model_after_transform", checked_retired_model_after_transform);
  g_test_add_func ("/filter-entry/cpp/checked_model_api_separation", checked_model_api_separation);

  g_test_add_func ("/filter-entry/cpp/bounded_dialog_read_owners", bounded_dialog_read_owners);
  g_test_add_func ("/filter-entry/cpp/original_entry_routes", original_entry_routes);
  g_test_add_func ("/filter-entry/cpp/native_type_factory_and_copy", native_type_factory_and_copy);
  g_test_add_func ("/filter-entry/cpp/independent_image_handle_owners", independent_image_handle_owners);
  g_test_add_func ("/filter-entry/cpp/owned_queries_outlive_definition_and_owner", owned_queries_outlive_definition_and_owner);
  g_test_add_func ("/filter-entry/cpp/definition_mark_idle_then_rerun", definition_mark_idle_then_rerun);
  g_test_add_func ("/filter-entry/cpp/mark_cancels_all_native_phases", mark_cancels_all_native_phases);
  g_test_add_func ("/filter-entry/cpp/closed_and_wrong_thread_containment", closed_and_wrong_thread_containment);
  g_test_add_func ("/filter-entry/cpp/running_owner_close_keeps_retained_cache", running_owner_close_keeps_retained_cache);
}

extern "C" void
gimp_test_filter_entry_assert_blinds_provider (Gimp *app)
{
  try
    {
      GimpPainter::FilterEditorSchema schema (app, GimpPainter::FilterProcedure::blinds);
      g_assert_true (schema.current (app));
    }
  catch (const std::exception& error)
    { g_error ("The real restored bundled Blinds provider is required: %s", error.what ()); }
}

extern "C" void
gimp_test_filter_entry_provider_rejections (Gimp *app, GimpFilterLayer *filter)
{
  gimp_test_filter_entry_assert_blinds_provider (app);
  auto *real_manager = app->plug_in_manager;
  auto *real_pdb = app->pdb;
  app->plug_in_manager = nullptr;
  gimp_test_filter_entry_assert_legacy_rejected (filter);
  app->plug_in_manager = real_manager;
  app->pdb = nullptr;
  gimp_test_filter_entry_assert_legacy_rejected (filter);
  app->pdb = real_pdb;
  for (guint scenario = 0; scenario < 8; ++scenario)
    {
      Registry registry (app);
      if (scenario == 0)
        {
          /* Newly initialized manager, with no completed restore capture. */
          gimp_test_filter_entry_assert_legacy_rejected (filter);
          continue;
        }
      auto owner = procedure (scenario == 2 ? "/tmp/untrusted-checked-blinds-provider" : nullptr);
      auto *p = GIMP_PROCEDURE (owner.get ());
      if (scenario == 3) G_PARAM_SPEC_INT (p->args[3])->minimum = 1;
      registry.attach (p);
      Object duplicate;
      if (scenario == 1) gimp_filter_parameter_editor_close (app->plug_in_manager);
      if (scenario == 4)
        {
          duplicate = procedure ();
          gimp_pdb_register_procedure (app->pdb, GIMP_PROCEDURE (duplicate.get ()));
        }
      if (scenario == 5)
        {
          duplicate = procedure ();
          gimp_pdb_unregister_procedure (app->pdb, p);
          gimp_pdb_register_procedure (app->pdb, GIMP_PROCEDURE (duplicate.get ()));
        }
      if (scenario == 6) app->plug_in_manager = registry.original_manager;
      if (scenario == 7) app->pdb = registry.original_pdb;
      gimp_test_filter_entry_assert_legacy_rejected (filter);
      app->plug_in_manager = GIMP_PLUG_IN_MANAGER (registry.manager.get ());
      app->pdb = GIMP_PDB (registry.pdb.get ());
    }
  gimp_test_filter_entry_assert_blinds_provider (app);
}
