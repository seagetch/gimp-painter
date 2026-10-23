/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "actions/layer-presets-actions.h"
#include "presets/preset-factory-gui.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
void preset_gui_handle_test (GimpActionGroup *group);
static void signature_and_null (void)
{
  void (*entry) (GimpActionGroup *) = preset_factory_gui_action_group_entry_point;
  entry (NULL); entry (NULL);
}
static void no_op_groups (void)
{
  GimpActionGroup *group = gimp_action_group_new (gimp, "preset-entry-contract", "Contract", NULL, NULL, NULL);
  GList *before;
  GList *after;
  guint i;
  before = gimp_action_group_list_actions (group);
  g_assert_null (before);
  preset_factory_gui_action_group_entry_point (group);
  after = gimp_action_group_list_actions (group);
  g_assert_null (after);
  /* Real native actions remain independent and unchanged by this old no-op. */
  layer_presets_actions_setup (group);
  before = gimp_action_group_list_actions (group);
  g_assert_cmpuint (g_list_length (before), ==, 2);
  for (i = 0; i < 3; ++i) preset_factory_gui_action_group_entry_point (group);
  after = gimp_action_group_list_actions (group);
  g_assert_cmpuint (g_list_length (after), ==, 2);
  g_assert_true (g_list_find (after, before->data) != NULL);
  g_assert_true (g_list_find (after, before->next->data) != NULL);
  preset_gui_handle_test (group);
  g_list_free (before); g_list_free (after);
  g_object_run_dispose (G_OBJECT (group));
  preset_factory_gui_action_group_entry_point (group);
  g_object_unref (group);
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/preset-gui-entry/signature-null", signature_and_null);
  g_test_add_func ("/preset-gui-entry/no-op-groups-handle", no_op_groups);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
