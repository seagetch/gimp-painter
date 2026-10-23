/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core/core-types.h"
#include "core/gimpmypaintbrush.h"
#include "painter/gimp-painter-binding.h"
void gimp_test_painter_mybrush_duplicate_handle_register (void);
}
#include "core/gimppaintermybrush-handle.hpp"
#include <type_traits>
#include <utility>

using GimpPainter::PainterMybrushRef;
using GimpPainter::MypaintBrushRef;
static_assert (std::is_same<PainterMybrushRef, MypaintBrushRef>::value,
               "The legacy handle spelling must alias the native owner");
static_assert (std::is_same<decltype (&gimp_mypaint_brush_duplicate),
                           GimpData *(*)(GimpMypaintBrush *)>::value,
               "C and C++ must see the same exact duplicate signature");
static_assert (std::is_same<decltype (std::declval<const MypaintBrushRef&> ().duplicate ()),
                           PainterMybrushRef>::value,
               "duplicate() must return an owning native handle");

#ifdef GIMP_TEST_MYBRUSH_WRAP_PIXBUF_COPY
namespace {
thread_local const GdkPixbuf *fault_icon = nullptr;
thread_local guint fault_count = 0;
}
extern "C" GdkPixbuf *__real_gdk_pixbuf_copy (const GdkPixbuf *icon);
extern "C" GdkPixbuf *__wrap_gdk_pixbuf_copy (const GdkPixbuf *icon);
extern "C" void gimp_test_mybrush_copy_fault_arm (const GdkPixbuf *icon);
extern "C" gboolean gimp_test_mybrush_copy_fault_consumed (void);

extern "C" GdkPixbuf *
__wrap_gdk_pixbuf_copy (const GdkPixbuf *icon)
{
  if (icon == fault_icon && fault_icon)
    {
      fault_icon = nullptr;
      ++fault_count;
      return nullptr;
    }
  return __real_gdk_pixbuf_copy (icon);
}

extern "C" void
gimp_test_mybrush_copy_fault_arm (const GdkPixbuf *icon)
{
  g_assert_nonnull (icon);
  g_assert_null (fault_icon);
  fault_count = 0;
  fault_icon = icon;
}

extern "C" gboolean
gimp_test_mybrush_copy_fault_consumed (void)
{
  return fault_icon == nullptr && fault_count == 1;
}
#endif

namespace {
void finalized (gpointer data, GObject *)
{
  ++*static_cast<guint *> (data);
}

template<class Function>
void expect_handle_error (Function function)
{
  bool caught = false;
  try { function (); }
  catch (const GimpPainter::Error& error)
    {
      caught = true;
      g_assert_nonnull (error.what ());
      g_assert_true (*error.what () != '\0');
    }
  catch (...) { g_assert_not_reached (); }
  g_assert_true (caught);
}

void named_standard_cold_recreation ()
{
  static_assert (std::is_same<decltype (&gimp_mypaint_brush_get_standard),
                 GimpData *(*)(GimpContext *)>::value,
                 "The original standard provider is a borrowed C result");
  if (! g_test_subprocess ())
    {
      g_test_trap_subprocess (nullptr, 20 * G_USEC_PER_SEC, GTestSubprocessFlags (0));
      g_test_trap_assert_passed ();
      return;
    }

  /* No application/context/factory has run in this process. The sole initial
   * reference is the native provider's creation seed, explicitly retired here
   * to model provider teardown. This is not permission for ordinary callers
   * to unref a borrowed get_standard result. */
  guint deaths = 0;
  auto *seed = gimp_mypaint_brush_get_standard (nullptr);
  g_assert_nonnull (seed);
  g_assert_cmpuint (G_OBJECT (seed)->ref_count, ==, 1);
  g_object_weak_ref (G_OBJECT (seed), finalized, &deaths);
  {
    auto first = MypaintBrushRef::get_standard ();
    g_assert_true (GIMP_DATA (first.get ()) == seed);
    g_assert_cmpuint (G_OBJECT (seed)->ref_count, ==, 2);
    {
      auto second = MypaintBrushRef::get_standard ();
      g_assert_true (second.get () == first.get ());
      g_assert_cmpuint (G_OBJECT (seed)->ref_count, ==, 3);
      g_object_unref (seed); // Cold fixture retires the known provider seed.
      g_assert_cmpuint (G_OBJECT (first.get ())->ref_count, ==, 2);
      g_assert_cmpuint (deaths, ==, 0);
    }
    g_assert_cmpuint (G_OBJECT (first.get ())->ref_count, ==, 1);
    g_assert_cmpuint (deaths, ==, 0);
    g_assert_true (gimp_mypaint_brush_get_standard (nullptr) == GIMP_DATA (first.get ()));
    g_assert_true (gimp_painter_binding_close (G_OBJECT (first.get ()), nullptr));
    // Native closed-read policy: identity survives, no shadow standard appears.
    {
      auto closed = MypaintBrushRef::get_standard ();
      g_assert_true (closed.get () == first.get ());
      auto resource = closed.snapshot ();
      expect_handle_error ([&] { closed.replace (resource); });
    }
  }
  g_assert_cmpuint (deaths, ==, 1);
  // Address reuse is legal: prove a fresh active object by state and teardown.
  auto *replacement = gimp_mypaint_brush_get_standard (nullptr);
  g_assert_cmpuint (G_OBJECT (replacement)->ref_count, ==, 1);
  g_object_weak_ref (G_OBJECT (replacement), finalized, &deaths);
  {
    auto owner = MypaintBrushRef::get_standard ();
    auto resource = owner.snapshot ();
    resource.set_base_value (BRUSH_OPAQUE, 0.375);
    owner.replace (resource);
    g_assert_cmpfloat (owner.snapshot ().base_value (BRUSH_OPAQUE), ==, 0.375);
    g_object_unref (replacement); // Retire this cold provider seed as well.
    g_assert_cmpuint (deaths, ==, 1);
  }
  g_assert_cmpuint (deaths, ==, 2);
}

void named_factory_handle ()
{
  static_assert (std::is_same<decltype (&gimp_mypaint_brush_new),
                 GimpData *(*)(GimpContext *, const gchar *)>::value,
                 "C factory signature is preserved");
  guint deaths = 0;
  {
    auto owner = MypaintBrushRef::create ("Factory owned handle");
    g_object_weak_ref (G_OBJECT (owner.get ()), finalized, &deaths);
    g_assert_cmpuint (G_OBJECT_TYPE (owner.get ()), ==, gimp_mypaint_brush_get_type ());
    g_assert_cmpstr (gimp_object_get_name (owner.get ()), ==, "Factory owned handle");
    auto snapshot = owner.snapshot ();
    snapshot.set_base_value (BRUSH_OPAQUE, 0.375);
    owner.replace (snapshot);
    g_assert_cmpfloat (owner.snapshot ().base_value (BRUSH_OPAQUE), ==, 0.375);
  }
  g_assert_cmpuint (deaths, ==, 1);
}

void handle_alias_and_resource_independence ()
{
  auto source = MypaintBrushRef::create ("Handle native name \316\262");
  auto resource = GimpPainter::MyPaint::Resource::decode (
    "{\"version\":3,\"parent_brush_name\":\"unrelated handle parent\","
    "\"settings\":{\"opaque\":{\"base_value\":0.625,"
    "\"inputs\":{\"pressure\":[[0,0.125],[1,0.875]]}}},"
    "\"texts\":{\"brushmark_name\":\"handle mark\"},"
    "\"future_metadata\":{\"retain\":[1,null,\"value\"]}}");
  source.replace (resource);
  gimp_data_clean (GIMP_DATA (source.get ()));
  const auto original = source.snapshot ().encode ();
  auto duplicate = source.duplicate ();
  g_assert_true (duplicate.get () != source.get ());
  g_assert_cmpstr (gimp_object_get_name (duplicate.get ()), ==, "Handle native name \316\262");
  const auto copied = duplicate.snapshot ().encode ();
  g_assert_cmpstr (copied.c_str (), ==, original.c_str ());
  const auto parent = duplicate.snapshot ().parent_brush_name ();
  g_assert_cmpstr (parent.c_str (), ==, "unrelated handle parent");
  auto edited = duplicate.snapshot ();
  edited.set_parent_brush_name ("edited duplicate parent");
  edited.set_base_value (BRUSH_OPAQUE, 0.25);
  edited.set_curve (BRUSH_OPAQUE, INPUT_PRESSURE,
                    {{0, 0.25}, {0.5, 0.75}, {1, 1}});
  duplicate.replace (edited);
  const auto unchanged = source.snapshot ().encode ();
  g_assert_cmpstr (unchanged.c_str (), ==, original.c_str ());
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source.get ())));
  g_assert_cmpstr (gimp_object_get_name (duplicate.get ()), ==, "Handle native name \316\262");
}

void handle_copy_move_ownership ()
{
  guint source_finalized = 0;
  guint duplicate_finalized = 0;
  {
    auto survivor = [&] {
      auto source = PainterMybrushRef::create ("Scoped named owner");
      g_object_weak_ref (G_OBJECT (source.get ()), finalized, &source_finalized);
      auto duplicate = source.duplicate ();
      g_object_weak_ref (G_OBJECT (duplicate.get ()), finalized, &duplicate_finalized);
      {
        MypaintBrushRef copy = duplicate;
        auto assigned = PainterMybrushRef::create ("Replaced temporary owner");
        assigned = copy;
        auto moved = std::move (assigned);
        g_assert_null (assigned.get ());
        g_assert_true (copy.get () == duplicate.get ());
        g_assert_true (moved.get () == duplicate.get ());
        expect_handle_error ([&] { assigned.duplicate (); });
      }
      g_assert_cmpuint (duplicate_finalized, ==, 0);
      return duplicate;
    } ();
    g_assert_cmpuint (source_finalized, ==, 1);
    g_assert_cmpuint (duplicate_finalized, ==, 0);
    g_assert_cmpstr (gimp_object_get_name (survivor.get ()), ==, "Scoped named owner");
    for (guint i = 0; i < 8; ++i)
      {
        guint repeated_finalized = 0;
        {
          auto repeated = survivor.duplicate ();
          g_object_weak_ref (G_OBJECT (repeated.get ()), finalized, &repeated_finalized);
          g_assert_true (repeated.get () != survivor.get ());
        }
        g_assert_cmpuint (repeated_finalized, ==, 1);
      }
  }
  g_assert_cmpuint (source_finalized, ==, 1);
  g_assert_cmpuint (duplicate_finalized, ==, 1);
}

void filename_load_handle ()
{
  static_assert (std::is_same<decltype (&gimp_mypaint_brush_load),
    GList *(*)(GimpContext *, const gchar *, GError **)>::value, "Original C filename signature");
  GError *error = nullptr;
  gchar *dir = g_dir_make_tmp ("painter-filename-handle-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "handle.myb", nullptr);
  g_assert_no_error (error);
  g_assert_true (g_file_set_contents (path, "version 2\n", -1, &error));
  g_assert_no_error (error);
  guint deaths = 0;
  {
    auto owner = MypaintBrushRef::load (path);
    g_object_weak_ref (G_OBJECT (owner.get ()), finalized, &deaths);
    g_assert_cmpstr (gimp_object_get_name (owner.get ()), ==, "handle");
    const auto snapshot = owner.snapshot ().encode ();
    g_assert_cmpint (g_unlink (path), ==, 0);
    expect_handle_error ([&] { MypaintBrushRef::load (path); });
    auto retained = owner;
    g_assert_true (retained.get () == owner.get ());
    const auto retained_snapshot = retained.snapshot ().encode ();
    g_assert_cmpstr (retained_snapshot.c_str (), ==, snapshot.c_str ());
    g_assert_cmpuint (deaths, ==, 0);
  }
  g_assert_cmpuint (deaths, ==, 1);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (dir);
}

void file_save_handle ()
{
  static_assert (std::is_same<decltype (&gimp_mypaint_brush_save),
    gboolean (*)(GimpData *, GError **)>::value, "Original save C signature");
  GError *error = nullptr;
  gchar *dir = g_dir_make_tmp ("painter-save-handle-XXXXXX", &error);
  gchar *path = g_build_filename (dir, "saved.myb", nullptr);
  g_assert_no_error (error);
  guint deaths = 0;
  {
    auto owner = MypaintBrushRef::create ("Handle save");
    g_object_weak_ref (G_OBJECT (owner.get ()), finalized, &deaths);
    expect_handle_error ([&] { owner.save (); });
    GFile *file = g_file_new_for_path (path);
    gimp_data_set_file (GIMP_DATA (owner.get ()), file, TRUE, TRUE);
    g_object_unref (file);
    owner.save ();
    g_assert_false (gimp_data_is_dirty (GIMP_DATA (owner.get ())));
    auto loaded = MypaintBrushRef::load (path);
    g_assert_cmpstr (gimp_object_get_name (loaded.get ()), ==, "Handle save");
    g_assert_true (gimp_painter_binding_close (G_OBJECT (owner.get ()), &error));
    g_assert_no_error (error);
    expect_handle_error ([&] { owner.save (); });
    g_assert_cmpuint (deaths, ==, 0);
  }
  g_assert_cmpuint (deaths, ==, 1);
  g_assert_cmpint (g_unlink (path), ==, 0);
  g_assert_cmpint (g_rmdir (dir), ==, 0);
  g_free (path); g_free (dir);
}

struct WrongThreadCall { const PainterMybrushRef *source; bool caught; };
gpointer handle_duplicate_wrong_thread (gpointer data)
{
  auto *call = static_cast<WrongThreadCall *> (data);
  try { call->source->duplicate (); }
  catch (const GimpPainter::Error&) { call->caught = true; }
  catch (...) { g_assert_not_reached (); }
  return nullptr;
}

void handle_failure_exceptions ()
{
  auto source = PainterMybrushRef::create ("Rejected handle source");
  const auto before = source.snapshot ().encode ();
  expect_handle_error ([] { PainterMybrushRef::adopt (nullptr); });
  gimp_object_set_name (GIMP_OBJECT (source.get ()), "");
  expect_handle_error ([&] { source.duplicate (); });
  gimp_object_set_name (GIMP_OBJECT (source.get ()), "Rejected handle source");
  gimp_data_clean (GIMP_DATA (source.get ()));
  WrongThreadCall call = { &source, false };
  auto *worker = g_thread_new ("named-handle-wrong-thread", handle_duplicate_wrong_thread, &call);
  g_thread_join (worker);
  g_assert_true (call.caught);
  auto valid = source.duplicate ();
  g_assert_nonnull (valid.get ());
  GError *error = nullptr;
  g_assert_true (gimp_painter_binding_close (G_OBJECT (source.get ()), &error));
  g_assert_no_error (error);
  expect_handle_error ([&] { source.duplicate (); });
  const auto after = source.snapshot ().encode ();
  g_assert_cmpstr (before.c_str (), ==, after.c_str ());
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source.get ())));
}

#ifdef GIMP_TEST_MYBRUSH_WRAP_PIXBUF_COPY
void handle_copy_failure_exception ()
{
  auto source = PainterMybrushRef::create ("Failed icon handle");
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  gdk_pixbuf_fill (icon, 0x197ba5ff);
  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_set_icon (source.get (), icon, &error));
  g_assert_no_error (error);
  gimp_data_clean (GIMP_DATA (source.get ()));
  const auto before = source.snapshot ().encode ();
  gimp_test_mybrush_copy_fault_arm (icon);
  expect_handle_error ([&] { source.duplicate (); });
  g_assert_true (gimp_test_mybrush_copy_fault_consumed ());
  const auto after = source.snapshot ().encode ();
  g_assert_cmpstr (before.c_str (), ==, after.c_str ());
  g_assert_cmpstr (gimp_object_get_name (source.get ()), ==, "Failed icon handle");
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source.get ())));
  auto recovered = source.duplicate ();
  g_assert_nonnull (recovered.get ());
  g_assert_cmpstr (gimp_object_get_name (recovered.get ()), ==, "Failed icon handle");
  g_object_unref (icon);
}
#endif
}

namespace {
static guint use_handle_begins, use_handle_ends;
static bool use_handle_throw;
void use_handle_begin (GimpPainterMybrush *)
{ ++use_handle_begins; if (use_handle_throw) throw std::runtime_error ("injected begin hook"); }
void use_handle_end (GimpPainterMybrush *)
{ ++use_handle_ends; if (use_handle_throw) throw std::runtime_error ("injected end hook"); }
void nested_use_handle ()
{
  static_assert (std::is_same<decltype (&gimp_mypaint_brush_begin_use), void (*)(GimpMypaintBrush *)>::value, "original begin signature");
  static_assert (std::is_same<decltype (&gimp_mypaint_brush_end_use), void (*)(GimpMypaintBrush *)>::value, "original end signature");
  auto owner = MypaintBrushRef::create ("Use handle");
  auto *klass = GIMP_MYPAINT_BRUSH_GET_CLASS (owner.get ());
  const auto old_begin = klass->begin_use, old_end = klass->end_use;
  klass->begin_use = use_handle_begin; klass->end_use = use_handle_end;
  use_handle_begins = use_handle_ends = 0; use_handle_throw = false;
  auto copy = owner;
  owner.begin_use (); copy.begin_use (); owner.end_use ();
  g_assert_cmpuint (use_handle_begins, ==, 1); g_assert_cmpuint (use_handle_ends, ==, 0);
  copy.end_use (); g_assert_cmpuint (use_handle_ends, ==, 1);
  // Exceptions in rebuilt C++ subclass hooks stop at the original C boundary;
  // transition count already changed, exactly like a reentrant hook observes.
  use_handle_throw = true;
  owner.begin_use (); owner.end_use ();
  use_handle_throw = false;
  owner.begin_use (); owner.end_use ();
  g_assert_cmpuint (use_handle_begins, ==, 3); g_assert_cmpuint (use_handle_ends, ==, 3);
  klass->begin_use = old_begin; klass->end_use = old_end;
}
}

extern "C" void
gimp_test_painter_mybrush_duplicate_handle_register (void)
{
  g_test_add_func ("/painter-mybrush/nested-use/handle-and-throwing-hooks", nested_use_handle);
  g_test_add_func ("/painter-mybrush/file-save/handle", file_save_handle);
  g_test_add_func ("/painter-mybrush/filename-load/handle", filename_load_handle);
  g_test_add_func ("/painter-mybrush/named-standard/cold-recreation", named_standard_cold_recreation);
  g_test_add_func ("/painter-mybrush/named-factory/handle", named_factory_handle);
  g_test_add_func ("/painter-mybrush/named-duplicate/handle-alias-resource", handle_alias_and_resource_independence);
  g_test_add_func ("/painter-mybrush/named-duplicate/handle-copy-move-ownership", handle_copy_move_ownership);
  g_test_add_func ("/painter-mybrush/named-duplicate/handle-failure-exceptions", handle_failure_exceptions);
#ifdef GIMP_TEST_MYBRUSH_WRAP_PIXBUF_COPY
  g_test_add_func ("/painter-mybrush/named-duplicate/handle-copy-failure-exception", handle_copy_failure_exception);
#endif
}
