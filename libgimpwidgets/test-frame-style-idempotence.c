/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include "gimpwidgetstypes.h"
#include "gimpframe.h"

typedef struct
{
  GtkWidget *frame;
  GtkWidget *label;
  GtkWidget *child;
} Fixture;

static Fixture
fixture_new (void)
{
  Fixture f;
  f.frame = gimp_frame_new ("Frame label");
  g_object_ref_sink (f.frame);
  f.label = gtk_frame_get_label_widget (GTK_FRAME (f.frame));
  f.child = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (f.frame), f.child);
  gtk_widget_show_all (f.frame);
  g_signal_emit_by_name (f.frame, "style-updated");
  return f;
}

static void
fixture_clear (Fixture *f)
{
  gtk_widget_destroy (f->frame);
  g_object_unref (f->frame);
}

static void
attributes_notified (GObject *object, GParamSpec *pspec, gpointer data)
{
  guint *count = data;
  (void) object;
  (void) pspec;
  ++*count;
}

static PangoAttrList *
bold_list (void)
{
  PangoAttrList *attrs = pango_attr_list_new ();
  PangoAttribute *weight = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  weight->start_index = 0;
  weight->end_index = G_MAXUINT;
  pango_attr_list_insert (attrs, weight);
  return attrs;
}

static void
assert_unchanged_update (Fixture *f)
{
  guint count = 0;
  PangoAttrList *before = pango_attr_list_ref (gtk_label_get_attributes (GTK_LABEL (f->label)));
  PangoLayout *layout = g_object_ref (gtk_label_get_layout (GTK_LABEL (f->label)));
  gulong handler = g_signal_connect (f->label, "notify::attributes",
                                     G_CALLBACK (attributes_notified), &count);

  for (guint i = 0; i < 3; i++)
    g_signal_emit_by_name (f->frame, "style-updated");

  /* Old production always replaces the list and emits this notification.
   * Holding refs prevents allocator reuse from disguising invalidation. */
  g_assert_cmpuint (count, ==, 0);
  g_assert_true (gtk_label_get_attributes (GTK_LABEL (f->label)) == before);
  g_assert_true (gtk_label_get_layout (GTK_LABEL (f->label)) == layout);
  g_signal_handler_disconnect (f->label, handler);
  pango_attr_list_unref (before);
  g_object_unref (layout);
}

static void
test_unchanged_style (void)
{
  Fixture f = fixture_new ();
  assert_unchanged_update (&f);
  fixture_clear (&f);
}

static void
test_different_attributes_replaced (gconstpointer data)
{
  Fixture f = fixture_new ();
  guint count = 0;
  PangoAttrList *other = pango_attr_list_new ();
  gint variant = GPOINTER_TO_INT (data);
  PangoAttribute *weight = pango_attr_weight_new (variant == 0 ? PANGO_WEIGHT_NORMAL : PANGO_WEIGHT_BOLD);
  PangoAttrList *expected = bold_list ();
  PangoLayout *layout;
  gulong handler;

  weight->start_index = variant == 2 ? 2 : 0;
  weight->end_index = variant == 2 ? 5 : G_MAXUINT;
  pango_attr_list_insert (other, weight);
  if (variant != 2)
    pango_attr_list_insert (other, pango_attr_foreground_new (65535, 0, 0));
  gtk_label_set_attributes (GTK_LABEL (f.label), other);
  pango_attr_list_unref (other);
  layout = g_object_ref (gtk_label_get_layout (GTK_LABEL (f.label)));
  handler = g_signal_connect (f.label, "notify::attributes",
                               G_CALLBACK (attributes_notified), &count);
  g_signal_emit_by_name (f.frame, "style-updated");

  /* Preserve the old complete replacement, not just a matching weight.
   * A genuinely different list must still notify and invalidate layout. */
  g_assert_cmpuint (count, ==, 1);
  g_assert_true (pango_attr_list_equal (gtk_label_get_attributes (GTK_LABEL (f.label)), expected));
  g_assert_true (gtk_label_get_layout (GTK_LABEL (f.label)) != layout);
  g_signal_handler_disconnect (f.label, handler);
  g_object_unref (layout);
  pango_attr_list_unref (expected);
  assert_unchanged_update (&f);
  fixture_clear (&f);
}

static void
test_equal_attributes_different_instance (void)
{
  Fixture f = fixture_new ();
  PangoAttrList *equal = bold_list ();
  gtk_label_set_attributes (GTK_LABEL (f.label), equal);
  pango_attr_list_unref (equal);
  assert_unchanged_update (&f);
  fixture_clear (&f);
}

static void
test_replacement_label (void)
{
  Fixture f = fixture_new ();
  PangoAttrList *expected = bold_list ();
  GtkWidget *bin = gtk_event_box_new ();
  GtkWidget *replacement = gtk_label_new ("Replacement");
  gtk_container_add (GTK_CONTAINER (bin), replacement);
  gtk_widget_show_all (bin);
  gtk_frame_set_label_widget (GTK_FRAME (f.frame), bin);
  f.label = replacement;
  g_assert_true (pango_attr_list_equal (gtk_label_get_attributes (GTK_LABEL (f.label)), expected));
  pango_attr_list_unref (expected);
  assert_unchanged_update (&f);
  fixture_clear (&f);
}

static void
test_changed_style_spacing_and_font (void)
{
  Fixture f = fixture_new ();
  GtkCssProvider *provider = gtk_css_provider_new ();
  GError *error = NULL;
  PangoLayout *expected;
  gint width;
  gint old_width;

  /* Retain style-dependent margins/font recomputation. The optimization does
   * not cache the whole style or skip the style-updated handler. */
  gtk_css_provider_load_from_data (provider,
    "* { -GimpFrame-label-spacing: 7; font-size: 12px; }", -1, &error);
  g_assert_no_error (error);
  gtk_style_context_add_provider (gtk_widget_get_style_context (f.frame),
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_signal_emit_by_name (f.frame, "style-updated");
  expected = gtk_widget_create_pango_layout (f.frame, "    ");
  pango_layout_get_pixel_size (expected, &old_width, NULL);
  g_assert_cmpint (gtk_widget_get_margin_top (f.child), ==, 7);
  g_assert_cmpint (gtk_widget_get_margin_start (f.child), ==, old_width);
  g_object_unref (expected);

  gtk_css_provider_load_from_data (provider,
    "* { -GimpFrame-label-spacing: 19; font-size: 24px; }", -1, &error);
  g_assert_no_error (error);
  g_signal_emit_by_name (f.frame, "style-updated");
  expected = gtk_widget_create_pango_layout (f.frame, "    ");
  pango_layout_get_pixel_size (expected, &width, NULL);
  g_assert_cmpint (width, >, old_width);
  g_assert_cmpint (gtk_widget_get_margin_top (f.child), ==, 19);
  g_assert_cmpint (gtk_widget_get_margin_start (f.child), ==, width);
  g_object_unref (expected);
  gtk_style_context_remove_provider (gtk_widget_get_style_context (f.frame),
                                     GTK_STYLE_PROVIDER (provider));
  g_object_unref (provider);
  fixture_clear (&f);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return 77;
  g_test_add_func ("/gimp-frame-style/unchanged", test_unchanged_style);
  g_test_add_data_func ("/gimp-frame-style/different-weight", GINT_TO_POINTER (0), test_different_attributes_replaced);
  g_test_add_data_func ("/gimp-frame-style/same-weight-extra-attribute", GINT_TO_POINTER (1), test_different_attributes_replaced);
  g_test_add_data_func ("/gimp-frame-style/partial-range-bold", GINT_TO_POINTER (2), test_different_attributes_replaced);
  g_test_add_func ("/gimp-frame-style/equal-list-different-instance", test_equal_attributes_different_instance);
  g_test_add_func ("/gimp-frame-style/replacement-label", test_replacement_label);
  g_test_add_func ("/gimp-frame-style/style-change", test_changed_style_spacing_and_font);
  return g_test_run ();
}
