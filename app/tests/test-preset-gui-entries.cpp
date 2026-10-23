/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
}
#include "presets/preset-factory-gui.hpp"
#include "presets/preset-factory-gui.hpp"
extern "C" void preset_gui_handle_test (GimpActionGroup *group);
extern "C" void preset_gui_handle_test (GimpActionGroup *group)
{
  using namespace GimpPainter;
  ActionGroupRef empty; empty.register_preset_actions ();
  auto owner = ActionGroupRef::retain (group);
  auto copy = owner;
  owner.reset ();
  g_assert_true (copy.get () == group);
  copy.register_preset_actions ();
  g_assert_cmpuint (TypeTraits<GimpActionGroup>::type (), ==, gimp_action_group_get_type ());
  auto *raw = gimp_action_group_new (group->gimp, "preset-handle-owned", "Owned", nullptr, nullptr, nullptr);
  gpointer weak = raw;
  g_object_add_weak_pointer (G_OBJECT (raw), &weak);
  auto adopted = ActionGroupRef::adopt (raw);
  auto retained = ActionGroupRef::retain (raw);
  adopted.reset ();
  g_assert_nonnull (weak);
  retained.register_preset_actions ();
  retained.reset ();
  g_assert_null (weak);
  auto *wrong = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  bool rejected = false;
  try { auto bad = ActionGroupRef::retain (reinterpret_cast<GimpActionGroup *> (wrong)); }
  catch (const Error &) { rejected = true; }
  g_assert_true (rejected);
  g_object_unref (wrong);
}
