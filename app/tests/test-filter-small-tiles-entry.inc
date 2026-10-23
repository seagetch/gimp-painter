#include <stdlib.h>
#include "core/gimpchannel.h"
#include "core/gimpchannel-select.h"
/* SPDX-License-Identifier: GPL-3.0-or-later */
static void
c_small_tiles_preparation_zero (void)
{
  Fixture fixture = fixture_new ();
  GError *error = NULL;
  GArray *saved;
  gboolean prepared = gimp_filter_layer_set_legacy_procedure (
    fixture.filter, "plug-in-small-tiles", NULL, &error);
  g_assert_no_error (error);
  g_assert_true (prepared);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  g_assert_nonnull (saved);
  g_assert_cmpuint (saved->len, ==, 4);
  for (guint i = 0; i < 4; ++i)
    {
      g_assert_cmpuint (G_VALUE_TYPE (argument (saved, i)), ==, G_TYPE_INT);
      g_assert_cmpint (g_value_get_int (argument (saved, i)), ==, 0);
    }
  g_array_unref (saved);
  fixture_clear (&fixture);
}
static GArray *tiles_args (gint factor)
{
  GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), 4);
  g_array_set_clear_func (args, legacy_value_clear); g_array_set_size (args, 4);
  for (guint i = 0; i < 4; ++i) g_value_init (argument (args, i), G_TYPE_INT);
  g_value_set_int (argument (args, 0), GIMP_RUN_NONINTERACTIVE);
  g_value_set_int (argument (args, 1), 123); g_value_set_int (argument (args, 2), 456);
  g_value_set_int (argument (args, 3), factor); return args;
}
static void tiles_values (GArray *args, gint mode, gint factor)
{
  const gint expected[] = { mode, 0, 0, factor };
  g_assert_nonnull (args); g_assert_cmpuint (args->len, ==, 4);
  for (guint i = 0; i < 4; ++i) {
    g_assert_cmpuint (G_VALUE_TYPE (argument (args, i)), ==, G_TYPE_INT);
    g_assert_cmpint (g_value_get_int (argument (args, i)), ==, expected[i]);
  }
}
static void tiles_rejected (Fixture *fixture, GArray *args)
{
  ReadCheckpoint before = checkpoint_new (fixture); GError *error = NULL;
  g_assert_false (gimp_filter_layer_set_legacy_procedure (fixture->filter, "plug-in-small-tiles", args, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_assert_nonnull (strstr (error->message, "Legacy Small Tiles"));
  g_clear_error (&error); checkpoint_assert_clear (fixture, &before);
}
static void c_tiles_shapes (void)
{
  Fixture fixture = fixture_new (); const guint counts[] = { 0, 3, 5, 7 };
  GArray *wrong;
  legacy_preserved_definition (&fixture);
  for (guint i = 0; i < G_N_ELEMENTS (counts); ++i) {
    GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), counts[i]);
    g_array_set_size (args, counts[i]); tiles_rejected (&fixture, args); g_array_unref (args);
  }
  wrong = g_array_sized_new (FALSE, TRUE, sizeof (gint), 4);
  g_array_set_size (wrong, 4); tiles_rejected (&fixture, wrong); g_array_unref (wrong);
  fixture_clear (&fixture);
}
static void c_tiles_owned_conversions (void)
{
  Fixture fixture = fixture_new (); GArray *args = tiles_args (3), *saved;
  g_value_set_enum (legacy_retype (args, 0, GIMP_TYPE_RUN_MODE), GIMP_RUN_NONINTERACTIVE);
  g_value_set_double (legacy_retype (args, 3, G_TYPE_DOUBLE), 3.75);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-small-tiles", args, NULL));
  g_value_set_double (argument (args, 3), 99); g_array_unref (args);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter); tiles_values (saved, GIMP_RUN_NONINTERACTIVE, 3);
  g_value_set_int (argument (saved, 3), 99); g_array_unref (saved);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter); tiles_values (saved, GIMP_RUN_NONINTERACTIVE, 3); g_array_unref (saved);
  args = tiles_args (1); g_value_set_boolean (legacy_retype (args, 0, G_TYPE_BOOLEAN), TRUE);
  g_value_set_float (legacy_retype (args, 3, G_TYPE_FLOAT), -2.75f);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-small-tiles", args, NULL));
  saved = gimp_filter_layer_get_procedure_args (fixture.filter); tiles_values (saved, 1, -2); g_array_unref (saved);
  g_array_unref (args); fixture_clear (&fixture);
}
static void c_tiles_ignored_context (void)
{
  Fixture fixture = fixture_new (); GArray *args = tiles_args (3), *saved;
  GObject *context = g_object_new (G_TYPE_OBJECT, NULL), *weak = context;
  g_object_add_weak_pointer (context, (gpointer *) &weak);
  g_value_take_object (legacy_retype (args, 1, G_TYPE_OBJECT), context);
  legacy_retype (args, 2, G_TYPE_INVALID);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-small-tiles", args, NULL));
  g_assert_cmpuint (context->ref_count, ==, 1);
  g_array_unref (args); g_assert_null (weak);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter); tiles_values (saved, GIMP_RUN_NONINTERACTIVE, 3);
  g_array_unref (saved); fixture_clear (&fixture);
}
static void c_tiles_invalid_scalars (void)
{
  Fixture fixture = fixture_new (); const gdouble bad[] = { NAN, INFINITY, -INFINITY, 2147483648.0, -2147483649.0 };
  legacy_preserved_definition (&fixture);
  for (guint slot = 0; slot < 4; slot += 3) {
    for (guint i = 0; i < G_N_ELEMENTS (bad); ++i) {
      GArray *args = tiles_args (3); g_value_set_double (legacy_retype (args, slot, G_TYPE_DOUBLE), bad[i]);
      tiles_rejected (&fixture, args); g_array_unref (args);
    }
    for (guint kind = 0; kind < 4; ++kind) {
      GArray *args = tiles_args (3);
      if (kind == 0) legacy_retype (args, slot, G_TYPE_INVALID);
      if (kind == 1) g_value_set_static_string (legacy_retype (args, slot, G_TYPE_STRING), "3");
      if (kind == 2) g_value_set_uint (legacy_retype (args, slot, G_TYPE_UINT), 3);
      if (kind == 3) g_value_set_enum (legacy_retype (args, slot, GIMP_TYPE_ORIENTATION_TYPE), GIMP_ORIENTATION_HORIZONTAL);
      tiles_rejected (&fixture, args); g_array_unref (args);
    }
  }
  fixture_clear (&fixture);
}
static void c_tiles_missing_provider (void)
{
  Fixture fixture = fixture_new (); GimpPlugInManager *manager = gimp->plug_in_manager; GimpPDB *pdb = gimp->pdb;
  legacy_preserved_definition (&fixture); gimp->plug_in_manager = NULL; tiles_rejected (&fixture, NULL);
  gimp->plug_in_manager = manager; gimp->pdb = NULL; tiles_rejected (&fixture, NULL); gimp->pdb = pdb;
  fixture_clear (&fixture);
}
static void tiles_define (GimpFilterLayer *filter, gint factor)
{
  GArray *args = tiles_args (factor); GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_legacy_procedure (filter, "plug-in-small-tiles", args, &error));
  g_assert_no_error (error); g_array_unref (args);
}
static void tiles_definition_preserved (GimpFilterLayer *filter, gint factor)
{
  GArray *args = gimp_filter_layer_get_procedure_args (filter); GBytes *raw = gimp_filter_layer_ref_definition (filter);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (filter), ==, "plug-in-small-tiles");
  g_assert_null (raw); tiles_values (args, GIMP_RUN_NONINTERACTIVE, factor); g_array_unref (args);
}

/* Same retained genuine-old 25-scene/50-output oracle as native Small Tiles;
 * only definition installation now goes through the checked C entry. */
static guchar *
small_tiles_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize expected)
{
  gchar *name = g_strdup_printf ("%s-%s.raw",id,label);
  gchar *path = g_build_filename (directory,name,NULL);
  gchar *bytes = NULL;
  gsize size = 0;
  GError *error = NULL;
  g_assert_true (g_file_get_contents (path,&bytes,&size,&error));
  g_assert_no_error (error); g_assert_cmpuint (size, ==, expected);
  g_free (path); g_free (name); return (guchar *) bytes;
}
static void
c_tiles_actual_old_live (void)
{
  const gchar *root = g_getenv ("GIMP_PAINTER_SMALL_TILES_FIXTURES");
  gchar *directory;
  gchar *manifest;
  gchar *text = NULL, **lines;
  guint compared = 0;
  g_assert_nonnull (root);
  directory = g_build_filename (root,"live",NULL);
  manifest = g_build_filename (directory,"cases.tsv",NULL);
  g_assert_true (g_file_get_contents (manifest,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,channels,selection;
      gint tiles;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      const Babl *format;
      guchar *input,*actual;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1); g_assert_cmpuint (g_strv_length (cols), ==, 10);
      width = atoi (cols[2]); height = atoi (cols[3]); channels = atoi (cols[4]);
      tiles = atoi (cols[5]);
      g_assert_cmpstr (cols[1], ==, "plug-in-small-tiles");
      g_assert_true (channels == 2 || channels == 4);
      selection = atoi (strstr (cols[0],"-s")+2);
      image = gimp_image_new (gimp,selection == 3 ? 63 : width,selection == 3 ? 63 : height,
                             channels == 2 ? GIMP_GRAY : GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      format = babl_format (channels == 2 ? "Y'A u8" : "R'G'B'A u8");
      source = gimp_layer_new (image,width,height,format,"Old fixture lower source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE);
      input = small_tiles_fixture (directory,cols[0],"source",width*height*channels);
      actual = g_malloc (width*height*channels);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
      if (selection == 3)
        gimp_channel_select_rectangle (gimp_image_get_mask (image),62,62,1,1,GIMP_CHANNEL_OP_REPLACE,FALSE,0,0,FALSE);
      else if (selection)
        {
          GeglBuffer *coverage = gegl_buffer_new (GEGL_RECTANGLE (0,0,width,height),babl_format ("Y u8"));
          guchar *mask = small_tiles_fixture (directory,cols[0],"merge1-mask",width*height);
          gegl_buffer_set (coverage,NULL,0,babl_format ("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
          gimp_channel_select_buffer (gimp_image_get_mask (image),"Old fixture selection",coverage,0,0,
                                      GIMP_CHANNEL_OP_REPLACE,FALSE,0,0);
          g_free (mask); g_object_unref (coverage);
        }
      filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,width,height,"Old SmallTiles fixture",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
      gimp_image_add_layer (image,GIMP_LAYER (filter),NULL,0,FALSE);
      tiles_define (filter,tiles);
      for (guint pass = 0; pass < 2; ++pass)
        {
          guchar *expected;
          if (pass)
            {
              if (selection == 3)
                {
                  g_free (input); input = small_tiles_fixture (directory,cols[0],"source2",width*height*channels);
                  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,format,input,GEGL_AUTO_ROWSTRIDE);
                }
              gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
            }
          legacy_wait (filter, GIMP_FILTER_LAYER_CLEAN);
          expected = small_tiles_fixture (directory,cols[0],pass ? "rerun" : "settled",width*height*channels);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,format,actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_test_message ("Actual old SmallTiles Filter scene %s pass%u",cols[0],pass+1);
          g_assert_cmpmem (actual,width*height*channels,expected,width*height*channels);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          g_free (expected); ++compared;
        }
      tiles_definition_preserved (filter,tiles);
      g_free (input); g_free (actual); g_object_unref (image); g_strfreev (cols);
    }
  g_assert_cmpuint (compared, ==, 50);
  g_strfreev (lines); g_free (text); g_free (manifest); g_free (directory);
}
static void
register_small_tiles_entry_tests (void)
{
  g_test_add_func ("/filter-entry-contracts/small-tiles/zero", c_small_tiles_preparation_zero);
  g_test_add_func ("/filter-entry-contracts/small-tiles/shapes", c_tiles_shapes);
  g_test_add_func ("/filter-entry-contracts/small-tiles/owned-conversions", c_tiles_owned_conversions);
  g_test_add_func ("/filter-entry-contracts/small-tiles/ignored-context", c_tiles_ignored_context);
  g_test_add_func ("/filter-entry-contracts/small-tiles/invalid-scalars", c_tiles_invalid_scalars);
  g_test_add_func ("/filter-entry-contracts/small-tiles/missing-provider", c_tiles_missing_provider);
  g_test_add_func ("/filter-entry-contracts/small-tiles/actual-old-live", c_tiles_actual_old_live);
}
