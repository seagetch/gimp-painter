/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Private executable only. Link-time wrappers inject one recoverable failure
 * on the calling thread; ordinary GLib allocators and other threads are never
 * faulted. Setup, diagnostics, cleanup and assertions run with faults disabled. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpdrawable.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpitem.h"
#include "core/gimpundostack.h"
#include "pdb/pdb-types.h"
#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"
#include "plug-in/plug-in-types.h"
#include "plug-in/gimppluginmanager.h"
#include "dialogs/painter-layer-dialog.h"
void gimp_test_filter_allocation_register (Gimp *application);
}
#include "core/gimpfilterparametereditor.hpp"
#include "painter/object-ref.hpp"
#include "painter/resources.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <execinfo.h>
#include <memory>
#include <new>
#include <string>
#include <unistd.h>
#include <vector>

namespace Fault {
enum class Family { cpp, recoverable };
struct State {
  bool armed = false, hit = false;
  Family family = Family::cpp;
  unsigned remaining = 0, visits = 0;
  std::array<void *, 32> frames {};
  int frame_count = 0;
};
thread_local State state;
bool fail (Family family) noexcept
{
  if (!state.armed || family != state.family) return false;
  ++state.visits;
  if (state.remaining) { --state.remaining; return false; }
  state.hit = true;
  state.armed = false; // Error construction and cleanup must be able to allocate.
  state.frame_count = backtrace (state.frames.data (), state.frames.size ());
  return true;
}
void report_trace ()
{
  // Trace capture happened at the failed allocation, after fault disarming.
  // Print only when a transaction invariant fails, never for successful tests.
  for (int i = 0; i < state.frame_count; ++i)
    {
      Dl_info info {};
      const auto address = reinterpret_cast<std::uintptr_t> (state.frames[i]);
      if (dladdr (state.frames[i], &info) && info.dli_fbase)
        g_test_message ("allocation failure frame=%d address=%p module=%s offset=0x%zx symbol=%s",
                        i, state.frames[i], info.dli_fname ? info.dli_fname : "unknown",
                        std::size_t (address - reinterpret_cast<std::uintptr_t> (info.dli_fbase)),
                        info.dli_sname ? info.dli_sname : "unknown");
      else g_test_message ("allocation failure frame=%d address=%p module=unknown", i, state.frames[i]);
    }
  std::fflush (stdout);
}
struct Scope {
  Scope (Family family, unsigned position) { state = {true, false, family, position, 0}; }
  ~Scope () { state.armed = false; }
};
}
extern "C" void *__real__Znwm (std::size_t);
extern "C" void *__real__Znam (std::size_t);
extern "C" gpointer __real_g_try_malloc (gsize);
extern "C" gpointer __real_g_try_malloc0_n (gsize, gsize);
extern "C" void *__wrap__Znwm (std::size_t);
extern "C" void *__wrap__Znam (std::size_t);
extern "C" gpointer __wrap_g_try_malloc (gsize);
extern "C" gpointer __wrap_g_try_malloc0_n (gsize, gsize);
extern "C" void *__wrap__Znwm (std::size_t size)
{ if (Fault::fail (Fault::Family::cpp)) throw std::bad_alloc (); return __real__Znwm (size); }
extern "C" void *__wrap__Znam (std::size_t size)
{ if (Fault::fail (Fault::Family::cpp)) throw std::bad_alloc (); return __real__Znam (size); }
extern "C" gpointer __wrap_g_try_malloc (gsize size)
{ return Fault::fail (Fault::Family::recoverable) ? nullptr : __real_g_try_malloc (size); }
extern "C" gpointer __wrap_g_try_malloc0_n (gsize count, gsize size)
{ return Fault::fail (Fault::Family::recoverable) ? nullptr : __real_g_try_malloc0_n (count, size); }

namespace {
using namespace GimpPainter;
using Object = ObjectRef<GObject>;
struct SnapshotFree { void operator() (GimpFilterArgumentsSnapshot *p) const { gimp_filter_arguments_snapshot_free (p); } };
using Snapshot = std::unique_ptr<GimpFilterArgumentsSnapshot, SnapshotFree>;
struct BytesFree { void operator() (GBytes *p) const { if (p) g_bytes_unref (p); } };
using BytesRef = std::unique_ptr<GBytes, BytesFree>;
constexpr unsigned sweep_limit = 4096;
struct ChildInventory { bool available = false; std::vector<long> pids; };
bool stat_identity (const gchar *text, long& pid, long& parent)
{
  const char *end = std::strrchr (text, ')');
  char state;
  return end && std::sscanf (text, "%ld", &pid) == 1 &&
    std::sscanf (end + 1, " %c %ld", &state, &parent) == 2 && pid > 0 && parent >= 0;
}
void child_observation_unavailable (const char *reason)
{
  static bool reported = false;
  if (reported) return;
  reported = true;
  g_test_message ("Child inventory observation unavailable: %s; no /proc child proof claimed. "
                  "Plug-in-open signal and synchronous elapsed-time checks remain active; "
                  "registered metadata acquisition has no worker by architecture.", reason);
  std::fflush (stdout);
}
ChildInventory children ()
{
  ChildInventory result;
  gchar *text = nullptr; GError *error = nullptr;
  if (!g_file_get_contents ("/proc/self/stat", &text, nullptr, &error))
    {
      child_observation_unavailable (error ? error->message : "cannot read /proc/self/stat");
      g_clear_error (&error); return result;
    }
  long self = 0, parent = 0;
  const bool valid = stat_identity (text, self, parent); g_free (text);
  if (!valid) { child_observation_unavailable ("cannot parse /proc/self/stat"); return result; }
  GDir *directory = g_dir_open ("/proc", 0, &error);
  if (!directory)
    {
      child_observation_unavailable (error ? error->message : "cannot enumerate /proc");
      g_clear_error (&error); return result;
    }
  bool complete = true;
  while (const gchar *entry = g_dir_read_name (directory))
    {
      if (!*entry || std::strspn (entry, "0123456789") != std::strlen (entry)) continue;
      gchar path[96]; g_snprintf (path, sizeof path, "/proc/%s/stat", entry);
      text = nullptr;
      if (!g_file_get_contents (path, &text, nullptr, &error))
        {
          // A process may disappear between enumeration and read. Other read
          // failures make the inventory inconclusive, not evidence of no child.
          if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) complete = false;
          g_clear_error (&error); continue;
        }
      long pid = 0, ppid = 0;
      if (!stat_identity (text, pid, ppid)) complete = false;
      else if (ppid == self) result.pids.push_back (pid);
      g_free (text); // Never retain or log unrelated names, state or argv.
    }
  g_dir_close (directory);
  std::sort (result.pids.begin (), result.pids.end ());
  result.available = complete;
  if (!complete) child_observation_unavailable ("one or more numeric /proc stat records were unreadable or invalid");
  return result;
}
struct AcquisitionWatch {
  Gimp *app; guint opened = 0; gulong handler; ChildInventory before_children;
  mutable bool inventory_proven = true;
  explicit AcquisitionWatch (Gimp *application) : app (application), before_children (children ())
  {
    handler = g_signal_connect (app->plug_in_manager, "plug-in-opened",
      G_CALLBACK (+[] (GimpPlugInManager *, gpointer, gpointer data) { ++*static_cast<guint *> (data); }), &opened);
  }
  ~AcquisitionWatch ()
  {
    g_signal_handler_disconnect (app->plug_in_manager, handler);
    g_test_message ("Acquisition observation: opened=%u proc-child-inventory=%s", opened,
                    before_children.available && inventory_proven ? "verified unchanged" : "unavailable; no proc proof");
  }
  void unchanged () const
  {
    g_assert_cmpuint (opened, ==, 0);
    const auto now = children ();
    if (before_children.available && now.available) g_assert_true (now.pids == before_children.pids);
    else inventory_proven = false;
  }
};

void descriptor_failures (gconstpointer data)
{
  auto *app = const_cast<Gimp *> (static_cast<const Gimp *> (data));
  AcquisitionWatch watch (app);
  for (unsigned route = 1; route <= 4; ++route)
    {
      const auto selected = static_cast<FilterProcedure> (route);
      FilterEditorSchema original (app, selected);
      g_assert_true (original.current (app)); // Warm lazy default metadata first.
      const auto& names = filter_editor_route_names (selected);
      std::array<GObject *, 4> owners {{G_OBJECT (app->pdb), G_OBJECT (app->plug_in_manager),
        G_OBJECT (gimp_pdb_lookup_procedure (app->pdb, names.execution)),
        G_OBJECT (gimp_pdb_lookup_procedure (app->pdb, names.public_name ? names.public_name : names.execution))}};
      std::array<guint, 4> references {};
      for (unsigned i = 0; i < owners.size (); ++i)
        { g_assert_nonnull (owners[i]); references[i] = owners[i]->ref_count; }
      for (bool freshness : {false, true})
        {
          unsigned injected = 0; bool completed = false;
          for (unsigned position = 0; position < sweep_limit; ++position)
            {
              bool success = false, bad_alloc = false;
              std::unique_ptr<FilterEditorSchema> result;
              g_test_message ("descriptor attempt route=%u operation=%s allocation-index=%u", route,
                              freshness ? "current" : "construct", position); std::fflush (stdout);
              const auto start = g_get_monotonic_time ();
              {
                Fault::Scope fault (Fault::Family::cpp, position);
                try {
                  if (freshness) success = original.current (app);
                  else { result.reset (new FilterEditorSchema (app, selected)); success = true; }
                } catch (const std::bad_alloc&) { bad_alloc = true; }
              }
              const bool hit = Fault::state.hit;
              const unsigned visits = Fault::state.visits;
              g_assert_cmpint (g_get_monotonic_time () - start, <, 10 * G_TIME_SPAN_SECOND);
              if (hit)
                {
                  ++injected; g_assert_false (success); g_assert_null (result.get ());
                  g_assert_true (freshness ? !bad_alloc : bad_alloc);
                }
              else { g_assert_true (success); completed = true; }
              result.reset ();
              for (unsigned i = 0; i < owners.size (); ++i)
                g_assert_cmpuint (owners[i]->ref_count, ==, references[i]);
              g_assert_true (original.current (app)); watch.unchanged ();
              if (!hit)
                { g_test_message ("descriptor route=%u operation=%s injected=%u allocations=%u refs-restored=yes opened=%u",
                    route, freshness ? "current" : "construct", injected, visits, watch.opened); break; }
            }
          g_assert_true (completed); g_assert_cmpuint (injected, >, 0);
        }
    }
}

constexpr unsigned value_count = 7;
std::array<Value, value_count> values (bool changed)
{
  std::array<Value, value_count> result;
  const GType types[] = {G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRV, GIMP_TYPE_DOUBLE_ARRAY,
                        GIMP_TYPE_INT32_ARRAY, GIMP_TYPE_ARRAY, G_TYPE_BYTES};
  for (unsigned i = 0; i < value_count; ++i) result[i] = Value (types[i]);
  g_value_set_int (result[0].get (), changed ? 9 : -7);
  g_value_set_string (result[1].get (), changed ? "replacement string" : "saved string");
  const gchar *strings[] = {changed ? "replacement" : "saved", "second", "third", nullptr};
  g_value_set_boxed (result[2].get (), strings);
  const gdouble real[] = {-0.0, changed ? 0.25 : 0.5, 1.0000000000000002};
  gimp_value_set_double_array (result[3].get (), real, G_N_ELEMENTS (real));
  const gint32 integers[] = {-7, changed ? 9 : 2, G_MAXINT};
  gimp_value_set_int32_array (result[4].get (), integers, G_N_ELEMENTS (integers));
  const guint8 raw[] = {0xff, 0, static_cast<guint8> (changed ? 9 : 2), 0x80};
  g_value_take_boxed (result[5].get (), gimp_array_new (raw, sizeof raw, FALSE));
  g_value_take_boxed (result[6].get (), g_bytes_new_static ("untouched immutable tail", 24));
  return result;
}
GimpValueArray *route_arguments (unsigned route)
{
  if (route == 1)
    return gimp_value_array_new_from_types (nullptr, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
      G_TYPE_INT, 23, G_TYPE_INT, 7, G_TYPE_INT, G_MININT, G_TYPE_INT, -9, G_TYPE_NONE);
  if (route == 2)
    return gimp_value_array_new_from_types (nullptr, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
      G_TYPE_INT, 2, G_TYPE_NONE);
  if (route == 3)
    return gimp_value_array_new_from_types (nullptr, G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456,
      G_TYPE_INT, 64, G_TYPE_INT, 3, G_TYPE_INT, 2, G_TYPE_DOUBLE, 1.0, G_TYPE_NONE);
  g_assert_cmpuint (route, ==, 4);
  gdouble matrix[25] {}; matrix[1] = -0.0; matrix[12] = 1.0;
  const gint32 channels[5] = {1, -7, 0, 2, 1};
  auto *args = gimp_value_array_new_from_types (nullptr,
    G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456, G_TYPE_INT, 25,
    GIMP_TYPE_DOUBLE_ARRAY, nullptr, G_TYPE_INT, -17, G_TYPE_DOUBLE, 1.0,
    G_TYPE_DOUBLE, -0.0, G_TYPE_INT, 5, GIMP_TYPE_INT32_ARRAY, nullptr,
    G_TYPE_INT, 2, G_TYPE_NONE);
  gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
  gimp_value_set_int32_array (gimp_value_array_index (args, 9), channels, 5);
  return args;
}
struct Fixture {
  const unsigned side;
  Object image;
  GimpFilterLayer *filter = nullptr;
  Snapshot original;
  BytesRef raw;
  GimpFilterLayerSnapshot saved {};
  guint64 revision = 0, runs = 0;
  std::vector<guint8> pixels;
  explicit Fixture (Gimp *app, unsigned route = 0)
    : side (route ? 16 : 4), image (Object::adopt (G_OBJECT (gimp_image_new (
      app, side, side, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR)))), pixels (side * side * 4)
  {
    auto *owner = GIMP_IMAGE (image.get ());
    filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (owner, side, side, "Allocation transaction", 1,
                                                     GIMP_LAYER_MODE_NORMAL_LEGACY));
    g_assert_nonnull (filter);
    g_assert_true (gimp_image_add_layer (owner, GIMP_LAYER (filter), nullptr, 0, FALSE));
    auto initial = values (false);
    GimpFilterArgumentSpec specs[value_count] {};
    for (unsigned i = 0; i < value_count; ++i)
      { specs[i].value_type = G_VALUE_TYPE (initial[i].get ()); specs[i].value = initial[i].get (); }
    GError *error = nullptr;
    original.reset (gimp_filter_arguments_snapshot_import (value_count, specs, &error));
    g_assert_no_error (error); g_assert_nonnull (original.get ());
    raw.reset (g_bytes_new_static ("original raw\0\xff", 14));
    if (route)
      {
        auto *args = route_arguments (route);
        const auto& names = filter_editor_route_names (static_cast<FilterProcedure> (route));
        g_assert_true (gimp_filter_layer_set_definition (filter, names.saved, raw.get (), args, &error));
        gimp_value_array_unref (args);
        original.reset (gimp_filter_layer_snapshot_arguments (filter));
      }
    else g_assert_true (gimp_filter_layer_set_definition_with_snapshot (filter,
      "future-allocation-transaction-fixture", raw.get (), original.get (), &error));
    g_assert_no_error (error);
    for (unsigned i = 0; i < pixels.size (); ++i) pixels[i] = guint8 (i * 3 + 17);
    const GeglRectangle extent {0, 0, gint (side), gint (side)};
    gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), &extent, 0,
                     babl_format ("R'G'B'A u8"), pixels.data (), GEGL_AUTO_ROWSTRIDE);
    gimp_filter_layer_mark_as_loaded (filter);
    gimp_image_undo_free (owner);
    g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &saved));
    g_assert_true (saved.cache_complete);
    revision = gimp_filter_layer_get_definition_revision (filter);
    runs = gimp_filter_layer_get_run_count (filter);
  }
  bool unchanged (const char *kind, unsigned position)
  {
    GimpFilterLayerSnapshot now {};
    g_assert_true (gimp_filter_layer_get_snapshot_state (filter, &now));
    Snapshot actual (gimp_filter_layer_snapshot_arguments (filter));
    BytesRef retained (gimp_filter_layer_ref_definition (filter));
    std::vector<guint8> actual_pixels (pixels.size ());
    const GeglRectangle extent {0, 0, gint (side), gint (side)};
    gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)), &extent, 1.0,
                    babl_format ("R'G'B'A u8"), actual_pixels.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    const auto depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (GIMP_IMAGE (image.get ())));
    const auto count = gimp_filter_arguments_snapshot_count (original.get ());
    bool identity = bool (actual) && gimp_filter_arguments_snapshot_count (actual.get ()) == count;
    for (unsigned i = 0; identity && i < count; ++i)
      identity = gimp_filter_arguments_snapshot_peek_value (actual.get (), i) ==
                 gimp_filter_arguments_snapshot_peek_value (original.get (), i);
    const bool same = identity && retained && g_bytes_equal (retained.get (), raw.get ()) &&
      gimp_filter_layer_get_definition_revision (filter) == revision && now.generation == saved.generation &&
      now.cache_generation == saved.cache_generation && now.cache_complete == saved.cache_complete &&
      now.state == saved.state && gimp_filter_layer_get_run_count (filter) == runs && depth == 0 && actual_pixels == pixels;
    if (!same)
      {
        g_test_message ("TRANSACTION FAILURE kind=%s index=%u model_identity=%d revision=%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT
          " generation=%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT " cache=%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT
          " complete=%d/%d undo=%d state=%d/%d pixels_equal=%d",
          kind, position, identity, gimp_filter_layer_get_definition_revision (filter), revision,
          now.generation, saved.generation, now.cache_generation, saved.cache_generation,
          now.cache_complete, saved.cache_complete, depth, now.state, saved.state, actual_pixels == pixels);
        Fault::report_trace ();
        g_test_fail ();
      }
    return same;
  }
};
struct PatchCase { Gimp *app; unsigned slot; Fault::Family family; const char *name; unsigned route = 0; };
void patch_failures (gconstpointer data)
{
  const auto& test = *static_cast<const PatchCase *> (data);
  auto replacements = values (true);
  Value route_replacement (test.route == 4 ? G_TYPE_DOUBLE : G_TYPE_INT);
  if (test.route == 4) g_value_set_double (route_replacement.get (), 0.125);
  else g_value_set_int (route_replacement.get (), test.route == 1 ? 47 : test.route == 2 ? 3 : 128);
  const GimpFilterArgumentPatch patch {test.slot,
    test.route ? route_replacement.get () : replacements[test.slot].get ()};
  unsigned injected = 0, violations = 0; bool completed = false;
  AcquisitionWatch watch (test.app);
  for (unsigned position = 0; position < sweep_limit; ++position)
    {
      Fixture fixture (test.app, test.route);
      const auto count = gimp_filter_arguments_snapshot_count (fixture.original.get ());
      GError *error = nullptr; gboolean success;
      g_test_message ("patch attempt kind=%s allocation-index=%u revision=%" G_GUINT64_FORMAT,
                      test.name, position, fixture.revision); std::fflush (stdout);
      const auto start = g_get_monotonic_time ();
      {
        Fault::Scope fault (test.family, position);
        success = gimp_filter_layer_edit_argument_patch (fixture.filter, fixture.revision,
          fixture.original.get (), 1, &patch, nullptr, nullptr, &error);
      }
      const bool hit = Fault::state.hit;
      const unsigned visits = Fault::state.visits;
      g_test_message ("patch outcome kind=%s allocation-index=%u hit=%d visits=%u success=%d error=%s",
                      test.name, position, hit, visits, success, error ? error->message : "none");
      std::fflush (stdout);
      g_assert_cmpint (g_get_monotonic_time () - start, <, 10 * G_TIME_SPAN_SECOND);
      if (hit)
        {
          ++injected;
          const bool reported = !success && error;
          if (!reported)
            { g_test_message ("FAULT NOT REPORTED kind=%s index=%u success=%d error=%s", test.name,
                              position, success, error ? error->message : "none"); Fault::report_trace (); g_test_fail (); }
          g_clear_error (&error);
          const bool retained = fixture.unchanged (test.name, position);
          if (!reported || !retained)
            {
              ++violations; watch.unchanged ();
              // Keep the failure latched, retire this fixture, and probe the
              // next index with an entirely fresh model/cache/history.
              continue;
            }
          // Retry the exact snapshot/patch after failure; only this edit adds Undo.
          g_assert_true (gimp_filter_layer_edit_argument_patch (fixture.filter, fixture.revision,
            fixture.original.get (), 1, &patch, nullptr, nullptr, &error));
        }
      else { g_assert_true (success); completed = true; }
      g_assert_no_error (error);
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (GIMP_IMAGE (fixture.image.get ()))), ==, 1);
      Snapshot actual (gimp_filter_layer_snapshot_arguments (fixture.filter));
      for (unsigned i = 0; i < count; ++i)
        if (i != test.slot)
          g_assert_true (gimp_filter_arguments_snapshot_peek_value (actual.get (), i) ==
                         gimp_filter_arguments_snapshot_peek_value (fixture.original.get (), i));
      g_assert_true (gimp_image_undo (GIMP_IMAGE (fixture.image.get ())));
      Snapshot undone (gimp_filter_layer_snapshot_arguments (fixture.filter));
      for (unsigned i = 0; i < count; ++i)
        g_assert_true (gimp_filter_arguments_snapshot_peek_value (undone.get (), i) ==
                       gimp_filter_arguments_snapshot_peek_value (fixture.original.get (), i));
      watch.unchanged ();
      if (!hit)
        { g_test_message ("patch kind=%s injected=%u allocations=%u transaction-violations=%u exact-cache-preserved=%s",
                          test.name, injected, visits, violations, violations ? "no" : "yes"); break; }
    }
  g_assert_true (completed);
  if (test.family == Fault::Family::cpp || test.slot != 0) g_assert_cmpuint (injected, >, 0);
}

GtkWidget *find_widget (GtkWidget *widget, const char *name)
{
  if (!g_strcmp0 (gtk_widget_get_name (widget), name)) return widget;
  if (!GTK_IS_CONTAINER (widget)) return nullptr;
  GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
  GtkWidget *result = nullptr;
  for (GList *item = children; item && !result; item = item->next)
    result = find_widget (GTK_WIDGET (item->data), name);
  g_list_free (children); return result;
}
void gtk_changed_array_failure (gconstpointer data)
{
  auto *app = const_cast<Gimp *> (static_cast<const Gimp *> (data));
  Fixture fixture (app);
  gdouble matrix[25] {}; matrix[12] = 1.0;
  const gint32 channels[5] = {1, -7, 0, 2, 1};
  GimpValueArray *args = gimp_value_array_new_from_types (nullptr,
    G_TYPE_INT, 7, G_TYPE_INT, 123, G_TYPE_INT, -456, G_TYPE_INT, 25,
    GIMP_TYPE_DOUBLE_ARRAY, nullptr, G_TYPE_INT, -17, G_TYPE_DOUBLE, 1.0,
    G_TYPE_DOUBLE, -0.0, G_TYPE_INT, 5, GIMP_TYPE_INT32_ARRAY, nullptr,
    G_TYPE_INT, 2, G_TYPE_NONE);
  gimp_value_set_double_array (gimp_value_array_index (args, 4), matrix, 25);
  gimp_value_set_int32_array (gimp_value_array_index (args, 9), channels, 5);
  GError *error = nullptr;
  g_assert_true (gimp_filter_layer_set_definition (fixture.filter, "plug-in-convmatrix",
                                                 fixture.raw.get (), args, &error));
  g_assert_no_error (error); gimp_value_array_unref (args);
  gimp_filter_layer_mark_as_loaded (fixture.filter);
  auto *image = GIMP_IMAGE (fixture.image.get ());
  gimp_image_undo_free (image);
  const auto revision = gimp_filter_layer_get_definition_revision (fixture.filter);
  const auto generation = gimp_filter_layer_get_generation (fixture.filter);
  const auto cache = gimp_filter_layer_get_cache_generation (fixture.filter);
  const auto runs = gimp_filter_layer_get_run_count (fixture.filter);
  Snapshot before (gimp_filter_layer_snapshot_arguments (fixture.filter));
  GtkWidget *parent = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (parent);
  GtkWidget *dialog = painter_filter_layer_dialog_new (image, GIMP_LAYER (fixture.filter),
                                                       gimp_get_user_context (app), parent);
  g_assert_nonnull (dialog); g_object_ref_sink (dialog); gtk_widget_show (dialog);
  auto *entry = find_widget (dialog, "painter-convmatrix-0");
  g_assert_true (GTK_IS_ENTRY (entry)); gtk_entry_set_text (GTK_ENTRY (entry), "0.25");
  gboolean closed = FALSE;
  g_signal_connect (dialog, "destroy", G_CALLBACK (+[] (GtkWidget *, gpointer p) {
    *static_cast<gboolean *> (p) = TRUE;
  }), &closed);
  AcquisitionWatch watch (app);
  g_test_message ("GTK matrix response allocation-index=0 family=g_try"); std::fflush (stdout);
  {
    Fault::Scope fault (Fault::Family::recoverable, 0);
    gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  }
  g_test_message ("GTK matrix response hit=%d visits=%u closed=%d revision=%" G_GUINT64_FORMAT "/%" G_GUINT64_FORMAT,
                  Fault::state.hit, Fault::state.visits, closed,
                  gimp_filter_layer_get_definition_revision (fixture.filter), revision); std::fflush (stdout);
  g_assert_true (Fault::state.hit); g_assert_false (closed);
  auto *error_label = find_widget (dialog, "painter-layer-error");
  g_assert_nonnull (error_label); g_assert_true (gtk_widget_get_visible (error_label));
  g_assert_cmpuint (gimp_filter_layer_get_definition_revision (fixture.filter), ==, revision);
  g_assert_cmpuint (gimp_filter_layer_get_generation (fixture.filter), ==, generation);
  g_assert_cmpuint (gimp_filter_layer_get_cache_generation (fixture.filter), ==, cache);
  g_assert_cmpuint (gimp_filter_layer_get_run_count (fixture.filter), ==, runs);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 0);
  Snapshot after (gimp_filter_layer_snapshot_arguments (fixture.filter));
  for (unsigned i = 0; i < gimp_filter_arguments_snapshot_count (before.get ()); ++i)
    g_assert_true (gimp_filter_arguments_snapshot_peek_value (before.get (), i) ==
                   gimp_filter_arguments_snapshot_peek_value (after.get (), i));
  BytesRef raw (gimp_filter_layer_ref_definition (fixture.filter));
  g_assert_true (g_bytes_equal (raw.get (), fixture.raw.get ()));
  std::vector<guint8> actual_pixels (fixture.pixels.size ());
  const GeglRectangle extent {0, 0, 4, 4};
  gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (fixture.filter)), &extent, 1.0,
                  babl_format ("R'G'B'A u8"), actual_pixels.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  g_assert_true (actual_pixels == fixture.pixels); watch.unchanged ();
  gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_assert_true (closed);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, 1);
  Snapshot edited (gimp_filter_layer_snapshot_arguments (fixture.filter));
  const auto *array = static_cast<const GimpArray *> (g_value_get_boxed (
    gimp_filter_arguments_snapshot_peek_value (edited.get (), 4)));
  gdouble first; std::memcpy (&first, array->data, sizeof first); g_assert_true (first == 0.25);
  g_assert_true (gimp_image_undo (image));
  g_object_unref (dialog); gtk_widget_destroy (parent); g_object_unref (parent);
}
}

extern "C" void gimp_test_filter_allocation_register (Gimp *application)
{
  g_test_add_data_func ("/painter-layer-ui/filter_fault_descriptors", application, descriptor_failures);
  g_test_add_data_func ("/painter-layer-ui/filter_fault_gtk_changed_array", application, gtk_changed_array_failure);
  static PatchCase cases[16];
  const char *kinds[] = {"scalar", "string", "strv", "double_array", "int32_array", "raw_array"};
  for (unsigned family = 0; family < 2; ++family)
    for (unsigned slot = 0; slot < 6; ++slot)
      {
        gchar *label = g_strdup_printf ("%s_%s", family ? "try" : "cpp", kinds[slot]);
        const char *name = g_intern_string (label); g_free (label);
        auto& item = cases[family * 6 + slot];
        item = {application, slot, family ? Fault::Family::recoverable : Fault::Family::cpp, name};
        gchar *path = g_strdup_printf ("/painter-layer-ui/filter_fault_patch_%s", name);
        g_test_add_data_func (path, &item, patch_failures); g_free (path);
      }
  const char *routes[] = {"blinds", "small_tiles", "retinex", "convolution"};
  for (unsigned route = 1; route <= 4; ++route)
    {
      auto& item = cases[11 + route];
      item = {application, route == 4 ? 7u : 3u, Fault::Family::cpp, routes[route - 1], route};
      gchar *path = g_strdup_printf ("/painter-layer-ui/filter_fault_route_%s", item.name);
      g_test_add_data_func (path, &item, patch_failures); g_free (path);
    }
}
