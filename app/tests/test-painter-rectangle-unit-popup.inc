/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Real input regression for native unit menus nested in rectangle details.
 * XTest is loaded at runtime, so non-X11 builds keep no new link dependency. */
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <gmodule.h>

typedef struct
{
  GModule *module;
  gint (*motion) (gpointer, gint, gint, gint, gulong);
  gint (*button) (gpointer, guint, gint, gulong);
  gint (*key)    (gpointer, guint, gint, gulong);
} RectanglePopupInput;

static void
rectangle_popup_settle (void)
{
  gint64 until = g_get_monotonic_time () + 100000;
  do
    {
      g_main_context_iteration (NULL, FALSE);
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < until);
}

static void
rectangle_popup_assert_no_grabs (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkSeat *seat = gdk_display_get_default_seat (display);

  g_assert_null (gtk_grab_get_current ());
  g_assert_false (gdk_display_device_is_grabbed (display, gdk_seat_get_pointer (seat)));
  g_assert_false (gdk_display_device_is_grabbed (display, gdk_seat_get_keyboard (seat)));
}

static void
rectangle_popup_click (RectanglePopupInput *input, GtkWidget *widget, gint x, gint y)
{
  GtkWidget *top = gtk_widget_get_toplevel (widget);
  GdkDisplay *display = gtk_widget_get_display (widget);
  gpointer native = gdk_x11_display_get_xdisplay (display);
  gint root_x, root_y, local_x, local_y;

  g_assert_true (gtk_widget_translate_coordinates (widget, top, x, y, &local_x, &local_y));
  gdk_window_get_origin (gtk_widget_get_window (top), &root_x, &root_y);
  input->motion (native, -1, root_x + local_x, root_y + local_y, 0);
  gdk_display_flush (display);
  rectangle_popup_settle ();
  input->button (native, 1, TRUE, 0);
  gdk_display_flush (display);
  rectangle_popup_settle ();
  input->button (native, 1, FALSE, 0);
  gdk_display_flush (display);
  rectangle_popup_settle ();
}

static void
rectangle_popup_click_center (RectanglePopupInput *input, GtkWidget *widget)
{
  rectangle_popup_click (input, widget,
                          gtk_widget_get_allocated_width (widget) / 2,
                          gtk_widget_get_allocated_height (widget) / 2);
}

static void
rectangle_popup_key (RectanglePopupInput *input, GtkWidget *widget, guint keyval)
{
  GdkDisplay *display = gtk_widget_get_display (widget);
  gpointer native = gdk_x11_display_get_xdisplay (display);
  GdkKeymapKey *keys = NULL;
  gint count = 0;

  g_assert_true (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display),
                                                   keyval, &keys, &count));
  g_assert_cmpint (count, >, 0);
  input->key (native, keys[0].keycode, TRUE, 0);
  input->key (native, keys[0].keycode, FALSE, 0);
  gdk_display_flush (display);
  rectangle_popup_settle ();
  g_free (keys);
}

static void
rectangle_popup_reveal (GtkWidget *widget)
{
  GtkWidget *scroll = gtk_widget_get_ancestor (widget, GTK_TYPE_SCROLLED_WINDOW);
  GtkWidget *viewport;
  GtkAdjustment *adjustment;
  gint x, y;
  gdouble value;

  g_assert_nonnull (scroll);
  gtk_widget_grab_focus (widget);
  rectangle_popup_settle ();
  viewport = gtk_bin_get_child (GTK_BIN (scroll));
  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroll));
  g_assert_true (gtk_widget_translate_coordinates (widget, viewport, 0, 0, &x, &y));
  value = gtk_adjustment_get_value (adjustment) + y +
          gtk_widget_get_allocated_height (widget) / 2.0 -
          gtk_adjustment_get_page_size (adjustment) / 2.0;
  gtk_adjustment_set_value (adjustment,
                            CLAMP (value, gtk_adjustment_get_lower (adjustment),
                                   gtk_adjustment_get_upper (adjustment) -
                                   gtk_adjustment_get_page_size (adjustment)));
  rectangle_popup_settle ();
}

static void
rectangle_popup_assert_format (GtkWidget *combo, gboolean shown)
{
  GtkCellArea *area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (combo));
  GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (combo));

  g_assert_cmpint (g_list_length (cells), ==, 1);
  g_assert_cmpint (gtk_cell_area_attribute_get_column (area, cells->data, "text"),
                  ==, shown ? GIMP_UNIT_STORE_UNIT_LONG_FORMAT :
                              GIMP_UNIT_STORE_UNIT_SHORT_FORMAT);
  g_list_free (cells);
}

static void
rectangle_popup_assert_menu (GtkWidget *combo, GtkWidget *popover, gboolean shown)
{
  gboolean actual = FALSE;
  GtkWidget *grab = gtk_grab_get_current ();
  g_object_get (combo, "popup-shown", &actual, NULL);
  g_assert_cmpint (actual, ==, shown);
  rectangle_popup_assert_format (combo, shown);
  if (popover)
    {
      g_assert_true (gtk_popover_get_modal (GTK_POPOVER (popover)));
      g_assert_true (gtk_widget_get_visible (popover));
    }
  if (shown)
    {
      g_assert_true (GTK_IS_MENU (grab));
      g_assert_true (gtk_widget_get_mapped (grab));
      {
        AtkObject *accessible = gtk_combo_box_get_popup_accessible (GTK_COMBO_BOX (combo));
        GtkWidget *menu, *top;
        gint minimum = 0, natural = 0;
        g_assert_true (GTK_IS_ACCESSIBLE (accessible));
        menu = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
        g_assert_true (GTK_IS_MENU (menu));
        g_assert_true (menu == grab);
        top = gtk_widget_get_toplevel (menu);
        gtk_widget_get_preferred_width (menu, &minimum, &natural);
        g_test_message ("Native unit menu width: allocated=%d minimum=%d natural=%d top=%d window=%d top-window=%d",
                         gtk_widget_get_allocated_width (menu), minimum, natural,
                         gtk_widget_get_allocated_width (top),
                         gdk_window_get_width (gtk_widget_get_window (menu)),
                         gdk_window_get_width (gtk_widget_get_window (top)));
        g_assert_cmpint (gtk_widget_get_allocated_width (menu), >=, natural);
        g_assert_cmpint (gdk_window_get_width (gtk_widget_get_window (top)), >=, natural);
      }
    }
  else
    {
      /* GtkMenu releases its native X grab on popdown. The resulting host
       * focus event restores GtkPopover's modal GTK grab asynchronously;
       * a fixed input delay is not a completion condition under load.
       * Wait for that exact state, retaining a bounded failure and the
       * original identity oracle (including NULL for non-popover hosts). */
      gint64 deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
      while (grab != popover && g_get_monotonic_time () < deadline)
        {
          g_main_context_iteration (NULL, FALSE);
          g_usleep (1000);
          grab = gtk_grab_get_current ();
        }
      g_assert_true (grab == popover);
    }
}

/* Replacing a native model and restyling must keep the menu hook live. These
 * are unbound native controls, so changing the model does not bypass a
 * property widget's externally-owned model contract. */
static void
rectangle_popup_native_lifetimes (RectanglePopupInput *input)
{
  for (guint scenario = 0; scenario < 4; scenario++)
    {
      GtkWidget *host = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      GtkWidget *button = NULL, *popover = NULL;
      GtkWidget *combo = gimp_unit_combo_box_new ();
      GtkWidget *weak_combo = combo, *weak_host = host;
      GtkCssProvider *style = gtk_css_provider_new ();
      GimpUnitStore *replacement;
      gint compact_minimum, compact_natural, closed_minimum, closed_natural;

      g_test_message ("Native popup lifecycle scenario %u", scenario);
      gtk_window_set_default_size (GTK_WINDOW (host), 500, 500);
      gtk_container_set_border_width (GTK_CONTAINER (root), 20);
      gtk_container_add (GTK_CONTAINER (host), root);
      if (scenario)
        {
          GtkWidget *body = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
          button = gtk_menu_button_new ();
          gtk_button_set_label (GTK_BUTTON (button), "Details");
          gtk_box_pack_start (GTK_BOX (root), button, FALSE, FALSE, 0);
          popover = gtk_popover_new (button);
          gtk_popover_set_modal (GTK_POPOVER (popover), TRUE);
          gtk_container_set_border_width (GTK_CONTAINER (body), 20);
          gtk_box_pack_start (GTK_BOX (body), combo, FALSE, FALSE, 0);
          gtk_container_add (GTK_CONTAINER (popover), body);
          gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
          gtk_widget_show_all (body);
        }
      else
        gtk_box_pack_start (GTK_BOX (root), combo, FALSE, FALSE, 0);
      g_object_add_weak_pointer (G_OBJECT (combo), (gpointer *) &weak_combo);
      g_object_add_weak_pointer (G_OBJECT (host), (gpointer *) &weak_host);
      gimp_unit_combo_box_set_active (GIMP_UNIT_COMBO_BOX (combo), gimp_unit_pixel ());
      gtk_widget_show_all (host);
      gtk_window_present (GTK_WINDOW (host));
      rectangle_popup_settle ();
      if (popover)
        rectangle_popup_click_center (input, button);
      gtk_widget_get_preferred_width (combo, &compact_minimum, &compact_natural);
      rectangle_popup_click_center (input, combo);
      rectangle_popup_assert_menu (combo, popover, TRUE);
      rectangle_popup_key (input, combo, GDK_KEY_Escape);
      rectangle_popup_assert_menu (combo, popover, FALSE);
      gtk_widget_get_preferred_width (combo, &closed_minimum, &closed_natural);
      g_test_message ("Closed native width: before=%d/%d after=%d/%d",
                      compact_minimum, compact_natural, closed_minimum, closed_natural);
      g_assert_cmpint (closed_natural, ==, compact_natural);

      replacement = gimp_unit_store_new (0);
      g_object_set (replacement, "long-format", "Long native unit name: %n",
                    "short-format", "%a", NULL);
      gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (replacement));
      g_object_unref (replacement);
      gimp_unit_combo_box_set_active (GIMP_UNIT_COMBO_BOX (combo), gimp_unit_pixel ());
      gtk_widget_set_name (combo, "rectangle-popup-lifecycle");
      gtk_css_provider_load_from_data (style,
                                       "#rectangle-popup-lifecycle { font-size: 20px; }",
                                       -1, NULL);
      gtk_style_context_add_provider (gtk_widget_get_style_context (combo),
                                      GTK_STYLE_PROVIDER (style),
                                      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      rectangle_popup_settle ();
      gtk_widget_get_preferred_width (combo, &compact_minimum, &compact_natural);
      rectangle_popup_click_center (input, combo);
      rectangle_popup_assert_menu (combo, popover, TRUE);
      rectangle_popup_key (input, combo, GDK_KEY_Escape);
      rectangle_popup_assert_menu (combo, popover, FALSE);
      gtk_widget_get_preferred_width (combo, &closed_minimum, &closed_natural);
      g_assert_cmpint (closed_natural, ==, compact_natural);
      {
        AtkObject *accessible = gtk_combo_box_get_popup_accessible (GTK_COMBO_BOX (combo));
        GtkWidget *old_menu = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
        GtkWidget *new_menu;
        GtkWidget *child = gtk_bin_get_child (GTK_BIN (combo));

        g_object_ref (old_menu);
        /* Public custom-child replacement reconstructs GtkComboBox's menu.
         * Keep old_menu alive so pointer inequality cannot be allocator reuse. */
        gtk_container_remove (GTK_CONTAINER (combo), child);
        gtk_container_add (GTK_CONTAINER (combo), gtk_label_new ("px"));
        gtk_widget_show_all (combo);
        rectangle_popup_settle ();
        accessible = gtk_combo_box_get_popup_accessible (GTK_COMBO_BOX (combo));
        new_menu = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
        g_assert_true (GTK_IS_MENU (new_menu));
        g_assert_true (new_menu != old_menu);
        g_object_unref (old_menu);
        rectangle_popup_click_center (input, combo);
        rectangle_popup_assert_menu (combo, popover, TRUE);
        rectangle_popup_key (input, combo, GDK_KEY_Escape);
        rectangle_popup_assert_menu (combo, popover, FALSE);
      }
      rectangle_popup_click_center (input, combo);
      rectangle_popup_assert_menu (combo, popover, TRUE);
      /* Destroying only an open combo leaves an unrealized menu grab in
       * GtkComboBox itself (also reproduced with GtkComboBoxText). Close this
       * menu first; whole-popover/window destruction remains tested open. */
      if (scenario == 1 || scenario == 3)
        {
          rectangle_popup_key (input, combo, GDK_KEY_Escape);
          rectangle_popup_assert_menu (combo, popover, FALSE);
        }
      if (scenario == 2)
        gtk_widget_destroy (popover);
      else if (scenario == 3)
        gtk_widget_destroy (combo);
      gtk_widget_destroy (host);
      rectangle_popup_settle ();
      g_assert_null (weak_combo);
      g_assert_null (weak_host);
      rectangle_popup_assert_no_grabs ();
      g_object_unref (style);
    }
}
#endif

static void
rectangle_units_real_popup_events (void)
{
#ifdef GDK_WINDOWING_X11
  RectanglePopupInput input = { 0 };
  GtkSettings *settings = gtk_settings_get_default ();
  gboolean animations = FALSE;

  if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      g_test_skip ("Real unit-menu input needs the X11 test display");
      return;
    }
  input.module = g_module_open ("libXtst.so.6", G_MODULE_BIND_LAZY);
  if (!input.module)
    {
      g_test_skip ("XTest runtime is unavailable");
      return;
    }
  g_assert_true (g_module_symbol (input.module, "XTestFakeMotionEvent", (gpointer *) &input.motion));
  g_assert_true (g_module_symbol (input.module, "XTestFakeButtonEvent", (gpointer *) &input.button));
  g_assert_true (g_module_symbol (input.module, "XTestFakeKeyEvent", (gpointer *) &input.key));
  g_object_get (settings, "gtk-enable-animations", &animations, NULL);
  g_object_set (settings, "gtk-enable-animations", FALSE, NULL);
  for (guint instance = 0; instance < 2 * G_N_ELEMENTS (rectangle_tool_names); instance++)
    {
      guint tool = instance % G_N_ELEMENTS (rectangle_tool_names);
      RectangleFixture fixture;
      GimpRectangleOptions *options;
      GtkWidget *host, *box, *space, *view, *button, *popover;
      RectangleUnitControls controls;

      rectangle_fixture_init (&fixture, tool);
      options = GIMP_RECTANGLE_OPTIONS (fixture.options);
      gimp_tools_get_tool_options_gui (fixture.options);
      rectangle_unit_dimensions (fixture.image, 1200, 800);
      gimp_image_set_resolution (fixture.image, 300, 200);
      rectangle_start (&fixture);
      rectangle_unit_seed (options);
      view = rectangle_new_view (fixture.options);
      controls = rectangle_unit_controls (view, options, FALSE);
      host = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      g_object_ref_sink (host);
      gtk_window_set_default_size (GTK_WINDOW (host), 1200, 700);
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      space = gtk_drawing_area_new ();
      gtk_box_pack_start (GTK_BOX (box), space, TRUE, TRUE, 0);
      gtk_box_pack_end (GTK_BOX (box), view, FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (host), box);
      gtk_widget_show_all (host);
      gtk_window_present (GTK_WINDOW (host));
      rectangle_popup_settle ();
      button = find (view, "gimp-painter-selection-details", GTK_TYPE_MENU_BUTTON);
      popover = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
      for (guint group = 0; group < 4; group++)
        {
          guint unit_group = MIN (group, 2);
          GtkWidget *combo = group < 3 ? controls.menu[group] :
            gimp_size_entry_get_unit_combo (controls.entry[5]);
          RectangleUnitValues before;
          GimpUnit *old_unit = NULL, *new_unit = NULL;
          gint compact_minimum, compact_natural, closed_minimum, closed_natural;

          g_test_message ("Real native unit popup %s: %s%s", rectangle_tool_names[tool],
                           rectangle_unit_names[unit_group], group == 3 ? " height row" : "");
          g_object_set (options, "fixed-rule-active", TRUE, "fixed-rule",
                        group == 3 ? GIMP_RECTANGLE_FIXED_HEIGHT : GIMP_RECTANGLE_FIXED_WIDTH,
                        rectangle_unit_names[unit_group], gimp_unit_pixel (), NULL);
          rectangle_popup_click_center (&input, button);
          g_assert_true (gtk_widget_get_visible (popover));
          rectangle_popup_reveal (combo);
          gtk_widget_get_preferred_width (combo, &compact_minimum, &compact_natural);
          before = rectangle_unit_values (options);
          g_object_get (options, rectangle_unit_names[unit_group], &old_unit, NULL);
          /* These are real server pointer/key events, not set_active() or an
           * action-signal shortcut that bypasses GtkComboBox's mouse path. */
          rectangle_popup_click_center (&input, combo);
          rectangle_popup_assert_menu (combo, popover, TRUE);
          rectangle_popup_key (&input, combo, GDK_KEY_Down);
          rectangle_popup_key (&input, combo, GDK_KEY_Return);
          rectangle_popup_assert_menu (combo, popover, FALSE);
          g_object_get (options, rectangle_unit_names[unit_group], &new_unit, NULL);
          g_assert_true (new_unit != old_unit);
          rectangle_unit_values_equal (rectangle_unit_values (options), before);
          g_clear_object (&old_unit);
          g_clear_object (&new_unit);
          /* Escape dismisses only the menu and restores the modal parent. */
          rectangle_popup_click_center (&input, combo);
          rectangle_popup_assert_menu (combo, popover, TRUE);
          rectangle_popup_key (&input, combo, GDK_KEY_Escape);
          rectangle_popup_assert_menu (combo, popover, FALSE);
          rectangle_unit_values_equal (rectangle_unit_values (options), before);
          /* Outside input dismisses the menu; a second outside click dismisses
           * the parent. The next iteration reopens the same details widget. */
          rectangle_popup_click_center (&input, combo);
          rectangle_popup_assert_menu (combo, popover, TRUE);
          rectangle_popup_click (&input, space, 20, 20);
          {
            gboolean shown = TRUE;
            g_object_get (combo, "popup-shown", &shown, NULL);
            g_assert_false (shown);
          }
          rectangle_popup_click (&input, space, 20, 20);
          rectangle_popup_settle ();
          g_assert_false (gtk_widget_get_visible (popover));
          g_assert_false (gtk_widget_has_grab (popover));
          rectangle_unit_values_equal (rectangle_unit_values (options), before);
          g_object_set (options, rectangle_unit_names[unit_group], gimp_unit_pixel (), NULL);
          rectangle_popup_settle ();
          rectangle_popup_assert_format (combo, FALSE);
          gtk_widget_get_preferred_width (combo, &closed_minimum, &closed_natural);
          g_test_message ("Closed toolbar unit width: before=%d/%d after=%d/%d",
                          compact_minimum, compact_natural, closed_minimum, closed_natural);
          g_assert_cmpint (closed_natural, ==, compact_natural);
        }
      gtk_widget_destroy (host);
      g_object_unref (host);
      g_object_unref (view);
      rectangle_fixture_clear (&fixture);
    }
  rectangle_popup_native_lifetimes (&input);
  g_object_set (settings, "gtk-enable-animations", animations, NULL);
  g_module_close (input.module);
#else
  g_test_skip ("Real unit-menu input needs X11 support");
#endif
}

/* Show-time model reentry: a newly-created native unit makes the store's
 * show-time synchronization emit row-inserted. Mutate exactly at that seam. */
#include "core/gimpunit.h"
#ifdef GDK_WINDOWING_X11
typedef struct
{
  GtkWidget *combo;
  GtkWidget *popover;
  GtkWidget *menu;
  GtkTreeModel *replacement;
  guint scenario;
  guint fired;
} RectanglePopupReentry;

static void
rectangle_popup_row_inserted (GtkTreeModel *model, GtkTreePath *path,
                              GtkTreeIter *iter, gpointer data)
{
  RectanglePopupReentry *state = data;

  (void) model;
  (void) path;
  (void) iter;
  if (state->fired++)
    return;
  g_test_message ("Unit sync row-inserted reentry scenario %u", state->scenario);
  switch (state->scenario)
    {
    case 0:
      gtk_combo_box_set_model (GTK_COMBO_BOX (state->combo), state->replacement);
      break;
    case 1:
      gtk_combo_box_set_model (GTK_COMBO_BOX (state->combo), NULL);
      break;
    case 2:
      gtk_widget_destroy (state->combo);
      break;
    case 3:
      gtk_widget_destroy (state->menu);
      break;
    case 4:
      gtk_widget_destroy (state->popover);
      break;
    }
}
#endif

static void
rectangle_units_popup_model_reentry (void)
{
#ifdef GDK_WINDOWING_X11
  RectanglePopupInput input = { 0 };
  GtkSettings *settings = gtk_settings_get_default ();
  gboolean animations = FALSE;


  if (!GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      g_test_skip ("Real unit-menu input needs the X11 test display");
      return;
    }
  input.module = g_module_open ("libXtst.so.6", G_MODULE_BIND_LAZY);
  if (!input.module)
    {
      g_test_skip ("XTest runtime is unavailable");
      return;
    }
  g_assert_true (g_module_symbol (input.module, "XTestFakeMotionEvent", (gpointer *) &input.motion));
  g_assert_true (g_module_symbol (input.module, "XTestFakeButtonEvent", (gpointer *) &input.button));
  g_assert_true (g_module_symbol (input.module, "XTestFakeKeyEvent", (gpointer *) &input.key));
  g_object_get (settings, "gtk-enable-animations", &animations, NULL);
  g_object_set (settings, "gtk-enable-animations", FALSE, NULL);
  for (guint scenario = 0; scenario < 5; scenario++)
    {
      RectanglePopupReentry state = { 0 };
      GtkWidget *host = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      GtkWidget *root = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
      GtkWidget *body = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      GtkWidget *button = gtk_menu_button_new ();
      GtkTreeModel *model;
      AtkObject *accessible;
      gchar *unit_name = g_strdup_printf ("Popup reentry native unit %u", scenario);

      state.scenario = scenario;
      state.combo = gimp_unit_combo_box_new ();
      state.popover = gtk_popover_new (button);
      state.replacement = GTK_TREE_MODEL (gimp_unit_store_new (0));
      /* Prime the replacement before the new unit exists. The callback does
       * not ask it to synchronize recursively or set an active row. */
      _gimp_unit_store_sync_units (GIMP_UNIT_STORE (state.replacement));
      gtk_window_set_default_size (GTK_WINDOW (host), 500, 500);
      gtk_container_set_border_width (GTK_CONTAINER (root), 20);
      gtk_container_add (GTK_CONTAINER (host), root);
      gtk_button_set_label (GTK_BUTTON (button), "Details");
      gtk_box_pack_start (GTK_BOX (root), button, FALSE, FALSE, 0);
      gtk_popover_set_modal (GTK_POPOVER (state.popover), TRUE);
      gtk_container_set_border_width (GTK_CONTAINER (body), 20);
      gtk_box_pack_start (GTK_BOX (body), state.combo, FALSE, FALSE, 0);
      gtk_container_add (GTK_CONTAINER (state.popover), body);
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), state.popover);
      gtk_widget_show_all (body);
      gimp_unit_combo_box_set_active (GIMP_UNIT_COMBO_BOX (state.combo), gimp_unit_pixel ());
      gtk_widget_show_all (host);
      gtk_window_present (GTK_WINDOW (host));
      rectangle_popup_settle ();
      rectangle_popup_click_center (&input, button);
      accessible = gtk_combo_box_get_popup_accessible (GTK_COMBO_BOX (state.combo));
      g_assert_true (GTK_IS_ACCESSIBLE (accessible));
      state.menu = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
      g_assert_true (GTK_IS_MENU (state.menu));
      g_object_add_weak_pointer (G_OBJECT (state.combo), (gpointer *) &state.combo);
      g_object_add_weak_pointer (G_OBJECT (state.popover), (gpointer *) &state.popover);
      g_object_add_weak_pointer (G_OBJECT (state.menu), (gpointer *) &state.menu);
      model = gtk_combo_box_get_model (GTK_COMBO_BOX (state.combo));
      g_signal_connect (model, "row-inserted",
                         G_CALLBACK (rectangle_popup_row_inserted), &state);
      _gimp_unit_new (gimp, unit_name, 7.25, 4, "pr", "p.r.");
      g_free (unit_name);
      /* Keep only the combo's normal model ownership, to expose a borrowed
       * model reference dropped by the reentrant replace/clear callback. */
      if (scenario >= 2)
        {
          /* GTK's popup-positioning caller does not support synchronous
           * destruction during ::show (also reproduced with GtkComboBoxText).
           * Exercise this callback's ownership outside that GTK caller. */
          GtkWidget *menu = g_object_ref (state.menu);
          gtk_widget_show (menu);
          g_object_unref (menu);
          rectangle_popup_settle ();
        }
      else
        rectangle_popup_click_center (&input, state.combo);
      g_assert_cmpuint (state.fired, >, 0);
      if (scenario < 2)
        {
          g_assert_true (gtk_combo_box_get_model (GTK_COMBO_BOX (state.combo)) ==
                         (scenario == 0 ? state.replacement : NULL));
          gtk_combo_box_popdown (GTK_COMBO_BOX (state.combo));
          rectangle_popup_settle ();
          if (scenario == 1)
            gtk_combo_box_set_model (GTK_COMBO_BOX (state.combo), state.replacement);
          gimp_unit_combo_box_set_active (GIMP_UNIT_COMBO_BOX (state.combo), gimp_unit_pixel ());
          rectangle_popup_click_center (&input, state.combo);
          rectangle_popup_assert_menu (state.combo, state.popover, TRUE);
          rectangle_popup_key (&input, state.combo, GDK_KEY_Escape);
          rectangle_popup_assert_menu (state.combo, state.popover, FALSE);
        }
      gtk_widget_destroy (host);
      rectangle_popup_settle ();
      g_assert_null (state.combo);
      g_assert_null (state.popover);
      g_assert_null (state.menu);
      rectangle_popup_assert_no_grabs ();
      g_object_unref (state.replacement);
    }
  g_object_set (settings, "gtk-enable-animations", animations, NULL);
  g_module_close (input.module);
#else
  g_test_skip ("Real unit-menu input needs X11 support");
#endif
}
