/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Exercise real global actions, not direct command callbacks, while the
 * selected display still exists but its native window owner is withdrawn. */
static void
transfer_screenshot (const gchar *name)
{
  const gchar *directory = g_getenv ("GIMP_TEST_TOOLBAR_TRANSFER_SHOTS");
  GdkWindow *root;
  GdkPixbuf *pixels;
  GError *error = NULL;
  gchar *path;
  gboolean painter_enabled;
  if (! directory)
    return;
  g_object_get (gimp->config, "painter-canvas-ui", &painter_enabled, NULL);
  g_assert_true (painter_enabled);
  g_assert_cmpint (g_mkdir_with_parents (directory, 0700), ==, 0);
  root = gdk_get_default_root_window ();
  g_assert_cmpint (gdk_window_get_width (root), ==, 1080);
  g_assert_cmpint (gdk_window_get_height (root), ==, 1920);
  gdk_display_sync (gdk_window_get_display (root));
  pixels = gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920);
  g_assert_nonnull (pixels);
  path = g_build_filename (directory, name, NULL);
  g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL));
  g_assert_no_error (error);
  g_free (path);
  g_object_unref (pixels);
}

static GtkWidget *
transfer_menu_item (GtkWidget *root, GimpAction *action)
{
  GtkWidget *found = NULL;
  if (g_object_get_data (G_OBJECT (root), "gimp-menu-action") == action)
    return root;
  if (GTK_IS_MENU_ITEM (root))
    {
      GtkWidget *submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (root));
      if (submenu)
        found = transfer_menu_item (submenu, action);
    }
  if (! found && GTK_IS_CONTAINER (root))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (root));
      for (GList *iter = children; iter && ! found; iter = iter->next)
        found = transfer_menu_item (iter->data, action);
      g_list_free (children);
    }
  return found;
}

static void
transfer_show_menu (GtkWidget *menu, GimpImageWindow *host)
{
  GdkEvent *event;
  event = gdk_event_new (GDK_BUTTON_PRESS);
  event->button.window = g_object_ref (gtk_widget_get_window (GTK_WIDGET (host)));
  event->button.send_event = TRUE;
  event->button.time = GDK_CURRENT_TIME;
  event->button.button = 1;
  gdk_event_set_device (event, gdk_seat_get_pointer
                        (gdk_display_get_default_seat (gtk_widget_get_display (GTK_WIDGET (host)))));
  gtk_menu_popup_at_widget (GTK_MENU (menu), GTK_WIDGET (host),
                            GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_NORTH_WEST, event);
  gdk_event_free (event);
  settle ();
}

typedef struct
{
  GimpImageWindow *host;
  GimpImageWindow *owner;
  GimpDisplayShell *shell;
  GimpImage *image;
  GimpUIManager *manager;
  GimpAction *guides;
  guint calls;
} TransferMembershipProbe;

static void
transfer_membership_probe (GtkWidget *widget, gpointer data)
{
  TransferMembershipProbe *probe = data;
  GVariant *state;
  if (widget != GTK_WIDGET (probe->shell) ||
      probe->calls ||
      gimp_display_shell_get_window (probe->shell) != probe->owner ||
      gimp_image_window_get_action_display (probe->host))
    return;
  probe->calls++;
  g_action_activate (G_ACTION (probe->guides), NULL);
  g_action_change_state (G_ACTION (probe->guides), g_variant_new_boolean (FALSE));
  gimp_ui_manager_update (probe->manager, gimp);
  g_assert_true (gimp_display_shell_get_show_guides (probe->shell));
  g_assert_true (gimp_image_get_simulation_bpc (probe->image));
  state = g_action_get_state (G_ACTION (probe->guides));
  g_assert_true (g_variant_get_boolean (state));
  g_variant_unref (state);
}

static void
transfer_global_actions (void)
{
  GimpDisplay *display = new_display ();
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  GimpImage *image = gimp_display_get_image (display);
  GimpImageWindow *owner = gimp_display_shell_get_window (shell);
  GimpImageWindow *host = find_host ();
  GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
  GimpAction *guides = gimp_ui_manager_find_action (manager, "view", "view-show-guides");
  GimpAction *close = gimp_ui_manager_find_action (manager, "view", "view-close");
  GVariant *before, *after;
  GtkWidget *item, *menu;
  gulong membership_hook;
  TransferMembershipProbe probe = {host, owner, shell, image, manager, guides, 0};

  select_display (display);
  if (g_getenv ("GIMP_TEST_TOOLBAR_TRANSFER_SHOTS"))
    {
      gtk_window_move (GTK_WINDOW (host), 0, 0);
      gtk_window_resize (GTK_WINDOW (host), 320, 1800);
      gtk_window_move (GTK_WINDOW (owner), 330, 0);
      gtk_window_resize (GTK_WINDOW (owner), 740, 1800);
      settle ();
    }
  gimp_display_shell_set_show_guides (shell, TRUE);
  gimp_image_set_simulation_bpc (image, TRUE);
  gimp_image_clean_all (image);
  gimp_ui_manager_update (manager, display);
  before = g_action_get_state (G_ACTION (guides));
  g_assert_true (g_variant_get_boolean (before));
  item = transfer_menu_item (GTK_WIDGET (host), guides);
  g_assert_nonnull (item);
  g_assert_true (GTK_IS_CHECK_MENU_ITEM (item));
  menu = gtk_widget_get_parent (item);
  g_assert_true (GTK_IS_MENU (menu));
  transfer_show_menu (menu, host);
  g_assert_true (gtk_widget_get_mapped (menu));
  g_assert_true (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)));
  transfer_screenshot ("01-open-native-menu.png");
  g_object_ref (item);
  g_object_ref (menu);
  g_object_ref (display);
  g_object_ref (image);
  g_object_ref (shell);
  g_object_ref (owner);
  /* GTK unmaps the shell before clearing its ancestor during real notebook
   * withdrawal. Native membership was already removed; assert that precise
   * gap in the observer rather than inferring it from an idle callback. */
  g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (shell)));
  membership_hook = g_signal_connect (shell, "unmap",
                                      G_CALLBACK (transfer_membership_probe), &probe);
  gimp_image_window_remove_shell (owner, shell);
  g_signal_handler_disconnect (shell, membership_hook);
  g_assert_cmpuint (probe.calls, ==, 1);
  g_assert_null (gimp_display_shell_get_window (shell));
  assert_route (host, NULL);
  /* No iteration: this is the synchronous transfer transaction gap. */
  g_action_activate (G_ACTION (guides), NULL);
  g_assert_true (gimp_display_shell_get_show_guides (shell));
  after = g_action_get_state (G_ACTION (guides));
  g_assert_true (g_variant_equal (before, after));
  g_variant_unref (after);
  gtk_menu_item_activate (GTK_MENU_ITEM (item));
  g_assert_true (gimp_display_shell_get_show_guides (shell));
  g_assert_true (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)));
  after = g_action_get_state (G_ACTION (guides));
  g_assert_true (g_variant_equal (before, after));
  g_variant_unref (after);
  g_action_change_state (G_ACTION (guides), g_variant_new_boolean (FALSE));
  g_assert_true (gimp_display_shell_get_show_guides (shell));
  g_action_activate (G_ACTION (close), NULL);
  g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (display)));
  g_assert_true (gimp_display_get_image (display) == image);
  gimp_ui_manager_update (manager, gimp);
  g_assert_true (gimp_image_get_simulation_bpc (image));
  g_assert_true (gimp_display_shell_get_show_guides (shell));
  after = g_action_get_state (G_ACTION (guides));
  g_assert_true (g_variant_equal (before, after));
  g_variant_unref (after);
  g_variant_unref (before);
  transfer_screenshot ("02-transfer-actions-blocked.png");
  gtk_menu_popdown (GTK_MENU (menu));
  gimp_image_window_add_shell (owner, shell);
  select_display (display);
  assert_route (host, display);
  gimp_ui_manager_update (manager, display);
  g_assert_true (gimp_display_shell_get_show_guides (shell));
  g_action_activate (G_ACTION (guides), NULL);
  g_assert_false (gimp_display_shell_get_show_guides (shell));
  settle ();
  transfer_show_menu (menu, host);
  g_assert_false (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)));
  transfer_screenshot ("03-owner-restored-action-resumed.png");
  gtk_menu_popdown (GTK_MENU (menu));
  g_object_unref (menu);
  g_object_unref (item);
  g_object_unref (owner);
  g_object_unref (shell);
  g_object_unref (image);
  g_object_unref (display);
  close_display (display);
}

typedef struct
{
  GimpDisplayShell *shell;
  GimpImageWindow *owner;
  gboolean fired;
  GimpColorRenderingIntent intent_at_withdrawal;
} TransferRadioProbe;

static void
transfer_radio_notify (GObject *object, GParamSpec *pspec, gpointer data)
{
  TransferRadioProbe *probe = data;
  (void) pspec;
  if (probe->fired || ! gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (object)))
    return;
  probe->fired = TRUE;
  gimp_image_window_remove_shell (probe->owner, probe->shell);
  probe->intent_at_withdrawal = gimp_color_config_get_display_intent
    (gimp_display_shell_get_color_config (probe->shell));
}

static void
transfer_radio_notification (void)
{
  GimpDisplay *display = new_display ();
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  GimpImageWindow *owner = gimp_display_shell_get_window (shell);
  GimpUIManager *manager = gimp_test_utils_get_ui_manager (gimp);
  GimpAction *radio = gimp_ui_manager_find_action (manager, "view", "view-display-intent-saturation");
  GimpColorConfig *config = gimp_display_shell_get_color_config (shell);
  GimpAction *relative = gimp_ui_manager_find_action (manager, "view", "view-display-intent-relative-colorimetric");
  GtkWidget *relative_item;
  GtkWidget *saturation_item;
  TransferRadioProbe probe = {shell, owner, FALSE, 0};
  GVariant *state;
  gulong handler;

  select_display (display);
  g_object_set (config, "display-rendering-intent", GIMP_COLOR_RENDERING_INTENT_PERCEPTUAL, NULL);
  gimp_ui_manager_update (manager, display);
  relative_item = transfer_menu_item (GTK_WIDGET (find_host ()), relative);
  saturation_item = transfer_menu_item (GTK_WIDGET (find_host ()), radio);
  g_assert_true (GTK_IS_RADIO_MENU_ITEM (relative_item));
  g_assert_true (GTK_IS_RADIO_MENU_ITEM (saturation_item));
  g_object_ref (shell);
  g_object_ref (owner);
  handler = g_signal_connect (radio, "notify::active", G_CALLBACK (transfer_radio_notify), &probe);
  g_action_activate (G_ACTION (radio), NULL);
  g_signal_handler_disconnect (radio, handler);
  g_assert_true (probe.fired);
  g_assert_null (gimp_display_shell_get_window (shell));
  /* A command completed before withdrawal is legitimate. No command may
   * change the model afterwards, and published radio state must agree. */
  g_assert_cmpint (gimp_color_config_get_display_intent (config), ==, probe.intent_at_withdrawal);
  state = g_action_get_state (G_ACTION (radio));
  g_assert_cmpint (g_variant_get_int32 (state), ==, probe.intent_at_withdrawal);
  g_variant_unref (state);
  gtk_menu_item_activate (GTK_MENU_ITEM (relative_item));
  g_assert_false (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (relative_item)));
  g_assert_true (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (saturation_item)));
  g_action_change_state (G_ACTION (radio), g_variant_new_int32 (GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC));
  g_assert_cmpint (gimp_color_config_get_display_intent (config), ==, probe.intent_at_withdrawal);
  state = g_action_get_state (G_ACTION (radio));
  g_assert_cmpint (g_variant_get_int32 (state), ==, probe.intent_at_withdrawal);
  g_variant_unref (state);
  gimp_image_window_add_shell (owner, shell);
  select_display (display);
  gimp_ui_manager_update (manager, display);
  state = g_action_get_state (G_ACTION (radio));
  g_assert_cmpint (g_variant_get_int32 (state), ==, gimp_color_config_get_display_intent (config));
  g_variant_unref (state);
  g_object_unref (owner);
  g_object_unref (shell);
  close_display (display);
}

static void
transfer_count_change (GimpAction *action, GVariant *value, gpointer data)
{
  (void) action;
  (void) value;
  (*(guint *) data)++;
}

static void
transfer_action_added (GimpActionGroup *group, const gchar *name, gpointer data)
{
  GimpAction *action = gimp_action_group_get_action (group, name);
  (void) data;
  g_assert_nonnull (action);
  g_action_activate (G_ACTION (action), NULL);
  g_action_change_state (G_ACTION (action), g_variant_new_boolean (TRUE));
}

static void
transfer_action_publication (void)
{
  GimpDisplay *display = new_display ();
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  GimpImageWindow *owner = gimp_display_shell_get_window (shell);
  GimpActionGroup *group = gimp_action_group_new (gimp, "transfer-publication-test", NULL, NULL, NULL, NULL);
  GimpAction *action = gimp_toggle_action_new ("transfer-published-action", "Transfer test", NULL, NULL, NULL, NULL, gimp_get_user_context (gimp));
  guint changes = 0;

  select_display (display);
  g_object_ref (shell);
  g_object_ref (owner);
  gimp_image_window_remove_shell (owner, shell);
  g_signal_connect (action, "change-state", G_CALLBACK (transfer_count_change), &changes);
  g_signal_connect (group, "action-added", G_CALLBACK (transfer_action_added), NULL);
  gimp_action_group_add_action (group, action);
  g_assert_cmpuint (changes, ==, 0);
  g_assert_false (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (action)));
  gimp_image_window_add_shell (owner, shell);
  select_display (display);
  g_action_activate (G_ACTION (action), NULL);
  g_assert_cmpuint (changes, ==, 1);
  g_assert_true (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (action)));
  /* An action can outlive its group. The dispatch guard must not dereference
   * the historical non-owning group back-pointer. */
  g_object_unref (group);
  g_action_activate (G_ACTION (action), NULL);
  g_assert_cmpuint (changes, ==, 2);
  g_object_unref (action);
  g_object_unref (owner);
  g_object_unref (shell);
  close_display (display);
}

/* A command observer may synchronously start a newer selection. The old
 * publisher must not overwrite that selection through later group callbacks. */
typedef struct
{
  GimpRadioAction *anchor;
  GimpRadioAction *moved;
  guint mode;
  guint obsolete;
  guint latest;
  gboolean nested;
} TransferRadioReentry;

static void
transfer_radio_command_reentry (GimpAction *action, GVariant *value, gpointer data)
{
  TransferRadioReentry *probe = data;
  (void) action;
  if (g_variant_get_int32 (value) == 1)
    {
      probe->obsolete++;
      if (! probe->nested)
        {
          probe->nested = TRUE;
          if (probe->mode == 0)
            gimp_radio_action_set_current_value (probe->anchor, 2);
          else if (probe->mode == 1)
            gimp_radio_action_set_group (probe->moved, NULL);
          else
            g_object_run_dispose (G_OBJECT (probe->anchor));
        }
    }
  else if (g_variant_get_int32 (value) == 2)
    probe->latest++;
}

static void
transfer_radio_command_supersession (void)
{
  const gchar *names[] = {"transfer-radio-zero", "transfer-radio-one", "transfer-radio-two"};
  for (guint mode = 0; mode < 3; mode++)
    {
      GimpAction *actions[3];
      TransferRadioReentry probe = {NULL, NULL, mode, 0, 0, FALSE};
      for (guint i = 0; i < 3; i++)
        {
          actions[i] = gimp_radio_action_new (names[i], names[i], NULL, NULL, NULL,
                                              NULL, i, gimp_get_user_context (gimp));
          gimp_radio_action_set_group (GIMP_RADIO_ACTION (actions[i]),
            i ? gimp_radio_action_get_group (GIMP_RADIO_ACTION (actions[0])) : NULL);
        }
      probe.anchor = GIMP_RADIO_ACTION (actions[0]);
      probe.moved = GIMP_RADIO_ACTION (actions[2]);
      for (guint i = 0; i < 3; i++)
        g_signal_connect (actions[i], "change-state",
                          G_CALLBACK (transfer_radio_command_reentry), &probe);
      gimp_radio_action_set_current_value (probe.anchor, 1);
      g_assert_true (probe.nested);
      g_assert_cmpuint (probe.obsolete, ==, 1);
      g_assert_cmpuint (probe.latest, ==, mode == 0 ? 3 : mode == 1 ? 1 : 0);
      if (mode == 2)
        g_assert_null (gimp_radio_action_get_group (probe.anchor));
      for (guint i = 0; i < 3; i++)
        {
          if (mode != 2 || i != 0)
            {
              gint expected = mode == 0 || (mode == 1 && i == 2) ? 2 : 1;
              g_assert_cmpint (gimp_radio_action_get_current_value (GIMP_RADIO_ACTION (actions[i])), ==, expected);
              g_assert_cmpint (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (actions[i])), ==,
                              mode == 0 ? i == 2 : (i == 1 || (mode == 1 && i == 2)));
            }
        }
      /* Validate the complete live group before cleanup: disposing an active
       * member legitimately selects a fallback in its remaining group. */
      for (guint i = 0; i < 3; i++)
        g_signal_handlers_disconnect_by_data (actions[i], &probe);
      for (guint i = 0; i < 3; i++)
        g_object_unref (actions[i]);
    }
}
