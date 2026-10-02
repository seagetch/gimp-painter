/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <string.h>
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpmybrush.h"
#include "core/gimppaintermybrush.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static void property_maps_and_parenting (void)
{
  GimpContext *parent = gimp_context_new (gimp, "painter-parent", NULL);
  GimpContext *child = gimp_context_new (gimp, "painter-child", NULL);
  GimpPainterMybrush *brush = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (parent, "context-brush"));
  GimpMybrush *standard = gimp_context_get_mybrush (parent);
  g_assert_cmpint (GIMP_CONTEXT_PROP_EXPAND, ==, 21);
  g_assert_cmpint (GIMP_CONTEXT_PROP_PAINTER_MYBRUSH, ==, 22);
  g_assert_cmpint (gimp_context_type_to_property (GIMP_TYPE_PAINTER_MYBRUSH), ==, 22);
  g_assert_cmpstr (gimp_context_type_to_prop_name (GIMP_TYPE_PAINTER_MYBRUSH), ==, "painter-mybrush");
  g_assert_cmpstr (gimp_context_type_to_signal_name (GIMP_TYPE_PAINTER_MYBRUSH), ==, "painter-mybrush-changed");
  g_assert_cmpint (gimp_context_type_to_property (GIMP_TYPE_MYBRUSH), ==, GIMP_CONTEXT_PROP_MYBRUSH);
  gimp_context_set_painter_mybrush (parent, brush);
  gimp_context_define_properties (child, GIMP_CONTEXT_PROP_MASK_ALL, FALSE);
  gimp_context_set_parent (child, parent);
  g_assert_true (gimp_context_get_painter_mybrush (child) == brush);
  g_assert_true (gimp_context_get_by_type (child, GIMP_TYPE_PAINTER_MYBRUSH) == GIMP_OBJECT (brush));
  gimp_object_set_name (GIMP_OBJECT (brush), "renamed-context-brush");
  g_assert_cmpstr (parent->painter_mybrush_name, ==, "renamed-context-brush");
  g_assert_true (gimp_context_get_mybrush (parent) == standard);
  gimp_context_copy_properties (parent, child, GIMP_CONTEXT_PROP_MASK_ALL);
  g_object_unref (child);g_object_unref (parent);g_object_unref (brush);
}
static void legacy_key_and_missing_name (void)
{
  GimpContext *context = gimp_context_new (gimp, "painter-legacy-key", NULL);
  GimpContext *copy = gimp_context_new (gimp, "painter-legacy-copy", NULL);
  GError *error = NULL;
  gchar *text;
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (context),
    "(mypaint-brush \"missing-but-retained-painter-resource\")", -1, NULL, &error));
  g_assert_no_error (error);
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH (gimp_context_get_painter_mybrush (context)));
  g_assert_cmpstr (context->painter_mybrush_name, ==, "missing-but-retained-painter-resource");
  text = gimp_config_serialize_to_string (GIMP_CONFIG (context), NULL);
  g_assert_nonnull (strstr (text, "(painter-mybrush \"missing-but-retained-painter-resource\")"));
  g_assert_null (strstr (text, "(mypaint-brush "));
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (copy), text, -1, NULL, &error));
  g_assert_no_error (error);g_assert_cmpstr (copy->painter_mybrush_name, ==, context->painter_mybrush_name);
  g_free (text);g_object_unref (copy);g_object_unref (context);
}
static void drop_context (gpointer data, GObject *unused)
{
  GimpContext **owner = data;
  g_clear_object (owner);
}
static void finalized (gpointer data, GObject *unused) { *((gboolean *) data) = TRUE; }
static void old_resource_finalizer_drops_context (void)
{
  GimpContext *context = gimp_context_new (gimp, "reentrant-context", NULL);
  GimpPainterMybrush *old = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (context, "old"));
  GimpPainterMybrush *replacement = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (context, "replacement"));
  gboolean gone = FALSE;
  g_object_weak_ref (G_OBJECT (context), finalized, &gone);
  gimp_context_set_painter_mybrush (context, old);
  g_object_weak_ref (G_OBJECT (old), drop_context, &context);
  g_object_unref (old);
  gimp_context_set_painter_mybrush (context, replacement);
  g_assert_null (context);g_assert_true (gone);
  g_object_unref (replacement);
}
typedef struct { GimpContext *context; GimpPainterMybrush *brush; guint signals; } Reentry;
static void replace_again (gpointer data, GObject *unused)
{
  Reentry *state = data;
  gimp_context_set_painter_mybrush (state->context, state->brush);
}
static void selected (GimpContext *context, GimpPainterMybrush *brush, gpointer data)
{
  Reentry *state = data;
  state->signals++;
  g_assert_true (brush == state->brush);
}
static void reentrant_selection_discards_stale_notify (void)
{
  GimpContext *context = gimp_context_new (gimp, "nested-context", NULL);
  GimpPainterMybrush *old = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (context, "old"));
  GimpPainterMybrush *outer = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (context, "outer"));
  GimpPainterMybrush *inner = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (context, "inner"));
  Reentry state = {context, inner, 0};
  gimp_context_set_painter_mybrush (context, old);
  g_signal_connect (context, "painter-mybrush-changed", G_CALLBACK (selected), &state);
  g_object_weak_ref (G_OBJECT (old), replace_again, &state);g_object_unref (old);
  gimp_context_set_painter_mybrush (context, outer);
  g_assert_true (gimp_context_get_painter_mybrush (context) == inner);
  g_assert_cmpuint (state.signals, ==, 1);
  g_object_unref (context);g_object_unref (outer);g_object_unref (inner);
}
int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-context/property-parent", property_maps_and_parenting);
  g_test_add_func ("/painter-context/legacy-key-missing-name", legacy_key_and_missing_name);
  g_test_add_func ("/painter-context/finalizer-drops-owner", old_resource_finalizer_drops_context);
  g_test_add_func ("/painter-context/nested-selection", reentrant_selection_discards_stale_notify);
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE);return result;
}
