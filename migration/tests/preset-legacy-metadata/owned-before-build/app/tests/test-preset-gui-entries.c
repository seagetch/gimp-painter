/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpstringaction.h"
#include "actions/layer-presets-actions.h"
#include "presets/preset-factory-gui.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
void preset_gui_handle_test (GimpActionGroup *group);
void preset_gui_dialog_handle_test (GimpActionGroup *group);
void preset_gui_preferences_handle_test (Gimp *application);
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
static void dialog_metadata (void)
{
  void (*entry) (GimpActionGroup *) = preset_factory_gui_dialogs_actions_entry_point;
  GimpActionGroup *group = gimp_action_group_new (gimp, "restored-preset-dialog", "Restored", NULL, NULL, NULL);
  GimpActionGroup *other = gimp_action_group_new (gimp, "typed-preset-dialog", "Typed", NULL, NULL, NULL);
  GimpAction *action;
  GList *actions;

  g_test_expect_message ("Gimp-Actions", G_LOG_LEVEL_CRITICAL, "*GIMP_IS_ACTION_GROUP*failed*");
  entry (NULL);
  g_test_assert_expected_messages ();
  entry (group);
  actions = gimp_action_group_list_actions (group);
  g_assert_cmpuint (g_list_length (actions), ==, 1); g_list_free (actions);
  action = gimp_action_group_get_action (group, "dialogs-layer-presets");
  g_assert_true (GIMP_IS_STRING_ACTION (action));
  g_assert_cmpstr (GIMP_STRING_ACTION (action)->value, ==, "gimp-layer-preset-list");
  g_assert_cmpstr (gimp_action_get_label (action), ==,
    g_dpgettext2 (NULL, "dialogs-action", "Layer presets"));
  g_assert_cmpstr (gimp_action_get_short_label (action), ==,
    g_dpgettext2 (NULL, "dialogs-action", "Layer presets"));
  g_assert_cmpstr (gimp_action_get_tooltip (action), ==,
    g_dpgettext2 (NULL, "dialogs-action", "Open layer presets dialog"));
  g_assert_cmpstr (gimp_action_get_help_id (action), ==, "gimp-layer-preset-dialog");
  /* Delegate duplicate policy to the same native provider; existing identity wins. */
  entry (group);
  g_assert_true (gimp_action_group_get_action (group, "dialogs-layer-presets") == action);
  preset_gui_dialog_handle_test (other);
  g_assert_nonnull (gimp_action_group_get_action (other, "dialogs-layer-presets"));
  g_assert_true (gimp_action_group_get_action (other, "dialogs-layer-presets") != action);
  g_object_unref (group); g_object_unref (other);
}
static void release_on_action (GActionGroup *group, const gchar *name, gpointer unused)
{
  (void) unused;
  g_assert_cmpstr (name, ==, "dialogs-layer-presets");
  g_object_unref (group);
}
static void dispose_on_action (GActionGroup *group, const gchar *name, gpointer unused)
{
  (void) name; (void) unused;
  g_object_run_dispose (G_OBJECT (group));
}
static void dialog_owner_callbacks (void)
{
  GimpActionGroup *group = gimp_action_group_new (gimp, "released-preset-dialog", "Released", NULL, NULL, NULL);
  gpointer weak = group;
  g_object_add_weak_pointer (G_OBJECT (group), &weak);
  g_signal_connect (group, "action-added", G_CALLBACK (release_on_action), NULL);
  preset_factory_gui_dialogs_actions_entry_point (group);
  g_assert_null (weak);
  group = gimp_action_group_new (gimp, "disposed-preset-dialog", "Disposed", NULL, NULL, NULL);
  g_signal_connect (group, "action-added", G_CALLBACK (dispose_on_action), NULL);
  preset_factory_gui_dialogs_actions_entry_point (group);
  g_assert_null (gimp_action_group_list_actions (group));
  g_object_unref (group);
}
static void preferences_metadata (void)
{
  GArray *(*entry_point) (void) = preset_factory_gui_prefs_entry_point;
  GArray *first = entry_point ();
  GArray *second = entry_point ();
  PrefsDialogEntry *entry;
  const gchar *static_label;
  g_assert_nonnull (first); g_assert_nonnull (second);
  g_assert_true (first != second); g_assert_true (first->data != second->data);
  g_assert_cmpuint (first->len, ==, 1);
  g_assert_cmpuint (g_array_get_element_size (first), ==, sizeof (PrefsDialogEntry));
  entry = &g_array_index (first, PrefsDialogEntry, 0);
  g_assert_cmpstr (entry->tree_label, ==, "Layer Presets");
  g_assert_cmpstr (entry->label, ==, "Layer presets Folders");
  g_assert_cmpstr (entry->icon, ==, "folders-layer-presets");
  g_assert_cmpstr (entry->help_data, ==, "");
  g_assert_cmpstr (entry->fs_label, ==, "Select Layer Presets Folders");
  g_assert_cmpstr (entry->path_property_name, ==, "layer-presets-path");
  g_assert_cmpstr (entry->writable_property_name, ==, "layer-presets-path-writable");
  g_assert_nonnull (g_object_class_find_property (G_OBJECT_GET_CLASS (gimp->config), entry->path_property_name));
  g_assert_nonnull (g_object_class_find_property (G_OBJECT_GET_CLASS (gimp->config), entry->writable_property_name));
  static_label = entry->label;
  entry->label = "caller-local replacement";
  g_assert_cmpstr (g_array_index (second, PrefsDialogEntry, 0).label, ==, static_label);
  g_array_unref (first); g_array_unref (second);
  /* Static strings outlive the caller-owned array storage. */
  g_assert_cmpstr (static_label, ==, "Layer presets Folders");
}
static void preferences_handle (void)
{
  preset_gui_preferences_handle_test (gimp);
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/preset-gui-entry/signature-null", signature_and_null);
  g_test_add_func ("/preset-gui-entry/no-op-groups-handle", no_op_groups);
  g_test_add_func ("/preset-gui-entry/dialog-metadata", dialog_metadata);
  g_test_add_func ("/preset-gui-entry/dialog-owner-callbacks", dialog_owner_callbacks);
  g_test_add_func ("/preset-gui-entry/preferences-metadata", preferences_metadata);
  g_test_add_func ("/preset-gui-entry/preferences-handle", preferences_handle);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);
  return result;
}
