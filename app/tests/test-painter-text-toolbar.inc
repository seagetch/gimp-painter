/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "core/gimpasync.h"
#include "core/gimpasyncset.h"
#include "core/gimpdatafactory.h"
#include "text/text-enums.h"
#include "text/gimpfont.h"
#include "tools/gimptextoptions.h"
#include "widgets/gimpcontainerentry.h"
#include "widgets/gimplanguageentry.h"

static GimpSizeEntry *
text_toolbar_size (GtkWidget *view)
{
  GtkWidget *entry = find (view, "gimp-painter-text-size", GIMP_TYPE_SIZE_ENTRY);
  g_assert_nonnull (entry);
  return GIMP_SIZE_ENTRY (entry);
}

static void
text_toolbar_assert_native (GimpToolOptions *options,
                            GtkWidget       *native,
                            GtkWidget       *size,
                            GPtrArray       *widgets,
                            GPtrArray       *parents)
{
  histogram_toolbar_assert_native (options, native, widgets, parents);
  g_assert_true (GIMP_TEXT_OPTIONS (options)->size_entry == size);
}

static void
text_toolbar_assert_color (GtkWidget *view,
                           GeglColor *expected)
{
  GeglColor *actual = gimp_color_button_get_color
    (GIMP_COLOR_BUTTON (find (view, "foreground", GIMP_TYPE_COLOR_BUTTON)));
  gdouble a[4], b[4];
  gegl_color_get_rgba (actual, a, a + 1, a + 2, a + 3);
  gegl_color_get_rgba (expected, b, b + 1, b + 2, b + 3);
  for (guint i = 0; i < 4; i++)
    g_assert_cmpfloat_with_epsilon (a[i], b[i], 1e-6);
  g_object_unref (actual);
}

static void
independent_text_toolbar (void)
{
  GimpToolOptions *options = info ("gimp-text-tool")->tool_options;
  GimpConfig *saved = gimp_config_duplicate (GIMP_CONFIG (options));
  GtkWidget *native = gimp_tools_get_tool_options_gui (options);
  GtkWidget *native_size = GIMP_TEXT_OPTIONS (options)->size_entry;
  GPtrArray *widgets = g_ptr_array_new (), *parents = g_ptr_array_new ();
  GPtrArray *trees[2] = { g_ptr_array_new (), g_ptr_array_new () };
  GtkWidget *views[2];
  gpointer weak[2];
  const gchar *booleans[] = { "use-editor", "antialias", "show-on-canvas" };
  const gchar *numbers[] = { "indent", "line-spacing", "letter-spacing" };
  const struct { const gchar *property; gint first, second; } enums[] = {
    { "hint-style", GIMP_TEXT_HINT_STYLE_NONE, GIMP_TEXT_HINT_STYLE_FULL },
    { "box-mode", GIMP_TEXT_BOX_DYNAMIC, GIMP_TEXT_BOX_FIXED },
    { "base-direction", GIMP_TEXT_DIRECTION_LTR, GIMP_TEXT_DIRECTION_TTB_RTL }
  };
  GeglColor *color = gegl_color_new ("rgb(0.2, 0.4, 0.8)");
  GimpFont *original_font = gimp_context_get_font (GIMP_CONTEXT (options));
  GimpContainer *fonts = gimp_data_factory_get_container (gimp->font_factory);
  GimpFont *fixture_fonts[2];
  gpointer weak_fonts[2];
  gint original_font_count = gimp_container_get_n_children (fonts);
  guint notifications = 0;

  if (original_font) g_object_ref (original_font);
  /* GUI testing intentionally disables font discovery. Build two normal,
   * test-owned GimpFont resources through the same public construction and
   * lookup-name APIs as the native factory. No disk/fontconfig/profile state
   * is changed, and this proves resource binding rather than glyph rendering. */
  for (guint i = 0; i < 2; i++)
    {
      const gchar *name = i ? "Painter test Serif" : "Painter test Sans";
      g_assert_null (gimp_container_get_child_by_name (fonts, name));
      fixture_fonts[i] = g_object_new (GIMP_TYPE_FONT, "name", name, NULL);
      gimp_font_set_lookup_name (fixture_fonts[i], g_strdup (i ? "Serif" : "Sans"));
      gimp_data_make_internal (GIMP_DATA (fixture_fonts[i]),
                               i ? "painter-test-serif" : "painter-test-sans");
      gimp_data_clean (GIMP_DATA (fixture_fonts[i]));
      weak_fonts[i] = fixture_fonts[i];
      g_object_add_weak_pointer (G_OBJECT (fixture_fonts[i]), &weak_fonts[i]);
      g_assert_true (gimp_container_add (fonts, GIMP_OBJECT (fixture_fonts[i])));
    }
  g_assert_cmpint (gimp_container_get_n_children (fonts), ==, original_font_count + 2);
  g_assert_nonnull (native_size);
  gather (native, widgets);
  for (guint i = 0; i < widgets->len; i++)
    g_ptr_array_add (parents, gtk_widget_get_parent (g_ptr_array_index (widgets, i)));
  for (guint i = 0; i < 2; i++)
    {
      views[i] = gimp_tools_create_painter_toolbar_gui (options);
      g_assert_nonnull (views[i]);
      g_assert_true (g_object_is_floating (views[i]));
      g_object_ref_sink (views[i]);
      g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (views[i])),
                       ==, GTK_ORIENTATION_HORIZONTAL);
      weak[i] = views[i];
      g_object_add_weak_pointer (G_OBJECT (views[i]), &weak[i]);
      gather (views[i], trees[i]);
      for (guint n = 0; n < trees[i]->len; n++)
        g_assert_false (g_ptr_array_find (widgets, g_ptr_array_index (trees[i], n), NULL));
      g_assert_true (GTK_WIDGET (text_toolbar_size (views[i])) != native_size);
    }
  for (guint n = 0; n < trees[0]->len; n++)
    g_assert_false (g_ptr_array_find (trees[1], g_ptr_array_index (trees[0], n), NULL));
  g_object_set (options, "font-size-unit", gimp_unit_pixel (), "font-size", 32.5, NULL);
  gimp_size_entry_set_value (text_toolbar_size (views[0]), 0, 41.25);
  g_assert_cmpfloat (GIMP_TEXT_OPTIONS (options)->font_size, ==, 41.25);
  g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[1]), 0), ==, 41.25);
  g_assert_cmpfloat (gimp_size_entry_get_value (GIMP_SIZE_ENTRY (native_size), 0), ==, 41.25);
  gimp_size_entry_set_value (GIMP_SIZE_ENTRY (native_size), 0, 23.5);
  for (guint i = 0; i < 2; i++)
    g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[i]), 0), ==, 23.5);
  {
    GtkWidget *font_boxes[2];
    GimpViewableButton *buttons[2];
    gint actual;
    for (guint i = 0; i < 2; i++)
      {
        font_boxes[i] = find (views[i], "gimp-painter-text-font", GTK_TYPE_BOX);
        g_assert_nonnull (font_boxes[i]);
        buttons[i] = g_object_get_data (G_OBJECT (font_boxes[i]), "viewable-button");
        g_assert_true (GIMP_IS_VIEWABLE_BUTTON (buttons[i]));
      }
    g_object_set (options, "font-view-type", GIMP_VIEW_TYPE_GRID,
                  "font-view-size", GIMP_VIEW_SIZE_LARGE, NULL);
    for (guint i = 0; i < 2; i++)
      {
        g_assert_cmpint (gimp_viewable_button_get_view_type (buttons[i]), ==, GIMP_VIEW_TYPE_GRID);
        g_assert_cmpint (gimp_viewable_button_get_view_size (buttons[i]), ==, GIMP_VIEW_SIZE_LARGE);
      }
    gimp_viewable_button_set_view_type (buttons[0], GIMP_VIEW_TYPE_LIST);
    gimp_viewable_button_set_view_size (buttons[0], GIMP_VIEW_SIZE_SMALL);
    g_object_get (options, "font-view-type", &actual, NULL);
    g_assert_cmpint (actual, ==, GIMP_VIEW_TYPE_LIST);
    g_object_get (options, "font-view-size", &actual, NULL);
    g_assert_cmpint (actual, ==, GIMP_VIEW_SIZE_SMALL);
    g_assert_cmpint (gimp_viewable_button_get_view_size (buttons[1]), ==, GIMP_VIEW_SIZE_SMALL);
  }
  for (guint i = 0; i < G_N_ELEMENTS (booleans); i++)
    for (guint value = 0; value < 2; value++)
      {
        gboolean actual;
        g_object_set (options, booleans[i], ! value, NULL);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
          (find (views[0], booleans[i], GTK_TYPE_TOGGLE_BUTTON)), value);
        g_object_get (options, booleans[i], &actual, NULL);
        g_assert_cmpint (actual, ==, value);
        g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
          (find (views[1], booleans[i], GTK_TYPE_TOGGLE_BUTTON))), ==, value);
        g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
          (find (native, booleans[i], GTK_TYPE_TOGGLE_BUTTON))), ==, value);
      }
  for (guint i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      gchar *signal = g_strdup_printf ("notify::%s", numbers[i]);
      GtkWidget *spin = find (views[0], numbers[i], GTK_TYPE_SPIN_BUTTON);
      gdouble actual;
      g_object_set (options, numbers[i], 0.0, NULL);
      notifications = 0;
      g_signal_connect (options, signal, G_CALLBACK (native_count_notify), &notifications);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), -3.5 - i);
      g_assert_cmpuint (notifications, ==, 1);
      g_object_get (options, numbers[i], &actual, NULL);
      g_assert_cmpfloat (actual, ==, -3.5 - i);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON
        (find (views[1], numbers[i], GTK_TYPE_SPIN_BUTTON))), ==, actual);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON
        (find (native, numbers[i], GTK_TYPE_SPIN_BUTTON))), ==, actual);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON
        (find (native, numbers[i], GTK_TYPE_SPIN_BUTTON)), 4.5 + i);
      g_assert_cmpuint (notifications, ==, 2);
      g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON (spin)), ==, 4.5 + i);
      g_signal_handlers_disconnect_by_data (options, &notifications);
      g_free (signal);
    }
  for (guint i = 0; i < G_N_ELEMENTS (enums); i++)
    {
      gint actual;
      g_object_set (options, enums[i].property, enums[i].first, NULL);
      gimp_int_combo_box_set_active (GIMP_INT_COMBO_BOX
        (find (views[0], enums[i].property, GIMP_TYPE_INT_COMBO_BOX)), enums[i].second);
      g_object_get (options, enums[i].property, &actual, NULL);
      g_assert_cmpint (actual, ==, enums[i].second);
      g_assert_true (gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX
        (find (views[1], enums[i].property, GIMP_TYPE_INT_COMBO_BOX)), &actual));
      g_assert_cmpint (actual, ==, enums[i].second);
      g_object_set (options, enums[i].property, enums[i].first, NULL);
      g_assert_true (gimp_int_combo_box_get_active (GIMP_INT_COMBO_BOX
        (find (views[0], enums[i].property, GIMP_TYPE_INT_COMBO_BOX)), &actual));
      g_assert_cmpint (actual, ==, enums[i].first);
    }
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (foreground_radio (views[0], "justify")),
                                   GIMP_TEXT_JUSTIFY_FILL);
  native_assert_radio_value (foreground_radio (views[1], "justify"), GIMP_TEXT_JUSTIFY_FILL, 4);
  native_assert_radio_value (foreground_radio (native, "justify"), GIMP_TEXT_JUSTIFY_FILL, 4);
  gimp_int_radio_group_set_active (GTK_RADIO_BUTTON (foreground_radio (native, "justify")),
                                   GIMP_TEXT_JUSTIFY_RIGHT);
  native_assert_radio_value (foreground_radio (views[0], "justify"), GIMP_TEXT_JUSTIFY_RIGHT, 4);
  gimp_color_button_set_color (GIMP_COLOR_BUTTON
    (find (views[0], "foreground", GIMP_TYPE_COLOR_BUTTON)), color);
  text_toolbar_assert_color (views[1], color);
  text_toolbar_assert_color (native, color);
  gegl_color_set_rgba (color, 0.8, 0.2, 0.1, 1.0);
  gimp_color_button_set_color (GIMP_COLOR_BUTTON
    (find (native, "foreground", GIMP_TYPE_COLOR_BUTTON)), color);
  text_toolbar_assert_color (views[0], color);
  text_toolbar_assert_color (views[1], color);
  for (guint i = 0; i < 2; i++)
    {
      GtkWidget *details_color = find (views[i], "gimp-painter-text-details-color",
                                        GIMP_TYPE_COLOR_BUTTON);
      g_assert_nonnull (details_color);
      g_assert_true (details_color != find (views[i], "foreground", GIMP_TYPE_COLOR_BUTTON));
      text_toolbar_assert_color (details_color, color);
    }
  gegl_color_set_rgba (color, 0.3, 0.7, 0.2, 1.0);
  gimp_color_button_set_color (GIMP_COLOR_BUTTON
    (find (views[1], "gimp-painter-text-details-color", GIMP_TYPE_COLOR_BUTTON)), color);
  text_toolbar_assert_color (views[0], color);
  text_toolbar_assert_color (views[1], color);
  text_toolbar_assert_color (native, color);
  text_toolbar_assert_color (find (views[0], "gimp-painter-text-details-color",
                                   GIMP_TYPE_COLOR_BUTTON), color);
#ifdef HAVE_ISO_CODES
  gimp_language_entry_set_code (GIMP_LANGUAGE_ENTRY
    (find (views[0], "language", GIMP_TYPE_LANGUAGE_ENTRY)), "en");
  g_assert_cmpstr (GIMP_TEXT_OPTIONS (options)->language, ==, "en");
  g_object_set (options, "language", "ja", NULL);
  for (guint i = 0; i < 2; i++)
    g_assert_cmpstr (gimp_language_entry_get_code (GIMP_LANGUAGE_ENTRY
      (find (views[i], "language", GIMP_TYPE_LANGUAGE_ENTRY))), ==, "ja");
#endif
  for (guint i = 0; i < 2; i++)
    {
      GimpFont *font = fixture_fonts[i];
      GtkWidget *entry = find (views[0], "GimpContainerEntry", GIMP_TYPE_CONTAINER_ENTRY);
      g_assert_nonnull (entry);
      gtk_entry_set_text (GTK_ENTRY (entry), gimp_object_get_name (font));
      g_assert_true (gimp_context_get_font (GIMP_CONTEXT (options)) == font);
      entry = find (views[1], "GimpContainerEntry", GIMP_TYPE_CONTAINER_ENTRY);
      g_assert_nonnull (entry);
      g_assert_true (GIMP_CONTAINER_ENTRY (entry)->viewable == GIMP_VIEWABLE (font));
    }
  gimp_context_set_font (GIMP_CONTEXT (options), fixture_fonts[0]);
  g_assert_true (GIMP_CONTAINER_ENTRY (find (views[0], "GimpContainerEntry",
                                            GIMP_TYPE_CONTAINER_ENTRY))->viewable ==
                 GIMP_VIEWABLE (fixture_fonts[0]));
  text_toolbar_assert_native (options, native, native_size, widgets, parents);
  gtk_widget_destroy (views[0]); g_object_unref (views[0]);
  g_assert_null (weak[0]);
  g_object_set (options, "indent", 17.5, NULL);
  g_assert_cmpfloat (gtk_spin_button_get_value (GTK_SPIN_BUTTON
    (find (views[1], "indent", GTK_TYPE_SPIN_BUTTON))), ==, 17.5);
  gtk_widget_destroy (views[1]); g_object_unref (views[1]);
  g_assert_null (weak[1]);
  text_toolbar_assert_native (options, native, native_size, widgets, parents);
  gimp_config_sync (G_OBJECT (saved), G_OBJECT (options), 0);
  gimp_context_set_font (GIMP_CONTEXT (options), original_font);
  for (guint i = 0; i < 2; i++)
    {
      g_assert_true (gimp_container_remove (fonts, GIMP_OBJECT (fixture_fonts[i])));
      g_object_unref (fixture_fonts[i]);
      g_assert_null (weak_fonts[i]);
    }
  g_assert_cmpint (gimp_container_get_n_children (fonts), ==, original_font_count);
  if (original_font) g_object_unref (original_font);
  g_object_unref (saved); g_object_unref (color);
  g_ptr_array_unref (widgets); g_ptr_array_unref (parents);
  g_ptr_array_unref (trees[0]); g_ptr_array_unref (trees[1]);
}

static void
text_toolbar_model_before_view (void)
{
  GimpToolOptions *options = g_object_new (GIMP_TYPE_TEXT_OPTIONS,
                                          "gimp", gimp, "tool-info",
                                          info ("gimp-text-tool"), NULL);
  GtkWidget *views[2];
  gpointer weak_options = options;
  GimpAsyncSet *fonts = gimp_data_factory_get_async_set (gimp->font_factory);
  GimpAsync *loading = gimp_async_new ();

  g_assert_null (GIMP_TEXT_OPTIONS (options)->size_entry);
  g_assert_null (gimp_tools_create_painter_toolbar_gui (options));
  /* Deliberately exercise the real shared font factory's loading state,
   * with a bounded fixture operation removed before the test returns. */
  gimp_async_set_add (fonts, loading);
  for (guint i = 0; i < 2; i++)
    {
      views[i] = gimp_painter_tool_toolbar_new (options);
      g_assert_nonnull (views[i]);
      g_object_ref_sink (views[i]);
      gtk_widget_show_all (views[i]);
      g_assert_false (gtk_widget_get_sensitive (views[i]));
      g_assert_true (gtk_widget_get_visible
        (find (views[i], "gimp-painter-text-fonts-busy", GTK_TYPE_LABEL)));
      g_assert_null (GIMP_TEXT_OPTIONS (options)->size_entry);
    }
  gimp_async_finish (loading, NULL);
  drain ();
  g_assert_true (gimp_async_set_is_empty (fonts));
  for (guint i = 0; i < 2; i++)
    {
      gtk_widget_show_all (views[i]);
      g_assert_true (gtk_widget_get_sensitive (views[i]));
      g_assert_false (gtk_widget_get_visible
        (find (views[i], "gimp-painter-text-fonts-busy", GTK_TYPE_LABEL)));
    }
  g_object_unref (loading);
  g_object_add_weak_pointer (G_OBJECT (options), &weak_options);
  g_object_unref (options);
  g_assert_nonnull (weak_options);
  gimp_size_entry_set_value (text_toolbar_size (views[0]), 0, 63.5);
  g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[1]), 0), ==, 63.5);
  gtk_widget_destroy (views[0]); g_object_unref (views[0]);
  g_assert_nonnull (weak_options);
  gimp_size_entry_set_value (text_toolbar_size (views[1]), 0, 27.25);
  gtk_widget_destroy (views[1]); g_object_unref (views[1]);
  g_assert_null (weak_options);
}

static void
text_toolbar_units_images (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpImage *previous = gimp_context_get_image (context);
  GimpImage *first = rectangle_unit_image (1200, 800, 300, 200);
  GimpImage *second = rectangle_unit_image (1800, 1000, 150, 400);
  GimpToolOptions *options = g_object_new (GIMP_TYPE_TEXT_OPTIONS,
                                          "gimp", gimp, "tool-info",
                                          info ("gimp-text-tool"), NULL);
  GtkWidget *views[2];
  guint notifications = 0;
  gpointer weak_image = first;

  if (previous) g_object_ref (previous);
  gimp_context_set_image (context, first);
  g_object_set (options, "font-size-unit", gimp_unit_point (), "font-size", 5000.0, NULL);
  for (guint i = 0; i < 2; i++)
    {
      views[i] = gimp_painter_tool_toolbar_new (options);
      g_object_ref_sink (views[i]);
      g_assert_cmpfloat_with_epsilon
        (gimp_size_entry_get_refval (text_toolbar_size (views[i]), 0), 5000.0 * 200.0 / 72.0, 1e-6);
    }
  /* Value bounds are 0..8192 in the chosen physical unit, not the pixel
   * interval inherited from the constructor's initial 72 DPI. */
  {
    const gdouble resolutions[] = { 300.0, 72.0, 600.0, 200.0 };
    for (guint n = 0; n < G_N_ELEMENTS (resolutions); n++)
      {
        gimp_image_set_resolution (first, 123.0, resolutions[n]);
        g_assert_cmpfloat (GIMP_TEXT_OPTIONS (options)->font_size, ==, 5000.0);
        for (guint i = 0; i < 2; i++)
          {
            GimpSizeEntry *entry = text_toolbar_size (views[i]);
            GtkWidget *spin = gimp_size_entry_get_help_widget (entry, 0);
            gdouble lower, upper;
            gtk_spin_button_get_range (GTK_SPIN_BUTTON (spin), &lower, &upper);
            g_assert_cmpfloat (lower, ==, 0.0);
            g_assert_cmpfloat (upper, ==, 8192.0);
            g_assert_cmpfloat (gimp_size_entry_get_value (entry, 0), ==, 5000.0);
            g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_refval (entry, 0),
                                            5000.0 * resolutions[n] / 72.0, 1e-6);
          }
      }
    gimp_size_entry_set_value (text_toolbar_size (views[0]), 0, 8192.0);
    g_assert_cmpfloat (GIMP_TEXT_OPTIONS (options)->font_size, ==, 8192.0);
    g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[1]), 0), ==, 8192.0);
    gimp_context_set_image (context, second);
    for (guint i = 0; i < 2; i++)
      g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_refval (text_toolbar_size (views[i]), 0),
                                      8192.0 * 400.0 / 72.0, 1e-6);
    gimp_context_set_image (context, first);
    gimp_image_set_resolution (first, 123.0, 300.0);
    g_object_set (options, "font-size", 5000.0, NULL);
    /* 5000pt converts above the model's 8192px maximum. Normalize before
     * publishing either property; a generic callback would warn/reject. */
    gimp_size_entry_set_unit (text_toolbar_size (views[0]), gimp_unit_pixel ());
    g_assert_true (GIMP_TEXT_OPTIONS (options)->unit == gimp_unit_pixel ());
    g_assert_cmpfloat (GIMP_TEXT_OPTIONS (options)->font_size, ==, 8192.0);
    for (guint i = 0; i < 2; i++)
      {
        g_assert_true (gimp_size_entry_get_unit (text_toolbar_size (views[i])) == gimp_unit_pixel ());
        g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[i]), 0), ==, 8192.0);
      }
    gimp_size_entry_set_value (text_toolbar_size (views[0]), 0, 24.0);
    gimp_size_entry_set_unit (text_toolbar_size (views[1]), gimp_unit_point ());
    g_assert_cmpfloat_with_epsilon (GIMP_TEXT_OPTIONS (options)->font_size,
                                    24.0 * 72.0 / 300.0, 1e-6);
    for (guint i = 0; i < 2; i++)
      {
        gdouble lower, upper;
        GtkWidget *spin = gimp_size_entry_get_help_widget (text_toolbar_size (views[i]), 0);
        gtk_spin_button_get_range (GTK_SPIN_BUTTON (spin), &lower, &upper);
        g_assert_cmpfloat (lower, ==, 0.0);
        g_assert_cmpfloat (upper, ==, 8192.0);
      }
    /* No resolution notification occurs between conversion and this edit. */
    gimp_size_entry_set_value (text_toolbar_size (views[1]), 0, 5000.0);
    g_assert_cmpfloat (GIMP_TEXT_OPTIONS (options)->font_size, ==, 5000.0);
    g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[0]), 0), ==, 5000.0);
    g_object_set (options, "font-size-unit", gimp_unit_pixel (), "font-size", 30.0, NULL);
    g_object_set (options, "font-size-unit", gimp_unit_point (), "font-size", 6000.0, NULL);
    for (guint i = 0; i < 2; i++)
      g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[i]), 0), ==, 6000.0);
    gimp_image_set_resolution (first, 123.0, 200.0);
    g_object_set (options, "font-size", 18.25, NULL);
  }
  g_signal_connect (options, "notify::font-size", G_CALLBACK (native_count_notify), &notifications);
  gimp_image_set_resolution (first, 600, 100);
  g_assert_cmpuint (notifications, ==, 0);
  for (guint i = 0; i < 2; i++)
    {
      g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[i]), 0), ==, 18.25);
      g_assert_cmpfloat_with_epsilon
        (gimp_size_entry_get_refval (text_toolbar_size (views[i]), 0), 18.25 * 100.0 / 72.0, 1e-6);
    }
  gimp_context_set_image (context, second);
  gimp_image_set_resolution (first, 90, 90);
  g_assert_cmpuint (notifications, ==, 0);
  for (guint i = 0; i < 2; i++)
    {
      GimpSizeEntry *entry = text_toolbar_size (views[i]);
      g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_refval (entry, 0),
                                      18.25 * 400.0 / 72.0, 1e-6);
    }
  gimp_size_entry_set_unit (text_toolbar_size (views[0]), gimp_unit_pixel ());
  g_assert_true (GIMP_TEXT_OPTIONS (options)->unit == gimp_unit_pixel ());
  g_assert_true (gimp_size_entry_get_unit (text_toolbar_size (views[1])) == gimp_unit_pixel ());
  g_assert_cmpfloat_with_epsilon (GIMP_TEXT_OPTIONS (options)->font_size, 18.25 * 400.0 / 72.0, 1e-6);
  g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_value (text_toolbar_size (views[1]), 0),
                                  GIMP_TEXT_OPTIONS (options)->font_size, 1e-6);
  gimp_size_entry_set_unit (text_toolbar_size (views[1]), gimp_unit_point ());
  g_assert_cmpfloat_with_epsilon (GIMP_TEXT_OPTIONS (options)->font_size, 18.25, 1e-6);
  gimp_size_entry_set_value (text_toolbar_size (views[1]), 0, 36.0);
  g_assert_cmpfloat (gimp_size_entry_get_value (text_toolbar_size (views[0]), 0), ==, 36.0);
  notifications = 0;
  gimp_context_set_image (context, NULL);
  for (guint i = 0; i < 2; i++)
    g_assert_cmpfloat (gimp_size_entry_get_refval (text_toolbar_size (views[i]), 0), ==, 36.0);
  g_assert_cmpuint (notifications, ==, 0);
  g_object_add_weak_pointer (G_OBJECT (first), &weak_image);
  g_object_unref (first);
  g_assert_null (weak_image); /* No image retained by either independent view. */
  gtk_widget_destroy (views[0]); g_object_unref (views[0]);
  gimp_context_set_image (context, second);
  gimp_image_set_resolution (second, 200, 250);
  g_assert_cmpfloat (gimp_size_entry_get_refval (text_toolbar_size (views[1]), 0), ==, 125.0);
  {
    GimpSizeEntry *retained = g_object_ref (text_toolbar_size (views[1]));
    gdouble before = gimp_size_entry_get_refval (retained, 0);
    gdouble size = GIMP_TEXT_OPTIONS (options)->font_size;
    gpointer weak_entry = retained;
    g_object_add_weak_pointer (G_OBJECT (retained), &weak_entry);
    gtk_widget_destroy (views[1]); g_object_unref (views[1]);
    notifications = 0;
    gimp_image_set_resolution (second, 72, 72);
    gimp_context_set_image (context, NULL);
    gimp_context_set_image (context, second);
    gimp_image_set_resolution (second, 144, 288);
    g_assert_cmpfloat (gimp_size_entry_get_refval (retained, 0), ==, before);
    g_assert_cmpfloat (GIMP_TEXT_OPTIONS (options)->font_size, ==, size);
    g_assert_cmpuint (notifications, ==, 0);
    g_object_unref (retained);
    g_assert_null (weak_entry);
  }
  gimp_context_set_image (context, previous);
  g_signal_handlers_disconnect_by_data (options, &notifications);
  g_object_unref (options); g_object_unref (second);
  if (previous) g_object_unref (previous);
}

static void
text_toolbar_assert_rgba (GtkWidget *view, gdouble r, gdouble g, gdouble b, gdouble a)
{
  GeglColor *expected = gegl_color_new ("black");
  gegl_color_set_rgba (expected, r, g, b, a);
  text_toolbar_assert_color (view, expected);
  g_object_unref (expected);
}

static void
text_toolbar_canvas_popup_lifetime (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *previous = gimp_context_get_tool (context);
  GimpToolOptions *options = info ("gimp-text-tool")->tool_options;
  GtkWidget *native = gimp_tools_get_tool_options_gui (options);
  GtkWidget *native_parent;
  GtkWidget *native_editor;
  GtkWidget *native_dockable;
  GtkWidget *existing_dockable;
  GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
  GtkWidget *native_size = GIMP_TEXT_OPTIONS (options)->size_entry;
  GPtrArray *native_widgets = g_ptr_array_new ();
  GPtrArray *native_parents = g_ptr_array_new ();
  GimpDisplayShell *shell;
  gboolean old_visible = gimp->config->painter_canvas_ui;
  GeglColor *original_color;
  GeglColor *test_color = gegl_color_new ("black");
  gboolean animations;
  GtkSettings *settings = gtk_settings_get_default ();

  gather (native, native_widgets);
  for (guint i = 0; i < native_widgets->len; i++)
    g_ptr_array_add (native_parents, gtk_widget_get_parent (g_ptr_array_index (native_widgets, i)));
  g_object_get (options, "foreground", &original_color, NULL);
  gegl_color_set_rgba (test_color, 0.1, 0.2, 0.3, 1.0);
  g_object_set (options, "foreground", test_color, NULL);
  g_object_get (settings, "gtk-enable-animations", &animations, NULL);
  g_object_set (settings, "gtk-enable-animations", FALSE, NULL);
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  shell = new_shell ();
  g_test_message ("Text fixture tool after new shell: %s",
                  gimp_object_get_name (gimp_context_get_tool (context)));
  g_object_set (gimp->config, "painter-canvas-ui", TRUE, NULL);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  drain ();
  /* A selected tool does not imply that a Tool Options editor exists.
   * Open the actual registered native Dockable, with its ordinary editor
   * and dockbook, before deriving ownership from its populated Move GUI. */
  existing_dockable = gimp_dialog_factory_find_widget (factory, "gimp-tool-options");
  native_dockable = gimp_window_strategy_show_dockable_dialog
    (GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp, factory,
     gimp_widget_get_monitor (GTK_WIDGET (shell)), "gimp-tool-options");
  g_assert_true (GIMP_IS_DOCKABLE (native_dockable));
  g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (native_dockable)));
  native_editor = gtk_bin_get_child (GTK_BIN (native_dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (native_editor));
  drain ();
  g_test_message ("Text fixture tool after native Dockable initialization: %s",
                  gimp_object_get_name (gimp_context_get_tool (context)));
  /* Shell/device initialization can restore the profile's tool after the
   * earlier selection. Establish Move only after that real setup is done. */
  gimp_context_set_tool (context, info ("gimp-move-tool"));
  drain ();
  g_assert_true (gimp_context_get_tool (context) == info ("gimp-move-tool"));
  GtkWidget *move_native = gimp_tools_get_tool_options_gui
    (info ("gimp-move-tool")->tool_options);
  native_parent = gtk_widget_get_parent (move_native);
  g_assert_nonnull (native_parent);
  g_assert_true (gtk_widget_get_ancestor (move_native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == native_editor);
  g_assert_true (gtk_widget_is_ancestor (move_native, native_dockable));
  g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (native_editor)) ==
                 info ("gimp-move-tool")->tool_options);
  if (gtk_widget_get_parent (native))
    g_assert_true (gtk_widget_get_parent (native) == native_parent);
  g_assert_true (g_ptr_array_index (native_widgets, 0) == native);
  g_ptr_array_index (native_parents, 0) = native_parent;
  for (guint cycle = 0; cycle < 3; cycle++)
    {
      GtkWidget *toolbar, *button, *popup;
      gpointer weak_toolbar, weak_popup, weak_color_dialog;
      gint64 deadline;

      gegl_color_set_rgba (test_color, 0.1, 0.2, 0.3, 1.0);
      g_object_set (options, "foreground", test_color, NULL);
      gimp_context_set_tool (context, info ("gimp-text-tool"));
      drain ();
      toolbar = find (shell->canvas, "gimp-painter-tool-toolbar", GTK_TYPE_BOX);
      g_assert_nonnull (toolbar);
      text_toolbar_assert_native (options, native, native_size,
                                native_widgets, native_parents);
      g_assert_true (gtk_widget_get_ancestor (native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == native_editor);
      g_assert_true (gtk_widget_is_ancestor (native, native_dockable));
      g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (native_editor)) == options);
      g_assert_false (gtk_widget_is_ancestor (native, toolbar));
      g_assert_true (gtk_widget_get_visible (native));
      button = find (toolbar, "gimp-painter-text-details", GTK_TYPE_MENU_BUTTON);
      g_assert_nonnull (button);
      /* This utility verifies real horizontal scrolling and anchor bounds. */
      histogram_toolbar_reveal (button);
      popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
      g_assert_nonnull (popup);
      g_assert_cmpint (gtk_popover_get_position (GTK_POPOVER (popup)), ==, GTK_POS_TOP);
      weak_toolbar = toolbar; weak_popup = popup;
      g_object_add_weak_pointer (G_OBJECT (toolbar), &weak_toolbar);
      g_object_add_weak_pointer (G_OBJECT (popup), &weak_popup);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
      while (! gtk_widget_get_mapped (popup) && g_get_monotonic_time () < deadline)
        {
          g_main_context_iteration (NULL, FALSE);
          g_usleep (1000);
        }
      g_assert_true (gtk_widget_get_mapped (popup));
      g_assert_true (gtk_widget_has_grab (popup));
      compact_popup_event (popup, GDK_KEY_PRESS, 0, 0);
      drain ();
      g_assert_false (gtk_widget_get_visible (popup));
      g_assert_false (gtk_widget_has_grab (popup));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
      drain ();
      g_assert_true (gtk_widget_get_mapped (popup));
      weak_color_dialog = NULL;
      if (cycle != 1)
        {
          GtkWidget *color = find (toolbar, cycle == 0 ? "foreground" :
                                    "gimp-painter-text-details-color", GIMP_TYPE_COLOR_PANEL);
          g_assert_nonnull (color);
          gtk_button_clicked (GTK_BUTTON (color));
          drain ();
          weak_color_dialog = GIMP_COLOR_PANEL (color)->color_dialog;
          g_assert_nonnull (weak_color_dialog);
          g_assert_true (gtk_widget_get_visible (weak_color_dialog));
          g_object_add_weak_pointer (G_OBJECT (weak_color_dialog), &weak_color_dialog);
          {
              GtkWidget *dialog = GIMP_COLOR_PANEL (color)->color_dialog;
              GtkWidget *cancel = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
              GtkWidget *accept = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
              g_assert_nonnull (cancel); g_assert_nonnull (accept);
              gegl_color_set_rgba (test_color, 0.9, 0.6, 0.2, 1.0);
              gimp_color_selection_set_color (GIMP_COLOR_SELECTION (GIMP_COLOR_DIALOG (dialog)->selection), test_color);
              gtk_button_clicked (GTK_BUTTON (cancel));
              drain ();
              g_assert_false (gtk_widget_get_visible (dialog));
              text_toolbar_assert_rgba (toolbar, 0.1, 0.2, 0.3, 1.0);
              text_toolbar_assert_rgba (native, 0.1, 0.2, 0.3, 1.0);
              text_toolbar_assert_rgba (find (toolbar, "gimp-painter-text-details-color",
                                               GIMP_TYPE_COLOR_PANEL), 0.1, 0.2, 0.3, 1.0);
              gtk_button_clicked (GTK_BUTTON (color));
              drain ();
              g_assert_true (gtk_widget_get_visible (dialog));
              gimp_color_selection_set_color (GIMP_COLOR_SELECTION (GIMP_COLOR_DIALOG (dialog)->selection), test_color);
              gtk_button_clicked (GTK_BUTTON (accept));
              drain ();
              g_assert_false (gtk_widget_get_visible (dialog));
              text_toolbar_assert_rgba (toolbar, 0.9, 0.6, 0.2, 1.0);
              text_toolbar_assert_rgba (native, 0.9, 0.6, 0.2, 1.0);
              text_toolbar_assert_rgba (find (toolbar, "gimp-painter-text-details-color",
                                               GIMP_TYPE_COLOR_PANEL), 0.9, 0.6, 0.2, 1.0);
              gtk_button_clicked (GTK_BUTTON (color));
              drain ();
              g_assert_true (gtk_widget_get_visible (dialog));
            }
        }
      else
        {
          /* Independently cover tool switching with the popup's actual grab,
           * before a color dialog can change GTK focus/grab state. */
          g_assert_true (gtk_widget_get_mapped (popup));
          g_assert_true (gtk_widget_has_grab (popup));
        }
      gimp_context_set_tool (context, info ("gimp-move-tool"));
      drain ();
      g_assert_null (weak_toolbar);
      g_assert_null (weak_popup);
      g_assert_null (weak_color_dialog);
      g_assert_null (gtk_grab_get_current ());
      text_toolbar_assert_native (options, native, native_size,
                                native_widgets, native_parents);
      g_assert_true (gtk_widget_get_ancestor (native, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == native_editor);
      g_assert_true (gtk_widget_is_ancestor (native, native_dockable));
      g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (native_editor)) ==
                     info ("gimp-move-tool")->tool_options);
      g_assert_false (gtk_widget_get_visible (native));
    }
  g_ptr_array_unref (native_widgets);
  g_ptr_array_unref (native_parents);
  if (! existing_dockable)
    gtk_widget_destroy (native_dockable);
  close_shell (shell);
  g_object_set (options, "foreground", original_color, NULL);
  g_object_unref (original_color);
  g_object_unref (test_color);
  g_object_set (gimp->config, "painter-canvas-ui", old_visible, NULL);
  gimp_context_set_tool (context, previous);
  g_object_set (settings, "gtk-enable-animations", animations, NULL);
  drain ();
}

/* Exercise the real canonical model and cached native size entry together.
 * Native resolution initialization mirrors gimp_text_tool_set_image; no text
 * layer rendering or simulated canvas text-editing claim is made here. */
static void
text_toolbar_native_units (void)
{
  GimpContext *context = gimp_get_user_context (gimp);
  GimpImage *previous = gimp_context_get_image (context);
  GimpImage *image = rectangle_unit_image (1000, 800, 150, 300);
  GimpToolOptions *options = info ("gimp-text-tool")->tool_options;
  GimpConfig *saved = gimp_config_duplicate (GIMP_CONFIG (options));
  GtkWidget *native = gimp_tools_get_tool_options_gui (options);
  GtkWidget *native_size = GIMP_TEXT_OPTIONS (options)->size_entry;
  GtkWidget *native_parent = gtk_widget_get_parent (native);
  GtkWidget *views[2];
  GimpSizeEntry *entries[3];
  guint notifications = 0;

  if (previous) g_object_ref (previous);
  gimp_context_set_image (context, image);
  g_object_set (options, "font-size-unit", gimp_unit_point (), "font-size", 24.0, NULL);
  gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (native_size), 0, 300.0, FALSE);
  entries[0] = GIMP_SIZE_ENTRY (native_size);
  for (guint i = 0; i < 2; i++)
    {
      views[i] = gimp_tools_create_painter_toolbar_gui (options);
      g_assert_nonnull (views[i]);
      g_object_ref_sink (views[i]);
      entries[i + 1] = text_toolbar_size (views[i]);
    }
  for (guint actor = 0; actor < 3; actor++)
    {
      gdouble points = 18.0 + actor * 6.0;
      gimp_size_entry_set_value (entries[actor], 0, points);
      for (guint i = 0; i < 3; i++)
        {
          g_assert_cmpfloat (gimp_size_entry_get_value (entries[i], 0), ==, points);
          g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_refval (entries[i], 0),
                                          points * 300.0 / 72.0, 1e-6);
        }
      gimp_size_entry_set_unit (entries[actor], gimp_unit_pixel ());
      for (guint i = 0; i < 3; i++)
        {
          g_assert_true (gimp_size_entry_get_unit (entries[i]) == gimp_unit_pixel ());
          g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_value (entries[i], 0),
                                          points * 300.0 / 72.0, 1e-6);
        }
      g_assert_cmpfloat_with_epsilon (GIMP_TEXT_OPTIONS (options)->font_size,
                                      points * 300.0 / 72.0, 1e-6);
      gimp_size_entry_set_unit (entries[actor], gimp_unit_point ());
      for (guint i = 0; i < 3; i++)
        g_assert_cmpfloat_with_epsilon (gimp_size_entry_get_value (entries[i], 0), points, 1e-6);
      g_assert_cmpfloat_with_epsilon (GIMP_TEXT_OPTIONS (options)->font_size, points, 1e-6);
    }
  g_assert_true (GIMP_TEXT_OPTIONS (options)->size_entry == native_size);
  g_assert_true (gimp_tools_get_tool_options_gui (options) == native);
  g_assert_true (gtk_widget_get_parent (native) == native_parent);
  g_signal_connect (options, "notify::font-size", G_CALLBACK (native_count_notify), &notifications);
  gtk_widget_destroy (views[0]); g_object_unref (views[0]);
  gtk_widget_destroy (views[1]); g_object_unref (views[1]);
  g_assert_cmpuint (notifications, ==, 0);
  g_signal_handlers_disconnect_by_data (options, &notifications);
  gimp_context_set_image (context, previous);
  /* Restore native metrics without claiming its private earlier resolution:
   * use the current context's Y axis, matching native text-tool convention. */
  {
    gdouble xres = 72.0, yres = 72.0;
    if (previous) gimp_image_get_resolution (previous, &xres, &yres);
    gimp_size_entry_set_resolution (GIMP_SIZE_ENTRY (native_size), 0, yres, FALSE);
  }
  gimp_config_sync (G_OBJECT (saved), G_OBJECT (options), 0);
  g_object_unref (saved); g_object_unref (image);
  if (previous) g_object_unref (previous);
}
