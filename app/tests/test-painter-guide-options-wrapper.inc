/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tools/gimpperspectiveguidetool.hpp"
#include "tools/gimpperspectiveguidetool.hpp"
#include "core/gimptoolinfo.h"
extern "C" void gimp_test_guide_options_handles (Gimp *, GimpToolInfo *);
extern "C" void gimp_test_guide_options_handles (Gimp *gimp, GimpToolInfo *info)
{
  using GimpPainter::PerspectiveGuideOptionsRef;
  using GimpPainter::PerspectiveGuideToolRef;
  guint deaths = 0;
  auto owner = PerspectiveGuideOptionsRef::adopt (static_cast<GimpPerspectiveGuideOptions *> (
      g_object_new (GIMP_TYPE_PERSPECTIVE_GUIDE_OPTIONS, "gimp", gimp, nullptr)));
  g_object_weak_ref (G_OBJECT (owner.get ()), finalized, &deaths);
  auto copy = owner;
  auto retained = PerspectiveGuideOptionsRef::retain (owner.get ());
  owner = owner;
  auto& self = owner; owner = std::move (self);
  auto moved = std::move (owner);
  g_assert_false (bool (owner));
  copy.reset (); retained.reset ();
  g_assert_cmpuint (deaths, ==, 0);
  moved.reset (); g_assert_cmpuint (deaths, ==, 1);
  g_assert_false (bool (PerspectiveGuideOptionsRef::retain (nullptr)));
  g_assert_false (bool (PerspectiveGuideOptionsRef::adopt (nullptr)));
  auto wrong = GimpPainter::ObjectRef<GObject>::adopt (G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr)));
  for (bool adopt : {false, true}) {
    bool caught = false;
    try {
      auto *raw = reinterpret_cast<GimpPerspectiveGuideOptions *> (wrong.get ());
      auto rejected = adopt ? PerspectiveGuideOptionsRef::adopt (raw) : PerspectiveGuideOptionsRef::retain (raw);
    } catch (const GimpPainter::Error& error) { caught = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
    g_assert_true (caught); g_assert_cmpuint (wrong.get ()->ref_count, ==, 1);
  }
  auto registered = PerspectiveGuideOptionsRef::retain (GIMP_PERSPECTIVE_GUIDE_OPTIONS (info->tool_options));
  registered.reset ();
  g_assert_true (GIMP_IS_PERSPECTIVE_GUIDE_OPTIONS (info->tool_options));
  auto tool = PerspectiveGuideToolRef::adopt (static_cast<GimpPerspectiveGuideTool *> (
      g_object_new (GIMP_TYPE_PERSPECTIVE_GUIDE_TOOL, "tool-info", info, nullptr)));
  g_assert_false (g_object_is_floating (tool.get ()));
  g_object_weak_ref (G_OBJECT (tool.get ()), finalized, &deaths);
  auto tool_copy = tool;
  auto tool_retained = PerspectiveGuideToolRef::retain (tool.get ());
  tool.reset (); tool_copy.reset ();
  g_assert_cmpuint (deaths, ==, 1);
  tool_retained.reset (); g_assert_cmpuint (deaths, ==, 2);
  g_assert_false (bool (PerspectiveGuideToolRef::retain (nullptr)));
  g_assert_false (bool (PerspectiveGuideToolRef::adopt (nullptr)));
}
extern "C" void gimp_test_guide_register (GimpToolRegisterCallback, gpointer);
extern "C" void gimp_test_guide_register (GimpToolRegisterCallback callback, gpointer data)
{
  GimpPainter::PerspectiveGuideToolRef::register_type (callback, data);
}
