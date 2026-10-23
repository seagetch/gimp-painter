/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <string.h>
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimppaintermybrush.h"
#include "core/gimpmypaintbrush.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
#include "core/gimptempbuf.h"
#include "painter/gimp-painter-binding.h"

static void load_all (void)
{
  const gchar *root = g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR");
  const gchar *groups[] = { "classic", "deevad", "experimental", "kaerhon", "ramon", "tanda" };
  guint count = 0;
  for (guint i = 0; i < G_N_ELEMENTS (groups); ++i)
    {
      gchar *path = g_build_filename (root, "data/painter-mypaint-brushes", groups[i], NULL);
      GDir *dir = g_dir_open (path, 0, NULL);
      const gchar *name;
      g_assert_nonnull (dir);
      while ((name = g_dir_read_name (dir)))
        {
          gchar *full;
          GFile *file;
          GFileInputStream *stream;
          GList *list;
          GError *error = NULL;
          GimpPainterMybrush *brush;
          GimpData *copy;
          gint width = -1, height = -1;
          gchar *a, *b;
          if (!g_str_has_suffix (name, ".myb")) continue;
          full = g_build_filename (path, name, NULL);
          file = g_file_new_for_path (full);
          stream = g_file_read (file, NULL, &error);
          g_assert_no_error (error);
          list = gimp_painter_mybrush_load (NULL, file, G_INPUT_STREAM (stream), &error);
          g_assert_no_error (error); g_assert_cmpuint (g_list_length (list), ==, 1);
          brush = list->data;
          g_assert_true (GIMP_IS_PAINTER_MYBRUSH (brush));
          g_assert_false (gimp_data_is_dirty (GIMP_DATA (brush)));
          g_assert_true (gimp_viewable_get_size (GIMP_VIEWABLE (brush), &width, &height));
          g_assert_cmpint (width, >, 0); g_assert_cmpint (height, >, 0);
          copy = gimp_data_duplicate (GIMP_DATA (brush));
          g_assert_true (GIMP_IS_PAINTER_MYBRUSH (copy));
          a = gimp_painter_mybrush_dup_json (brush, &error);
          b = gimp_painter_mybrush_dup_json (GIMP_PAINTER_MYBRUSH (copy), &error);
          g_assert_no_error (error); g_assert_cmpstr (a, ==, b);
          g_free (a); g_free (b); g_object_unref (copy);
          g_object_unref (brush); g_list_free (list); g_object_unref (stream); g_object_unref (file); g_free (full); ++count;
        }
      g_dir_close (dir); g_free (path);
    }
  g_assert_cmpuint (count, ==, 177);
}
static void stream_icon_and_copy (void)
{
  GimpPainterMybrush *brush = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "original"));
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  GdkPixbuf *owned;
  GOutputStream *output = g_memory_output_stream_new_resizable ();
  GInputStream *input;
  GFile *file = g_file_new_for_path ("/tmp/nonexistent-painter-preview-roundtrip.myb");
  GList *loaded;
  GError *error = NULL;
  GimpTempBuf *preview;
  const gchar *json = "{\"version\":3,\"settings\":{\"texture_grain\":{\"base_value\":0.37,\"inputs\":{}}},\"unknown\":[1,2]}";
  g_assert_true (gimp_painter_mybrush_set_json (brush, json, &error));
  gdk_pixbuf_fill (icon, 0xa04080ff);
  g_assert_true (gimp_painter_mybrush_set_icon (brush, icon, &error));
  g_assert_true (gimp_painter_mybrush_set_icon (brush, icon, &error));
  g_object_unref (icon);
  owned = gimp_painter_mybrush_ref_icon (brush); g_assert_nonnull (owned);
  g_assert_cmpint (gdk_pixbuf_get_rowstride (owned), >=, 12); g_object_unref (owned);
  preview = gimp_viewable_get_new_preview (GIMP_VIEWABLE (brush), gimp_get_user_context (gimp), 12, 20, NULL);
  g_assert_nonnull (preview); g_assert_cmpint (gimp_temp_buf_get_width (preview), ==, 12);
  gimp_temp_buf_unref (preview);
  gimp_object_set_name (GIMP_OBJECT (brush), "renamed");
  g_assert_true (GIMP_DATA_GET_CLASS (brush)->save (GIMP_DATA (brush), output, &error));
  g_assert_no_error (error);
  input = g_memory_input_stream_new_from_data (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (output)),
    g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (output)), NULL);
  loaded = gimp_painter_mybrush_load (NULL, file, input, &error);
  g_assert_no_error (error); g_assert_nonnull (loaded);
  g_assert_cmpstr (gimp_object_get_name (loaded->data), ==, "renamed");
  owned = gimp_painter_mybrush_ref_icon (loaded->data); g_assert_nonnull (owned);
  g_assert_cmpint (gdk_pixbuf_get_width (owned), ==, 3); g_assert_cmpint (gdk_pixbuf_get_height (owned), ==, 5);
  g_assert_cmpuint (gdk_pixbuf_get_pixels (owned)[0], ==, 0xa0); g_object_unref (owned);
  g_object_unref (loaded->data); g_list_free (loaded); g_object_unref (input);
  g_assert_true (g_output_stream_close (output, NULL, &error));
  g_assert_false (GIMP_DATA_GET_CLASS (brush)->save (GIMP_DATA (brush), output, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED); g_clear_error (&error);
  g_assert_false (gimp_painter_mybrush_set_json (brush, "{bad", &error)); g_assert_nonnull (error); g_clear_error (&error);
  owned = gimp_painter_mybrush_ref_icon (brush); g_assert_nonnull (owned); g_object_unref (owned);
  g_assert_true (gimp_painter_mybrush_set_icon (brush, NULL, &error)); g_assert_null (gimp_painter_mybrush_ref_icon (brush));
  g_object_unref (output); g_object_unref (file); g_object_unref (brush);
}
static void closed_owner (void)
{
  GimpPainterMybrush *brush = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "closed"));
  GError *error = NULL;
  g_assert_true (gimp_painter_binding_close (G_OBJECT (brush), &error));
  g_assert_false (gimp_painter_mybrush_set_json (brush, "{\"version\":3}", &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  g_object_run_dispose (G_OBJECT (brush)); g_object_run_dispose (G_OBJECT (brush)); g_object_unref (brush);
}

static const gchar copy_fixture_json[] =
  "{\"version\":3,\"parent_brush_name\":\"fixture-parent\",\"group\":\"copy-tests\","
  "\"settings\":{"
    "\"opaque\":{\"base_value\":0.625,\"inputs\":{"
      "\"pressure\":[[0,0],[0.25,0.125],[1,1]],"
      "\"speed1\":[[0,-0.5],[1,0.75]],"
      "\"future_input\":{\"retain\":[true,null,\"opaque\"]}},"
      "\"future_setting_metadata\":{\"label\":\"keep me\"}},"
    "\"texture_grain\":{\"base_value\":0.375,\"inputs\":{}},"
    "\"future_setting\":{\"nested\":[1,{\"keep\":false}]}},"
  "\"switches\":{\"non_incremental\":true,\"use_gimp_texture\":false,"
    "\"future_switch\":{\"retain\":true}},"
  "\"texts\":{\"brushmark_name\":\"fixture brushmark\",\"texture_name\":null,"
    "\"future_text\":[\"keep\",null]},"
  "\"future_root\":{\"array\":[3,null,{\"nested\":\"preserved\"}]}}";

static gchar *
brush_json (GimpPainterMybrush *brush)
{
  GError *error = NULL;
  gchar *json = gimp_painter_mybrush_dup_json (brush, &error);
  g_assert_no_error (error);
  g_assert_nonnull (json);
  return json;
}

static void
assert_brush_json (GimpPainterMybrush *brush,
                   const gchar        *expected)
{
  gchar *json = brush_json (brush);
  g_assert_cmpstr (json, ==, expected);
  g_free (json);
}

static void
assert_fixture_payload (GimpPainterMybrush *brush)
{
  JsonParser *actual = json_parser_new ();
  JsonParser *expected = json_parser_new ();
  GError *error = NULL;
  gchar *json = brush_json (brush);

  g_assert_true (json_parser_load_from_data (actual, json, -1, &error));
  g_assert_no_error (error);
  g_assert_true (json_parser_load_from_data (expected, copy_fixture_json, -1, &error));
  g_assert_no_error (error);
  /* Compare every metadata member and curve, including opaque future data.
   * Only the separately tested embedded icon may differ from the fixture. */
  json_object_remove_member (json_node_get_object (json_parser_get_root (actual)),
                             "painter_preview_png");
  g_assert_true (json_node_equal (json_parser_get_root (actual),
                                 json_parser_get_root (expected)));
  g_free (json);
  g_object_unref (actual);
  g_object_unref (expected);
}

static guchar *
icon_pixel (GdkPixbuf *icon,
            gint       x,
            gint       y)
{
  return gdk_pixbuf_get_pixels (icon) + y * gdk_pixbuf_get_rowstride (icon) +
         x * gdk_pixbuf_get_n_channels (icon);
}

static void
assert_icon_pixels (GdkPixbuf *actual,
                    GdkPixbuf *expected)
{
  gint row_bytes;

  g_assert_nonnull (actual);
  g_assert_nonnull (expected);
  g_assert_cmpint (gdk_pixbuf_get_colorspace (actual), ==, gdk_pixbuf_get_colorspace (expected));
  g_assert_cmpint (gdk_pixbuf_get_bits_per_sample (actual), ==, gdk_pixbuf_get_bits_per_sample (expected));
  g_assert_cmpint (gdk_pixbuf_get_has_alpha (actual), ==, gdk_pixbuf_get_has_alpha (expected));
  g_assert_cmpint (gdk_pixbuf_get_n_channels (actual), ==, gdk_pixbuf_get_n_channels (expected));
  g_assert_cmpint (gdk_pixbuf_get_width (actual), ==, gdk_pixbuf_get_width (expected));
  g_assert_cmpint (gdk_pixbuf_get_height (actual), ==, gdk_pixbuf_get_height (expected));
  row_bytes = gdk_pixbuf_get_width (expected) * gdk_pixbuf_get_n_channels (expected);
  for (gint y = 0; y < gdk_pixbuf_get_height (expected); ++y)
    g_assert_cmpmem (icon_pixel (actual, 0, y), row_bytes,
                     icon_pixel (expected, 0, y), row_bytes);
}

static GimpPainterMybrush *
new_icon_copy_fixture (const gchar *name,
                       gboolean     alpha)
{
  GimpPainterMybrush *brush = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, name));
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, alpha, 8, 3, 5);
  GError *error = NULL;

  g_assert_nonnull (brush);
  g_assert_nonnull (icon);
  for (gint y = 0; y < 5; ++y)
    for (gint x = 0; x < 3; ++x)
      for (gint c = 0; c < gdk_pixbuf_get_n_channels (icon); ++c)
        icon_pixel (icon, x, y)[c] = (guchar) (17 + 23 * x + 31 * y + 47 * c);
  g_assert_true (gimp_painter_mybrush_set_json (brush, copy_fixture_json, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_painter_mybrush_set_icon (brush, icon, &error));
  g_assert_no_error (error);
  g_object_unref (icon);
  assert_fixture_payload (brush);
  gimp_data_clean (GIMP_DATA (brush));
  return brush;
}

static void
copy_icon_pixel_independence (gconstpointer user_data)
{
  gboolean duplicate = GPOINTER_TO_INT (user_data);
  /* Exercise both RGBA and padded RGB rows through the generic GimpData API. */
  GimpPainterMybrush *source = new_icon_copy_fixture ("copy-source", duplicate);
  GimpPainterMybrush *target;
  GimpPainterMybrush *decoded;
  GdkPixbuf *source_icon = gimp_painter_mybrush_ref_icon (source);
  GdkPixbuf *target_icon;
  GdkPixbuf *expected_source;
  GdkPixbuf *expected_target;
  GdkPixbuf *decoded_icon;
  gchar *original_json = brush_json (source);
  gchar *target_json;
  GError *error = NULL;

  /* The live icon can differ from its last serialized PNG. Copy its current
   * pixels, not an icon reconstructed from the resource's older PNG bytes. */
  icon_pixel (source_icon, 2, 4)[1] ^= 0x5a;
  expected_source = gdk_pixbuf_copy (source_icon);
  assert_brush_json (source, original_json);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));

  if (duplicate)
    {
      target = GIMP_PAINTER_MYBRUSH (gimp_data_duplicate (GIMP_DATA (source)));
      g_assert_nonnull (target);
      g_assert_null (gimp_object_get_name (GIMP_OBJECT (target)));
    }
  else
    {
      GdkPixbuf *old_icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 7, 2);
      target = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "copy-target"));
      gdk_pixbuf_fill (old_icon, 0xabcdef80);
      g_assert_true (gimp_painter_mybrush_set_json (target, "{\"version\":3,\"replace_me\":true}", &error));
      g_assert_no_error (error);
      g_assert_true (gimp_painter_mybrush_set_icon (target, old_icon, &error));
      g_assert_no_error (error);
      g_object_unref (old_icon);
      gimp_data_clean (GIMP_DATA (target));
      gimp_data_copy (GIMP_DATA (target), GIMP_DATA (source));
      g_assert_cmpstr (gimp_object_get_name (GIMP_OBJECT (target)), ==, "copy-target");
    }

  target_icon = gimp_painter_mybrush_ref_icon (target);
  assert_icon_pixels (target_icon, expected_source);
  assert_icon_pixels (source_icon, expected_source);
  assert_brush_json (target, original_json);
  assert_brush_json (source, original_json);
  assert_fixture_payload (target);
  g_assert_cmpstr (gimp_object_get_name (GIMP_OBJECT (source)), ==, "copy-source");
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (target)));
  gimp_data_clean (GIMP_DATA (target));

  /* Both checks fail with shared live icon storage. Compare visible bytes,
   * not object identity or uninitialized row-padding bytes. */
  icon_pixel (target_icon, 0, 0)[0] ^= 0x3c;
  assert_icon_pixels (source_icon, expected_source);
  expected_target = gdk_pixbuf_copy (target_icon);
  icon_pixel (source_icon, 1, 1)[2] ^= 0x6d;
  icon_pixel (expected_source, 1, 1)[2] ^= 0x6d;
  assert_icon_pixels (target_icon, expected_target);
  assert_brush_json (source, original_json);
  assert_brush_json (target, original_json);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (target)));

  /* Direct pixel writes do not serialize or dirty the brush. Explicitly
   * setting the icon does, without changing the other brush's resource. */
  g_assert_true (gimp_painter_mybrush_set_icon (target, target_icon, &error));
  g_assert_no_error (error);
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (target)));
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  target_json = brush_json (target);
  g_assert_cmpstr (target_json, !=, original_json);
  assert_brush_json (source, original_json);
  assert_fixture_payload (target);
  decoded = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "decoded-copy"));
  g_assert_true (gimp_painter_mybrush_set_json (decoded, target_json, &error));
  g_assert_no_error (error);
  decoded_icon = gimp_painter_mybrush_ref_icon (decoded);
  assert_icon_pixels (decoded_icon, expected_target);
  g_object_unref (decoded_icon);
  g_object_unref (decoded);

  g_assert_true (gimp_painter_mybrush_set_icon (source, source_icon, &error));
  g_assert_no_error (error);
  assert_brush_json (target, target_json);
  assert_fixture_payload (source);
  assert_icon_pixels (source_icon, expected_source);
  assert_icon_pixels (target_icon, expected_target);
  g_object_unref (source_icon);
  g_object_unref (source);
  assert_icon_pixels (target_icon, expected_target);
  g_object_unref (target_icon);
  g_object_unref (target);
  g_object_unref (expected_source);
  g_object_unref (expected_target);
  g_free (original_json);
  g_free (target_json);
}

static void
copy_without_icon_clears_target (void)
{
  GimpPainterMybrush *source = GIMP_PAINTER_MYBRUSH (gimp_painter_mybrush_new (NULL, "no-icon"));
  GimpPainterMybrush *target = new_icon_copy_fixture ("old-icon", TRUE);
  GimpData *duplicate;
  GdkPixbuf *old_icon = gimp_painter_mybrush_ref_icon (target);
  gpointer weak_icon = old_icon;
  GError *error = NULL;
  gchar *json;
  gint width = 0, height = 0;

  g_assert_true (gimp_painter_mybrush_set_json (source, copy_fixture_json, &error));
  g_assert_no_error (error);
  gimp_data_clean (GIMP_DATA (source));
  json = brush_json (source);
  g_object_add_weak_pointer (G_OBJECT (old_icon), &weak_icon);
  g_object_unref (old_icon);
  gimp_data_copy (GIMP_DATA (target), GIMP_DATA (source));
  g_assert_null (weak_icon);
  g_assert_null (gimp_painter_mybrush_ref_icon (target));
  g_assert_true (gimp_viewable_get_size (GIMP_VIEWABLE (target), &width, &height));
  g_assert_cmpint (width, ==, 48);
  g_assert_cmpint (height, ==, 48);
  assert_brush_json (target, json);
  assert_fixture_payload (target);
  g_assert_true (gimp_data_is_dirty (GIMP_DATA (target)));
  duplicate = gimp_data_duplicate (GIMP_DATA (source));
  g_assert_nonnull (duplicate);
  g_assert_null (gimp_painter_mybrush_ref_icon (GIMP_PAINTER_MYBRUSH (duplicate)));
  assert_brush_json (GIMP_PAINTER_MYBRUSH (duplicate), json);
  assert_brush_json (source, json);
  g_assert_null (gimp_painter_mybrush_ref_icon (source));
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_free (json);
  g_object_unref (duplicate);
  g_object_unref (target);
  g_object_unref (source);
}

static void
self_copy_coherent (void)
{
  GimpPainterMybrush *brush = new_icon_copy_fixture ("self-copy", TRUE);
  GdkPixbuf *before = gimp_painter_mybrush_ref_icon (brush);
  GdkPixbuf *expected = gdk_pixbuf_copy (before);
  GdkPixbuf *after;
  gchar *json = brush_json (brush);

  /* The public GimpData API makes self-copy a no-op, including dirty state. */
  gimp_data_copy (GIMP_DATA (brush), GIMP_DATA (brush));
  after = gimp_painter_mybrush_ref_icon (brush);
  g_assert_true (after == before);
  assert_icon_pixels (after, expected);
  assert_brush_json (brush, json);
  assert_fixture_payload (brush);
  g_assert_cmpstr (gimp_object_get_name (GIMP_OBJECT (brush)), ==, "self-copy");
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (brush)));
  g_free (json);
  g_object_unref (expected);
  g_object_unref (after);
  g_object_unref (before);
  g_object_unref (brush);
}

typedef struct { GimpData *target; gboolean fired; } ReleaseOnIconFinalize;
static void release_target_on_icon_finalize (gpointer user_data, GObject *where_icon_was)
{
  ReleaseOnIconFinalize *state = user_data;
  GimpData *target = state->target;
  state->target = NULL;
  state->fired = TRUE;
  g_object_unref (target);
}
static void copy_icon_finalizer_reentry (void)
{
  GimpData *source = gimp_painter_mybrush_new (NULL, "source");
  GimpData *target = gimp_painter_mybrush_new (NULL, "target");
  GdkPixbuf *icon = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 3, 5);
  GError *error = NULL;
  ReleaseOnIconFinalize state = { target, FALSE };
  gpointer weak_target = target;
  gdk_pixbuf_fill (icon, 0xff0000ff);
  g_assert_true (gimp_painter_mybrush_set_icon (GIMP_PAINTER_MYBRUSH (target), icon, &error));
  g_assert_no_error (error);
  g_object_weak_ref (G_OBJECT (icon), release_target_on_icon_finalize, &state);
  g_object_add_weak_pointer (G_OBJECT (target), &weak_target);
  g_object_unref (icon); /* target is now the old icon's sole owner */
  gimp_data_copy (target, source);
  g_assert_true (state.fired);
  g_assert_null (state.target);
  g_assert_null (weak_target); /* finalizes safely after the complete callback */
  g_object_unref (source);
}
/* This translation unit must remain C: assigning and checking the exact legacy
 * function pointer also exercises the exported C ABI, not a C++ substitute. */
typedef GimpData *(*NamedBrushDuplicate) (GimpMypaintBrush *);
_Static_assert (_Generic (&gimp_mypaint_brush_duplicate,
                         NamedBrushDuplicate: 1, default: 0),
                "The named duplicate must retain its exact legacy C signature");
static NamedBrushDuplicate const named_duplicate = gimp_mypaint_brush_duplicate;

static void
count_named_source_signal (GObject  *object,
                           gpointer  data)
{
  (void) object;
  ++*(guint *) data;
}

static void
named_duplicate_c_signature_and_payload (void)
{
  GimpMypaintBrush *source = GIMP_MYPAINT_BRUSH (
    gimp_painter_mybrush_new (NULL, "Native name \316\261 differs from parent"));
  GimpData *copy;
  GimpData *generic;
  GFile *file = g_file_new_for_path ("/tmp/named-duplicate-source-only.myb");
  GError *error = NULL;
  gchar *json;
  guint signals = 0;

  g_assert_true (GIMP_IS_MYPAINT_BRUSH (source));
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH (source));
  g_assert_true (gimp_painter_mybrush_set_json (source, copy_fixture_json, &error));
  g_assert_no_error (error);
  gimp_data_set_file (GIMP_DATA (source), file, FALSE, FALSE);
  gimp_data_clean (GIMP_DATA (source));
  json = brush_json (source);
  g_signal_connect (source, "dirty", G_CALLBACK (count_named_source_signal), &signals);
  g_signal_connect (source, "name-changed", G_CALLBACK (count_named_source_signal), &signals);
  g_signal_connect (source, "settings-changed", G_CALLBACK (count_named_source_signal), &signals);

  copy = named_duplicate (source);
  g_assert_nonnull (copy);
  g_assert_true (GIMP_IS_MYPAINT_BRUSH (copy));
  g_assert_true (copy != GIMP_DATA (source));
  g_assert_false (g_object_is_floating (copy));
  g_assert_cmpstr (gimp_object_get_name (copy), ==, "Native name \316\261 differs from parent");
  assert_fixture_payload (GIMP_MYPAINT_BRUSH (copy));
  assert_brush_json (GIMP_MYPAINT_BRUSH (copy), json);
  g_assert_null (gimp_data_get_file (copy));
  g_assert_null (gimp_painter_mybrush_ref_icon (GIMP_MYPAINT_BRUSH (copy)));
  g_assert_true (gimp_data_get_file (GIMP_DATA (source)) == file);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Native name \316\261 differs from parent");
  assert_brush_json (source, json);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_assert_cmpuint (signals, ==, 0);

  /* The legacy named entry point must not change generic GimpData behavior. */
  generic = gimp_data_duplicate (GIMP_DATA (source));
  g_assert_nonnull (generic);
  g_assert_null (gimp_object_get_name (generic));
  assert_fixture_payload (GIMP_MYPAINT_BRUSH (generic));
  g_assert_cmpuint (signals, ==, 0);
  g_object_unref (generic);
  g_object_unref (copy);
  g_object_unref (source);
  g_object_unref (file);
  g_free (json);
}

static void
named_duplicate_live_icon (gconstpointer user_data)
{
  gboolean alpha = GPOINTER_TO_INT (user_data);
  GimpMypaintBrush *source = new_icon_copy_fixture ("Named live icon", alpha);
  GdkPixbuf *source_icon = gimp_painter_mybrush_ref_icon (source);
  GdkPixbuf *expected;
  GdkPixbuf *first_icon;
  GdkPixbuf *second_icon;
  GimpMypaintBrush *first;
  GimpMypaintBrush *second;
  gchar *json = brush_json (source);

  if (!alpha)
    g_assert_cmpint (gdk_pixbuf_get_rowstride (source_icon), >,
                     3 * gdk_pixbuf_get_n_channels (source_icon));
  /* A serialized round-trip would silently revert this live pixel edit. */
  icon_pixel (source_icon, 2, 4)[1] ^= 0x5a;
  expected = gdk_pixbuf_copy (source_icon);
  first = GIMP_MYPAINT_BRUSH (named_duplicate (source));
  g_assert_nonnull (first);
  second = GIMP_MYPAINT_BRUSH (named_duplicate (first));
  g_assert_nonnull (second);
  g_assert_true (first != source && second != source && second != first);
  first_icon = gimp_painter_mybrush_ref_icon (first);
  second_icon = gimp_painter_mybrush_ref_icon (second);
  g_assert_true (first_icon != source_icon && second_icon != source_icon &&
                 second_icon != first_icon);
  assert_icon_pixels (first_icon, expected);
  assert_icon_pixels (second_icon, expected);
  assert_brush_json (source, json);
  assert_brush_json (first, json);
  assert_brush_json (second, json);
  assert_fixture_payload (first);
  assert_fixture_payload (second);
  g_assert_cmpstr (gimp_object_get_name (first), ==, "Named live icon");
  g_assert_cmpstr (gimp_object_get_name (second), ==, "Named live icon");
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));

  icon_pixel (first_icon, 0, 0)[0] ^= 0x3c;
  assert_icon_pixels (source_icon, expected);
  assert_icon_pixels (second_icon, expected);
  icon_pixel (source_icon, 1, 1)[2] ^= 0x6d;
  assert_icon_pixels (second_icon, expected);
  g_object_unref (source_icon);
  g_object_unref (source);
  g_object_unref (first_icon);
  g_object_unref (first);
  assert_icon_pixels (second_icon, expected);
  assert_brush_json (second, json);
  g_assert_cmpstr (gimp_object_get_name (second), ==, "Named live icon");
  g_object_unref (second_icon);
  g_object_unref (second);
  g_object_unref (expected);
  g_free (json);
}

static void
named_duplicate_release_orders (void)
{
  for (guint source_first = 0; source_first < 2; ++source_first)
    {
      GimpMypaintBrush *source = new_icon_copy_fixture ("Independent lifetime", TRUE);
      gchar *json = brush_json (source);
      gpointer weak_source = source;
      g_object_add_weak_pointer (G_OBJECT (source), &weak_source);
      for (guint iteration = 0; iteration < 8; ++iteration)
        {
          GimpData *copy = named_duplicate (source);
          gpointer weak_copy = copy;
          g_assert_nonnull (copy);
          g_object_add_weak_pointer (G_OBJECT (copy), &weak_copy);
          if (source_first && iteration == 7)
            {
              g_object_unref (source);
              g_assert_null (weak_source);
              assert_brush_json (GIMP_MYPAINT_BRUSH (copy), json);
              g_assert_cmpstr (gimp_object_get_name (copy), ==, "Independent lifetime");
            }
          g_object_unref (copy);
          g_assert_null (weak_copy); /* transfer-full: no hidden extra reference */
          if (weak_source)
            {
              assert_brush_json (source, json);
              g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
            }
        }
      if (weak_source)
        g_object_unref (source);
      g_assert_null (weak_source);
      g_free (json);
    }
}

static void
named_duplicate_invalid_inputs (void)
{
  GObject *wrong_type = g_object_new (G_TYPE_OBJECT, NULL);
  GimpMypaintBrush *source = new_icon_copy_fixture ("Temporarily nameless", TRUE);
  gchar *json = brush_json (source);
  GdkPixbuf *icon = gimp_painter_mybrush_ref_icon (source);
  GdkPixbuf *expected = gdk_pixbuf_copy (icon);

  g_assert_null (named_duplicate (NULL));
  /* Deliberate wrong-type input must not go through a warning-producing cast. */
  g_assert_null (named_duplicate ((GimpMypaintBrush *) wrong_type));
  gimp_object_set_name (GIMP_OBJECT (source), "");
  gimp_data_clean (GIMP_DATA (source));
  g_assert_null (named_duplicate (source));
  g_assert_cmpstr (gimp_object_get_name (source), ==, "");
  assert_brush_json (source, json);
  assert_icon_pixels (icon, expected);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  gimp_object_set_name (GIMP_OBJECT (source), NULL);
  gimp_data_clean (GIMP_DATA (source));
  g_assert_null (named_duplicate (source));
  g_assert_null (gimp_object_get_name (source));
  assert_brush_json (source, json);
  assert_icon_pixels (icon, expected);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_object_unref (expected);
  g_object_unref (icon);
  g_object_unref (source);
  g_object_unref (wrong_type);
  g_free (json);
}

static void
named_duplicate_closed_owner (void)
{
  GimpMypaintBrush *source = new_icon_copy_fixture ("Closed named source", TRUE);
  gchar *json = brush_json (source);
  GError *error = NULL;

  g_assert_true (gimp_painter_binding_close (G_OBJECT (source), &error));
  g_assert_no_error (error);
  g_assert_null (named_duplicate (source));
  g_assert_null (named_duplicate (source));
  assert_brush_json (source, json); /* Closed reads remain available. */
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Closed named source");
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_object_run_dispose (G_OBJECT (source));
  g_assert_null (named_duplicate (source));
  g_object_run_dispose (G_OBJECT (source));
  g_object_unref (source);
  g_free (json);
}

static gpointer
named_duplicate_on_wrong_thread (gpointer source)
{
  return named_duplicate (source);
}

static void
named_duplicate_wrong_thread (void)
{
  GimpMypaintBrush *source = new_icon_copy_fixture ("Thread-bound named source", FALSE);
  gchar *json = brush_json (source);
  GdkPixbuf *icon = gimp_painter_mybrush_ref_icon (source);
  GdkPixbuf *expected = gdk_pixbuf_copy (icon);
  GimpData *copy;
  guint signals = 0;

  g_signal_connect (source, "dirty", G_CALLBACK (count_named_source_signal), &signals);
  g_signal_connect (source, "name-changed", G_CALLBACK (count_named_source_signal), &signals);
  g_signal_connect (source, "settings-changed", G_CALLBACK (count_named_source_signal), &signals);
  for (guint i = 0; i < 4; ++i)
    {
      GThread *worker = g_thread_new ("named-duplicate-wrong-thread",
                                     named_duplicate_on_wrong_thread, source);
      g_assert_null (g_thread_join (worker));
    }
  assert_brush_json (source, json);
  assert_icon_pixels (icon, expected);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Thread-bound named source");
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_assert_cmpuint (signals, ==, 0);
  copy = named_duplicate (source); /* A rejection does not close the owner. */
  g_assert_nonnull (copy);
  assert_fixture_payload (GIMP_MYPAINT_BRUSH (copy));
  g_object_unref (copy);
  g_object_unref (expected);
  g_object_unref (icon);
  g_object_unref (source);
  g_free (json);
}

typedef struct
{
  GimpMypaintBrush *source;
  GimpData        *copy;
  guint            calls;
} DuplicateOnIconFinalize;

static void
named_duplicate_on_icon_finalize (gpointer data,
                                 GObject *where_icon_was)
{
  DuplicateOnIconFinalize *state = data;
  (void) where_icon_was;
  ++state->calls;
  state->copy = named_duplicate (state->source);
}

static void
named_duplicate_finalizer_reentry (void)
{
  GimpMypaintBrush *source = new_icon_copy_fixture ("Reentrant named source", TRUE);
  GimpData *copy = named_duplicate (source);
  GdkPixbuf *icon;
  DuplicateOnIconFinalize state = { source, NULL, 0 };
  gchar *json = brush_json (source);
  GError *error = NULL;

  g_assert_nonnull (copy);
  icon = gimp_painter_mybrush_ref_icon (GIMP_MYPAINT_BRUSH (copy));
  g_object_weak_ref (G_OBJECT (icon), named_duplicate_on_icon_finalize, &state);
  g_object_unref (icon);
  g_object_unref (copy);
  g_assert_cmpuint (state.calls, ==, 1);
  g_assert_nonnull (state.copy);
  assert_brush_json (GIMP_MYPAINT_BRUSH (state.copy), json);
  g_assert_cmpstr (gimp_object_get_name (state.copy), ==, "Reentrant named source");
  g_object_unref (state.copy);
  state.copy = NULL;

  /* Closing an icon reenters the exact public entry point with its owner still
   * alive but already closing. It must reject instead of resurrecting it. */
  icon = gimp_painter_mybrush_ref_icon (source);
  g_object_weak_ref (G_OBJECT (icon), named_duplicate_on_icon_finalize, &state);
  g_object_unref (icon);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (source), &error));
  g_assert_no_error (error);
  g_assert_cmpuint (state.calls, ==, 2);
  g_assert_null (state.copy);
  assert_brush_json (source, json);
  g_object_unref (source);
  g_free (json);
}

#ifdef GIMP_TEST_MYBRUSH_WRAP_PIXBUF_COPY
void     gimp_test_mybrush_copy_fault_arm      (const GdkPixbuf *icon);
gboolean gimp_test_mybrush_copy_fault_consumed (void);

static void
named_duplicate_copy_failure_atomic (void)
{
  GimpMypaintBrush *source = new_icon_copy_fixture ("Allocation failure source", FALSE);
  GdkPixbuf *icon = gimp_painter_mybrush_ref_icon (source);
  GdkPixbuf *expected = gdk_pixbuf_copy (icon);
  GdkPixbuf *unrelated;
  GdkPixbuf *copied_icon;
  GimpData *copy;
  GimpData *generic;
  GFile *file = g_file_new_for_path ("/tmp/failed-named-duplicate-source-only.myb");
  gchar *json = brush_json (source);
  guint signals = 0;

  gimp_data_set_file (GIMP_DATA (source), file, FALSE, FALSE);
  gimp_data_clean (GIMP_DATA (source));
  g_signal_connect (source, "dirty", G_CALLBACK (count_named_source_signal), &signals);
  g_signal_connect (source, "name-changed", G_CALLBACK (count_named_source_signal), &signals);
  g_signal_connect (source, "settings-changed", G_CALLBACK (count_named_source_signal), &signals);
  gimp_test_mybrush_copy_fault_arm (icon);
  unrelated = gdk_pixbuf_copy (expected);
  g_assert_nonnull (unrelated);
  g_assert_false (gimp_test_mybrush_copy_fault_consumed ());
  g_object_unref (unrelated);
  copy = named_duplicate (source); /* Actual C call contains C++ bad_alloc. */
  g_assert_null (copy);
  g_assert_true (gimp_test_mybrush_copy_fault_consumed ());
  assert_brush_json (source, json);
  assert_icon_pixels (icon, expected);
  g_assert_cmpstr (gimp_object_get_name (source), ==, "Allocation failure source");
  g_assert_true (gimp_data_get_file (GIMP_DATA (source)) == file);
  g_assert_false (gimp_data_is_dirty (GIMP_DATA (source)));
  g_assert_cmpuint (signals, ==, 0);

  copy = named_duplicate (source);
  g_assert_nonnull (copy);
  g_assert_true (gimp_test_mybrush_copy_fault_consumed ());
  g_assert_cmpstr (gimp_object_get_name (copy), ==, "Allocation failure source");
  g_assert_null (gimp_data_get_file (copy));
  assert_brush_json (GIMP_MYPAINT_BRUSH (copy), json);
  copied_icon = gimp_painter_mybrush_ref_icon (GIMP_MYPAINT_BRUSH (copy));
  assert_icon_pixels (copied_icon, expected);
  g_object_unref (copied_icon);
  generic = gimp_data_duplicate (GIMP_DATA (source));
  g_assert_nonnull (generic);
  g_assert_null (gimp_object_get_name (generic));
  assert_brush_json (GIMP_MYPAINT_BRUSH (generic), json);
  g_assert_cmpuint (signals, ==, 0);
  g_object_unref (generic);
  g_object_unref (copy);
  g_object_unref (file);
  g_object_unref (expected);
  g_object_unref (icon);
  g_object_unref (source);
  g_free (json);
}
#endif

void gimp_test_painter_mybrush_duplicate_handle_register (void);

int main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_SRCDIR", "app/tests/gimpdir");
  gimp = gimp_init_for_testing ();
  g_test_add_func ("/painter-mybrush/177-native-load-copy", load_all);
  g_test_add_func ("/painter-mybrush/stream-icon-copy", stream_icon_and_copy);
  g_test_add_func ("/painter-mybrush/closed-owner", closed_owner);
  g_test_add_data_func ("/painter-mybrush/duplicate-icon-pixel-independence", GINT_TO_POINTER (TRUE), copy_icon_pixel_independence);
  g_test_add_data_func ("/painter-mybrush/copy-icon-pixel-independence", GINT_TO_POINTER (FALSE), copy_icon_pixel_independence);
  g_test_add_func ("/painter-mybrush/copy-without-icon-clears-target", copy_without_icon_clears_target);
  g_test_add_func ("/painter-mybrush/self-copy-coherent", self_copy_coherent);
  g_test_add_func ("/painter-mybrush/copy-icon-finalizer-reentry", copy_icon_finalizer_reentry);
  g_test_add_func ("/painter-mybrush/named-duplicate/c-signature-and-payload", named_duplicate_c_signature_and_payload);
  g_test_add_data_func ("/painter-mybrush/named-duplicate/live-icon-rgb-padded", GINT_TO_POINTER (FALSE), named_duplicate_live_icon);
  g_test_add_data_func ("/painter-mybrush/named-duplicate/live-icon-rgba", GINT_TO_POINTER (TRUE), named_duplicate_live_icon);
  g_test_add_func ("/painter-mybrush/named-duplicate/release-orders", named_duplicate_release_orders);
  g_test_add_func ("/painter-mybrush/named-duplicate/invalid-inputs", named_duplicate_invalid_inputs);
  g_test_add_func ("/painter-mybrush/named-duplicate/closed-owner", named_duplicate_closed_owner);
  g_test_add_func ("/painter-mybrush/named-duplicate/wrong-thread", named_duplicate_wrong_thread);
  g_test_add_func ("/painter-mybrush/named-duplicate/finalizer-reentry", named_duplicate_finalizer_reentry);
#ifdef GIMP_TEST_MYBRUSH_WRAP_PIXBUF_COPY
  g_test_add_func ("/painter-mybrush/named-duplicate/copy-failure-atomic", named_duplicate_copy_failure_atomic);
#endif
  gimp_test_painter_mybrush_duplicate_handle_register ();
  result = g_test_run ();
  gimp_test_utils_set_gimp3_directory ("GIMP_TESTING_ABS_TOP_BUILDDIR", "app/tests/gimpdir-output");
  gimp_exit (gimp, TRUE); return result;
}
