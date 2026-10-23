/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "tools/gimpbucketfillbrushtool.hpp"
#include "tools/gimpbucketfillbrushtool.hpp"
#include "core/gimptoolinfo.h"
#include <stdexcept>
#include <new>
extern "C" void gimp_test_bucket_handles (GimpToolInfo *);
extern "C" void gimp_test_bucket_cpp_register (GimpToolRegisterCallback, gpointer);
extern "C" void gimp_test_bucket_cpp_exceptions (void);
extern "C" void __real_gimp_bucket_fill_brush_tool_register (GimpToolRegisterCallback, gpointer);
extern "C" void __wrap_gimp_bucket_fill_brush_tool_register (GimpToolRegisterCallback, gpointer);
static guint entry_calls;
extern "C" void __wrap_gimp_bucket_fill_brush_tool_register (GimpToolRegisterCallback callback, gpointer data)
{ ++entry_calls; __real_gimp_bucket_fill_brush_tool_register (callback, data); }
extern "C" void gimp_test_bucket_cpp_register (GimpToolRegisterCallback callback, gpointer data)
{
  entry_calls = 0;
  GimpPainter::BucketFillBrushToolRef::register_type (callback, data);
  g_assert_cmpuint (entry_calls, ==, 1);
}
static void finalized (gpointer data, GObject *) { ++*static_cast<guint *> (data); }
extern "C" void gimp_test_bucket_handles (GimpToolInfo *info)
{
  using GimpPainter::BucketFillBrushToolRef;
  guint deaths = 0;
  auto tool = BucketFillBrushToolRef::adopt (static_cast<GimpBucketFillBrushTool *> (
    g_object_new (GIMP_TYPE_BUCKET_FILL_BRUSH_TOOL, "tool-info", info, nullptr)));
  g_object_weak_ref (G_OBJECT (tool.get ()), finalized, &deaths);
  auto copy = tool; auto retained = BucketFillBrushToolRef::retain (tool.get ());
  auto& self = tool; tool = self; tool = std::move (self);
  auto moved = std::move (tool); g_assert_false (bool (tool));
  copy.reset (); retained.reset (); g_assert_cmpuint (deaths, ==, 0);
  moved.reset (); g_assert_cmpuint (deaths, ==, 1);
  g_assert_false (bool (BucketFillBrushToolRef::retain (nullptr)));
  g_assert_false (bool (BucketFillBrushToolRef::adopt (nullptr)));
  auto wrong = GimpPainter::ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  for (bool adopt : {false, true}) {
    bool caught = false;
    try {
      auto *raw = reinterpret_cast<GimpBucketFillBrushTool *> (wrong.get ());
      auto rejected = adopt ? BucketFillBrushToolRef::adopt (raw) : BucketFillBrushToolRef::retain (raw);
    } catch (const GimpPainter::Error& error) { caught = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
    g_assert_true (caught); g_assert_cmpuint (wrong.get ()->ref_count, ==, 1);
  }
}
static guint throw_kind, throw_calls;
static void throwing_callback (GType, GType, GimpToolOptionsGUIFunc, GimpContextPropMask,
  const gchar *, const gchar *, const gchar *, const gchar *, const gchar *, const gchar *, const gchar *, const gchar *, gpointer)
{
  ++throw_calls;
  switch (throw_kind) {
  case 0: throw GimpPainter::Error (GIMP_PAINTER_ERROR_INVALID_STATE, "test register failure");
  case 1: throw std::bad_alloc ();
  case 2: throw std::runtime_error ("test register failure");
  default: throw 17;
  }
}
extern "C" void gimp_test_bucket_cpp_exceptions (void)
{
  for (throw_kind = 0; throw_kind < 4; ++throw_kind) {
    throw_calls = 0;
    gimp_bucket_fill_brush_tool_register (throwing_callback, nullptr);
    g_assert_cmpuint (throw_calls, ==, 1);
    gimp_fill_brush_tool_register (throwing_callback, nullptr);
    g_assert_cmpuint (throw_calls, ==, 2);
  }
}
