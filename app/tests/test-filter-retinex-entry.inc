/* SPDX-License-Identifier: GPL-3.0-or-later */
static void
c_retinex_preparation_zero (void)
{
  Fixture fixture = fixture_new ();
  GError *error = NULL;
  GArray *saved;
  gboolean prepared = gimp_filter_layer_set_legacy_procedure (
    fixture.filter, "plug-in-retinex", NULL, &error);
  g_assert_no_error (error); g_assert_true (prepared);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  g_assert_nonnull (saved); g_assert_cmpuint (saved->len, ==, 7);
  for (guint i = 0; i < 7; ++i)
    {
      g_assert_cmpuint (G_VALUE_TYPE (argument (saved, i)), ==, i == 6 ? G_TYPE_DOUBLE : G_TYPE_INT);
      if (i == 6) g_assert_cmpfloat (g_value_get_double (argument (saved, i)), ==, 0.0);
      else g_assert_cmpint (g_value_get_int (argument (saved, i)), ==, 0);
    }
  g_array_unref (saved); fixture_clear (&fixture);
}
static GArray *retinex_entry_args (gint scale, gint nscales, gint mode, gdouble cvar)
{
  const gint values[] = { GIMP_RUN_NONINTERACTIVE, 123, 456, scale, nscales, mode };
  GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), 7);
  g_array_set_clear_func (args, legacy_value_clear); g_array_set_size (args, 7);
  for (guint i = 0; i < 6; ++i) g_value_init (argument (args, i), G_TYPE_INT);
  g_value_init (argument (args, 6), G_TYPE_DOUBLE);
  for (guint i = 0; i < 6; ++i) g_value_set_int (argument (args, i), values[i]);
  g_value_set_double (argument (args, 6), cvar); return args;
}
static void retinex_entry_values (GArray *args, gint run, gint scale, gint nscales, gint mode, gdouble cvar)
{
  const gint values[] = { run, 0, 0, scale, nscales, mode };
  g_assert_nonnull (args); g_assert_cmpuint (args->len, ==, 7);
  for (guint i = 0; i < 6; ++i) {
    g_assert_cmpuint (G_VALUE_TYPE (argument (args, i)), ==, G_TYPE_INT);
    g_assert_cmpint (g_value_get_int (argument (args, i)), ==, values[i]);
  }
  g_assert_cmpuint (G_VALUE_TYPE (argument (args, 6)), ==, G_TYPE_DOUBLE);
  g_assert_cmpfloat (g_value_get_double (argument (args, 6)), ==, cvar);
}
static void retinex_entry_rejected (Fixture *fixture, GArray *args)
{
  ReadCheckpoint before = checkpoint_new (fixture); GError *error = NULL;
  g_assert_false (gimp_filter_layer_set_legacy_procedure (fixture->filter, "plug-in-retinex", args, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE);
  g_assert_nonnull (strstr (error->message, "Legacy Retinex"));
  g_clear_error (&error); checkpoint_assert_clear (fixture, &before);
}
static void c_retinex_shapes (void)
{
  Fixture fixture = fixture_new (); const guint counts[] = { 0, 4, 6, 8 };
  GArray *wrong;
  legacy_preserved_definition (&fixture);
  for (guint i = 0; i < G_N_ELEMENTS (counts); ++i) {
    GArray *args = g_array_sized_new (FALSE, TRUE, sizeof (GValue), counts[i]);
    g_array_set_size (args, counts[i]); retinex_entry_rejected (&fixture, args); g_array_unref (args);
  }
  wrong = g_array_sized_new (FALSE, TRUE, sizeof (gint), 7);
  g_array_set_size (wrong, 7); retinex_entry_rejected (&fixture, wrong); g_array_unref (wrong);
  fixture_clear (&fixture);
}
static void c_retinex_owned_conversions (void)
{
  Fixture fixture = fixture_new ();
  const GType types[] = { G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_FLOAT };
  const gdouble doubles[] = { 0.123456789012345, 2147483648.25, -2147483649.75 };
  for (guint i = 0; i < G_N_ELEMENTS (doubles); ++i) {
    GArray *args = retinex_entry_args (16, 3, 2, doubles[i]), *saved;
    g_value_set_enum (legacy_retype (args, 0, GIMP_TYPE_RUN_MODE), GIMP_RUN_NONINTERACTIVE);
    g_value_set_float (legacy_retype (args, 3, G_TYPE_FLOAT), 16.75f);
    g_value_set_boolean (legacy_retype (args, 4, G_TYPE_BOOLEAN), TRUE);
    g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-retinex", args, NULL));
    g_value_set_double (argument (args, 6), 99); g_array_unref (args);
    saved = gimp_filter_layer_get_procedure_args (fixture.filter);
    retinex_entry_values (saved, GIMP_RUN_NONINTERACTIVE, 16, 1, 2, doubles[i]);
    g_value_set_double (argument (saved, 6), 99); g_array_unref (saved);
    saved = gimp_filter_layer_get_procedure_args (fixture.filter);
    retinex_entry_values (saved, GIMP_RUN_NONINTERACTIVE, 16, 1, 2, doubles[i]); g_array_unref (saved);
  }
  for (guint i = 0; i < G_N_ELEMENTS (types); ++i) {
    GArray *args = retinex_entry_args (16, 0, 2, 0), *saved;
    GValue *tail = legacy_retype (args, 6, types[i]);
    if (i == 0) g_value_set_int (tail, -2);
    if (i == 1) g_value_set_boolean (tail, TRUE);
    if (i == 2) g_value_set_float (tail, 0.25f);
    /* GLib intentionally has no boolean-to-double transform. Do not invent one. */
    if (i == 1) { retinex_entry_rejected (&fixture, args); g_array_unref (args); continue; }
    g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-retinex", args, NULL));
    saved = gimp_filter_layer_get_procedure_args (fixture.filter);
    retinex_entry_values (saved, GIMP_RUN_NONINTERACTIVE, 16, 0, 2, i == 0 ? -2 : i == 1 ? 1 : 0.25);
    g_array_unref (saved); g_array_unref (args);
  }
  fixture_clear (&fixture);
}
static void c_retinex_ignored_context (void)
{
  Fixture fixture = fixture_new (); GArray *args = retinex_entry_args (16, 0, 2, 0.125), *saved;
  GObject *context = g_object_new (G_TYPE_OBJECT, NULL), *weak = context;
  g_object_add_weak_pointer (context, (gpointer *) &weak);
  g_value_take_object (legacy_retype (args, 1, G_TYPE_OBJECT), context); legacy_retype (args, 2, G_TYPE_INVALID);
  g_assert_true (gimp_filter_layer_set_legacy_procedure (fixture.filter, "plug-in-retinex", args, NULL));
  g_assert_cmpuint (context->ref_count, ==, 1); g_array_unref (args); g_assert_null (weak);
  saved = gimp_filter_layer_get_procedure_args (fixture.filter);
  retinex_entry_values (saved, GIMP_RUN_NONINTERACTIVE, 16, 0, 2, 0.125);
  g_array_unref (saved); fixture_clear (&fixture);
}
static void c_retinex_invalid_scalars (void)
{
  Fixture fixture = fixture_new (); const gdouble bad[] = { NAN, INFINITY, -INFINITY, 2147483648.0, -2147483649.0 };
  legacy_preserved_definition (&fixture);
  for (guint slot = 0; slot < 7; ++slot) {
    if (slot == 1 || slot == 2) continue;
    for (guint i = 0; i < (slot == 6 ? 3 : G_N_ELEMENTS (bad)); ++i) {
      GArray *args = retinex_entry_args (16, 3, 2, 0.125);
      g_value_set_double (legacy_retype (args, slot, G_TYPE_DOUBLE), bad[i]);
      retinex_entry_rejected (&fixture, args); g_array_unref (args);
    }
    for (guint kind = 0; kind < 4; ++kind) {
      GArray *args = retinex_entry_args (16, 3, 2, 0.125);
      if (kind == 0) legacy_retype (args, slot, G_TYPE_INVALID);
      if (kind == 1) g_value_set_static_string (legacy_retype (args, slot, G_TYPE_STRING), "3");
      if (kind == 2) g_value_set_uint (legacy_retype (args, slot, G_TYPE_UINT), 3);
      if (kind == 3) g_value_set_enum (legacy_retype (args, slot, GIMP_TYPE_ORIENTATION_TYPE), GIMP_ORIENTATION_HORIZONTAL);
      retinex_entry_rejected (&fixture, args); g_array_unref (args);
    }
  }
  fixture_clear (&fixture);
}
static void c_retinex_missing_provider (void)
{
  Fixture fixture = fixture_new (); GimpPlugInManager *manager = gimp->plug_in_manager; GimpPDB *pdb = gimp->pdb;
  legacy_preserved_definition (&fixture); gimp->plug_in_manager = NULL; retinex_entry_rejected (&fixture, NULL);
  gimp->plug_in_manager = manager; gimp->pdb = NULL; retinex_entry_rejected (&fixture, NULL); gimp->pdb = pdb;
  fixture_clear (&fixture);
}
static void retinex_define (GimpFilterLayer *filter, gint scale, gint nscales, gint mode, gdouble cvar)
{
  GArray *args = retinex_entry_args (scale, nscales, mode, cvar); GError *error = NULL;
  g_assert_true (gimp_filter_layer_set_legacy_procedure (filter, "plug-in-retinex", args, &error));
  g_assert_no_error (error); g_array_unref (args);
}
static void retinex_definition_preserved (GimpFilterLayer *filter, gint scale, gint nscales, gint mode, gdouble cvar)
{
  GArray *args = gimp_filter_layer_get_procedure_args (filter);
  g_assert_cmpstr (gimp_filter_layer_get_procedure (filter), ==, "plug-in-retinex");
  g_assert_null (gimp_filter_layer_ref_definition (filter));
  retinex_entry_values (args, GIMP_RUN_NONINTERACTIVE, scale, nscales, mode, cvar); g_array_unref (args);
}
static gchar *
retinex_fixture_directory (void)
{
  const gchar *root = g_getenv ("GIMP_PAINTER_RETINEX_FIXTURES");
  g_assert_nonnull (root);
  return g_build_filename (root,"live",NULL);
}

static guchar *
retinex_fixture (const gchar *directory, const gchar *id, const gchar *label, gsize expected)
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
retinex_actual_old_live (void)
{
  gchar *directory = retinex_fixture_directory ();
  gchar *manifest = g_build_filename (directory,"cases.tsv",NULL);
  gchar *text = NULL, **lines;
  guint compared = 0;
  g_assert_true (g_file_get_contents (manifest,&text,NULL,NULL));
  lines = g_strsplit (text,"\n",-1);
  for (guint row = 0; lines[row]; ++row)
    {
      gchar **cols;
      guint width,height,selection;
      gint scale,nscales,mode;
      gdouble cvar;
      GimpImage *image;
      GimpLayer *source;
      GimpFilterLayer *filter;
      guchar *input,*actual;
      if (!*lines[row]) continue;
      cols = g_strsplit (lines[row],"\t",-1);
      g_assert_cmpuint (g_strv_length (cols), ==, 10);
      g_assert_cmpstr (cols[1], ==, "plug-in-retinex");
      g_assert_cmpint (atoi (cols[4]), ==, 4);
      width = atoi (cols[2]); height = atoi (cols[3]);
      scale = atoi (cols[6]); nscales = atoi (cols[7]); mode = atoi (cols[8]);
      cvar = g_ascii_strtod (cols[9],NULL);
      g_assert_nonnull (strstr (cols[0],"-s"));
      selection = atoi (strstr (cols[0],"-s")+2);
      g_assert_cmpuint (selection, <=, 2);
      image = gimp_image_new (gimp,width,height,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
      source = gimp_layer_new (image,width,height,babl_format ("R'G'B'A u8"),"Old Retinex source",1,GIMP_LAYER_MODE_NORMAL_LEGACY);
      gimp_image_add_layer (image,source,NULL,0,FALSE);
      input = retinex_fixture (directory,cols[0],"source",width*height*4);
      actual = g_malloc (width*height*4);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)),NULL,0,
                       babl_format ("R'G'B'A u8"),input,GEGL_AUTO_ROWSTRIDE);
      if (selection)
        {
          GeglBuffer *coverage = gegl_buffer_new (GEGL_RECTANGLE (0,0,width,height),babl_format ("Y u8"));
          guchar *mask = retinex_fixture (directory,cols[0],"merge1-mask",width*height);
          gegl_buffer_set (coverage,NULL,0,babl_format ("Y u8"),mask,GEGL_AUTO_ROWSTRIDE);
          gimp_channel_select_buffer (gimp_image_get_mask (image),"Old Retinex selection",coverage,0,0,
                                      GIMP_CHANNEL_OP_REPLACE,FALSE,0,0);
          g_free (mask); g_object_unref (coverage);
        }
      filter = GIMP_FILTER_LAYER (gimp_filter_layer_new (image,width,height,"Old Retinex fixture",1,GIMP_LAYER_MODE_NORMAL_LEGACY));
      gimp_image_add_layer (image,GIMP_LAYER (filter),NULL,0,FALSE);
      retinex_define (filter,scale,nscales,mode,cvar);
      for (guint pass = 0; pass < 2; ++pass)
        {
          guchar *expected;
          if (pass) gimp_drawable_update (GIMP_DRAWABLE (source),0,0,width,height);
          legacy_wait (filter, GIMP_FILTER_LAYER_CLEAN);
          expected = retinex_fixture (directory,cols[0],pass ? "rerun" : "settled",width*height*4);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (filter)),NULL,1,
                           babl_format ("R'G'B'A u8"),actual,GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);
          g_test_message ("Actual old Retinex Filter scene %s pass%u",cols[0],pass+1);
          g_assert_cmpmem (actual,width*height*4,expected,width*height*4);
          g_assert_cmpuint (gimp_filter_layer_get_run_count (filter), ==, pass+1);
          g_free (expected); ++compared;
        }
      retinex_definition_preserved (filter,scale,nscales,mode,cvar);
      g_free (input); g_free (actual); g_object_unref (image); g_strfreev (cols);
    }
  g_assert_cmpuint (compared, ==, 12);
  g_strfreev (lines); g_free (text); g_free (manifest); g_free (directory);
}

static void
register_retinex_entry_tests (void)
{
  g_test_add_func ("/filter-entry-contracts/retinex/zero", c_retinex_preparation_zero);
  g_test_add_func ("/filter-entry-contracts/retinex/shapes", c_retinex_shapes);
  g_test_add_func ("/filter-entry-contracts/retinex/owned-conversions", c_retinex_owned_conversions);
  g_test_add_func ("/filter-entry-contracts/retinex/ignored-context", c_retinex_ignored_context);
  g_test_add_func ("/filter-entry-contracts/retinex/invalid-scalars", c_retinex_invalid_scalars);
  g_test_add_func ("/filter-entry-contracts/retinex/missing-provider", c_retinex_missing_provider);
  g_test_add_func ("/filter-entry-contracts/retinex/actual-old-live", retinex_actual_old_live);
}
