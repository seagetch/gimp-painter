/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Retired tool groups may outlive the root list through oncanvas UI refs. */
static GPtrArray *retain_tool_groups (void)
{
  GPtrArray *groups = g_ptr_array_new_with_free_func (g_object_unref);
  for (gint n = 0; n < gimp_container_get_n_children (gimp->tool_item_list); n++)
    {
      GimpObject *item = gimp_container_get_child_by_index (gimp->tool_item_list, n);
      if (GIMP_IS_TOOL_GROUP (item)) g_ptr_array_add (groups, g_object_ref (item));
    }
  g_assert_cmpuint (groups->len, >, 0);
  return groups;
}
static void check_retired_tool_groups (GPtrArray *groups)
{
  for (guint n = 0; n < groups->len; n++)
    {
      GimpToolGroup *group = g_ptr_array_index (groups, n);
      g_assert_false (gimp_container_have (gimp->tool_item_list, GIMP_OBJECT (group)));
      g_assert_true (gimp_container_is_empty (gimp_viewable_get_children (GIMP_VIEWABLE (group))));
      g_assert_null (gimp_tool_group_get_active_tool (group));
    }
  for (gint n = 0; n < gimp_container_get_n_children (gimp->tool_info_list); n++)
    {
      GimpObject *tool_info = gimp_container_get_child_by_index (gimp->tool_info_list, n);
      GimpViewable *parent = gimp_viewable_get_parent (GIMP_VIEWABLE (tool_info));
      if (parent)
        {
          g_assert_true (gimp_container_have (gimp->tool_item_list, GIMP_OBJECT (parent)));
          g_assert_true (gimp_container_have (gimp_viewable_get_children (parent), tool_info));
        }
    }
  g_ptr_array_unref (groups);
}
static void retained_groups_deserialize (void)
{
  gchar *text = NULL;
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "migration", "fixtures", "legacy-perspective", "gimp3-before-ruler-toolrc", NULL);
  GError *error = NULL;
  create_image ();
  g_assert_true (g_file_get_contents (path, &text, NULL, &error)); g_assert_no_error (error);
  for (guint n = 0; n < 3; n++)
    {
      GPtrArray *groups = retain_tool_groups ();
      GScanner *scanner = gimp_scanner_new_string (text, -1, &error); g_assert_no_error (error);
      /* Cover both a live root and orphan groups retained after caller clear. */
      if (n == 1) gimp_container_clear (gimp->tool_item_list);
      g_assert_true (gimp_tools_deserialize (gimp, gimp->tool_item_list, scanner));
      g_assert_no_error (error); gimp_scanner_unref (scanner);
      check_retired_tool_groups (groups);
      GimpObject *first = gimp_container_get_child_by_index (gimp->tool_item_list, 0);
      g_assert_true (GIMP_IS_TOOL_GROUP (first));
      g_assert_cmpstr (gimp_tool_group_get_active_tool (GIMP_TOOL_GROUP (first)), ==, "gimp-move-tool");
    }
  g_free (text); g_free (path); close_image ();
}
static void retained_groups_reset (void)
{
  create_image ();
  for (guint n = 0; n < 3; n++)
    {
      GPtrArray *groups = retain_tool_groups ();
      gimp_tools_reset (gimp, gimp->tool_item_list, FALSE);
      check_retired_tool_groups (groups);
      g_assert_false (gimp_container_is_empty (gimp->tool_item_list));
    }
  close_image ();
}
