/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Included by gimppaintertooltoolbar.c. Independent counterpart of the
 * pinned legacy gimp_text_options_gui_full(TRUE)/create_view factories. */

typedef struct
{
  GimpContext *context;              /* owned by Gimp, weak signal target */
  GWeakRef     image;
} PainterTextMetrics;

/* The generic size-entry binding assumes pixel bounds survive unit changes.
 * Text instead bounds the numeric value in whichever unit is selected. Keep
 * this adjustment local to independent text views, not the shared widget. */
static void
painter_text_size_notify (GObject       *options,
                          GParamSpec    *unused,
                          GimpSizeEntry *entry)
{
  GParamSpecDouble *spec = G_PARAM_SPEC_DOUBLE
    (g_object_class_find_property (G_OBJECT_GET_CLASS (options), "font-size"));
  GimpUnit *unit;
  gdouble size;

  g_object_get (options, "font-size-unit", &unit, "font-size", &size, NULL);
  g_signal_handlers_block_matched (entry, G_SIGNAL_MATCH_DATA,
                                    0, 0, NULL, NULL, options);
  if (gimp_size_entry_get_unit (entry) != unit)
    gimp_size_entry_set_unit (entry, unit);
  gimp_size_entry_set_value_boundaries (entry, 0, spec->minimum, spec->maximum);
  gimp_size_entry_set_value (entry, 0, size);
  g_signal_handlers_unblock_matched (entry, G_SIGNAL_MATCH_DATA,
                                      0, 0, NULL, NULL, options);
  g_object_unref (unit);
}

static void
painter_text_size_changed (GimpSizeEntry *entry,
                           GObject       *options)
{
  GimpUnit *unit;
  gdouble size;
  gdouble value = gimp_size_entry_get_value (entry, 0);

  g_object_get (options, "font-size-unit", &unit, "font-size", &size, NULL);
  /* GimpSizeEntry changes its numeric adjustment before emitting
   * unit-changed. Do not publish that intermediate size with the old unit. */
  if (gimp_size_entry_get_unit (entry) == unit && value != size)
    g_object_set (options, "font-size", value, NULL);
  g_object_unref (unit);
}

static void
painter_text_unit_changed (GimpSizeEntry *entry,
                           GObject       *options)
{
  GParamSpecDouble *spec = G_PARAM_SPEC_DOUBLE
    (g_object_class_find_property (G_OBJECT_GET_CLASS (options), "font-size"));
  GimpUnit *unit = gimp_size_entry_get_unit (entry);
  GimpUnit *previous;
  gdouble size;
  gdouble value = CLAMP (gimp_size_entry_get_value (entry, 0),
                         spec->minimum, spec->maximum);

  g_object_get (options, "font-size-unit", &previous, "font-size", &size, NULL);
  g_signal_handlers_block_matched (entry, G_SIGNAL_MATCH_DATA,
                                    0, 0, NULL, NULL, options);
  gimp_size_entry_set_value_boundaries (entry, 0, spec->minimum, spec->maximum);
  gimp_size_entry_set_value (entry, 0, value);
  /* GObject publishes the paired notifications only after both canonical
   * fields are set, so independent and native observers read one state. */
  if (unit != previous || value != size)
    g_object_set (options, "font-size", value, "font-size-unit", unit, NULL);
  g_signal_handlers_unblock_matched (entry, G_SIGNAL_MATCH_DATA,
                                      0, 0, NULL, NULL, options);
  g_object_unref (previous);
}

static void
painter_text_resolution (GimpImage     *image,
                         GimpSizeEntry *entry)
{
  GObject *options = g_object_get_data (G_OBJECT (entry), "painter-text-options");
  GParamSpecDouble *spec = G_PARAM_SPEC_DOUBLE
    (g_object_class_find_property (G_OBJECT_GET_CLASS (options), "font-size"));
  gdouble size;
  gdouble xres = 72.0, yres = 72.0;

  if (image)
    gimp_image_get_resolution (image, &xres, &yres);
  /* font-size is expressed in font-size-unit, not pixels. Preserve that
   * value when resolution changes, matching the native text-tool entry. */
  g_object_get (options, "font-size", &size, NULL);
  g_signal_handlers_block_matched (entry, G_SIGNAL_MATCH_DATA,
                                    0, 0, NULL, NULL, options);
  gimp_size_entry_set_resolution (entry, 0, yres, FALSE);
  /* Resolution changes preserve pixel bounds in GimpSizeEntry. The model
   * instead bounds values in the selected unit; restore those limits before
   * setting a large physical size, otherwise it silently clamps the view. */
  gimp_size_entry_set_value_boundaries (entry, 0, spec->minimum, spec->maximum);
  gimp_size_entry_set_value (entry, 0, size);
  g_signal_handlers_unblock_matched (entry, G_SIGNAL_MATCH_DATA,
                                      0, 0, NULL, NULL, options);
}

static void
painter_text_image_changed (GimpContext   *context,
                            GimpImage     *image,
                            GimpSizeEntry *entry)
{
  PainterTextMetrics *metrics = g_object_get_data (G_OBJECT (entry),
                                                   "painter-text-metrics");
  GimpImage *previous = g_weak_ref_get (&metrics->image);

  if (previous)
    {
      g_signal_handlers_disconnect_by_func (previous,
                                            painter_text_resolution, entry);
      g_object_unref (previous);
    }
  g_weak_ref_set (&metrics->image, image);
  if (image)
    g_signal_connect_object (image, "resolution-changed",
                              G_CALLBACK (painter_text_resolution), entry, 0);
  painter_text_resolution (image, entry);
}

static void
painter_text_metrics_destroy (GtkWidget          *entry,
                              PainterTextMetrics *metrics)
{
  GimpImage *image = g_weak_ref_get (&metrics->image);
  GObject *options = g_object_get_data (G_OBJECT (entry), "painter-text-options");

  g_signal_handlers_disconnect_by_data (options, entry);
  g_signal_handlers_disconnect_by_data (entry, options);
  if (image)
    {
      g_signal_handlers_disconnect_by_func (image,
                                            painter_text_resolution, entry);
      g_object_unref (image);
    }
  g_weak_ref_set (&metrics->image, NULL);
  if (metrics->context)
    {
      g_signal_handlers_disconnect_by_func (metrics->context,
                                            painter_text_image_changed, entry);
      g_object_remove_weak_pointer (G_OBJECT (metrics->context),
                                    (gpointer *) &metrics->context);
      metrics->context = NULL;
    }
}

static void
painter_text_metrics_free (gpointer data)
{
  PainterTextMetrics *metrics = data;

  g_weak_ref_clear (&metrics->image);
  g_free (metrics);
}

static void
painter_text (GtkWidget       *box,
              GimpToolOptions *options)
{
  GObject            *config = G_OBJECT (options);
  GimpContext        *context = gimp_get_user_context (GIMP_CONTEXT (options)->gimp);
  GimpAsyncSet       *fonts = gimp_data_factory_get_async_set
                               (GIMP_CONTEXT (options)->gimp->font_factory);
  GtkWidget          *entry;
  GtkWidget          *row;
  GtkWidget          *button;
  GtkWidget          *details;
  PainterTextMetrics *metrics;

  g_object_bind_property (fonts, "empty", box, "sensitive", G_BINDING_SYNC_CREATE);
  button = gtk_label_new (_("Loading fonts…"));
  painter_pack (box, button);
  gtk_widget_set_name (button, "gimp-painter-text-fonts-busy");
  gtk_widget_set_no_show_all (button, TRUE);
  g_object_bind_property (fonts, "empty", button, "visible",
                          G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);
  row = gimp_prop_font_box_new (NULL, GIMP_CONTEXT (options), _("Font"), 2,
                                "font-view-type", "font-view-size");
  gtk_widget_set_name (row, "gimp-painter-text-font");
  painter_pack (box, row);
  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  painter_pack (row, gtk_label_new (_("Size:")));
  entry = gimp_prop_size_entry_new (config, "font-size", FALSE,
                                    "font-size-unit", "%n",
                                    GIMP_SIZE_ENTRY_UPDATE_SIZE, 72.0);
  gtk_widget_set_name (entry, "gimp-painter-text-size");
  /* Never assign options->size_entry: it belongs to the native dock. */
  g_object_set_data (G_OBJECT (entry), "painter-text-options", options);
  /* Replace only this newly constructed entry's generic size/unit binding.
   * Keep its bound-property metadata, help widgets and normal GTK controls. */
  g_signal_handlers_disconnect_by_data (entry, config);
  g_signal_handlers_disconnect_by_data (config, entry);
  g_signal_connect (entry, "value-changed",
                     G_CALLBACK (painter_text_size_changed), config);
  g_signal_connect (entry, "refval-changed",
                     G_CALLBACK (painter_text_size_changed), config);
  g_signal_connect (entry, "unit-changed",
                     G_CALLBACK (painter_text_unit_changed), config);
  g_signal_connect_object (config, "notify::font-size",
                            G_CALLBACK (painter_text_size_notify), entry, 0);
  g_signal_connect_object (config, "notify::font-size-unit",
                            G_CALLBACK (painter_text_size_notify), entry, 0);
  metrics = g_new0 (PainterTextMetrics, 1);
  metrics->context = context;
  g_object_add_weak_pointer (G_OBJECT (context), (gpointer *) &metrics->context);
  g_weak_ref_init (&metrics->image, NULL);
  g_object_set_data_full (G_OBJECT (entry), "painter-text-metrics", metrics,
                          painter_text_metrics_free);
  g_signal_connect (entry, "destroy",
                     G_CALLBACK (painter_text_metrics_destroy), metrics);
  g_signal_connect_object (context, "image-changed",
                            G_CALLBACK (painter_text_image_changed), entry, 0);
  painter_text_image_changed (context, gimp_context_get_image (context),
                              GIMP_SIZE_ENTRY (entry));
  painter_pack (row, entry);
  painter_pack (box, row);
  painter_check (box, config, "use-editor", _("Use editor"));
  painter_check (box, config, "antialias", _("Antialiasing"));
  button = gimp_prop_color_button_new (config, "foreground", _("Text Color"),
                                       40, 24, GIMP_COLOR_AREA_FLAT);
  gimp_color_button_set_update (GIMP_COLOR_BUTTON (button), TRUE);
  gimp_color_panel_set_context (GIMP_COLOR_PANEL (button), GIMP_CONTEXT (options));
  painter_pack (box, button);
  details = painter_details (box, _("Details…"), "gimp-painter-text-details");
  painter_combo (details, config, "hint-style", _("Hinting"), -1, -1);
  button = gimp_prop_color_button_new (config, "foreground", _("Text Color"),
                                       40, 24, GIMP_COLOR_AREA_FLAT);
  gtk_widget_set_name (button, "gimp-painter-text-details-color");
  gimp_color_button_set_update (GIMP_COLOR_BUTTON (button), TRUE);
  gimp_color_panel_set_context (GIMP_COLOR_PANEL (button), GIMP_CONTEXT (options));
  painter_pack (details, button);
  painter_pack (details, gimp_prop_enum_icon_box_new (config, "justify",
                                                     "format-justify", 0, 0));
  painter_number (details, config, "indent", _("Indentation"), 1.0, 1);
  painter_number (details, config, "line-spacing", _("Line spacing"), 1.0, 1);
  painter_number (details, config, "letter-spacing", _("Letter spacing"), 1.0, 1);
  painter_combo (details, config, "box-mode", _("Box"), 0, 0);
#ifdef HAVE_ISO_CODES
  painter_pack (details, gtk_label_new (_("Language:")));
  painter_pack (details, gimp_prop_language_entry_new (config, "language"));
#endif
  /* Current additions, intentionally distinguished from legacy controls. */
  painter_check (details, config, "show-on-canvas", _("Show on-canvas editor"));
  painter_combo (details, config, "base-direction", _("Direction"), 0, 0);
  /* Modern outline/stroke controls remain in the full native dock. */
}
