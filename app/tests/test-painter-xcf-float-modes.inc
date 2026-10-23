/* SPDX-License-Identifier: GPL-3.0-or-later
 * Float-mode transport proof, not the independent arithmetic oracle. */
#include "operations/layer-modes/gimp-layer-modes.h"
#include "operations/layer-modes-legacy/gimpoperationpainterlegacy.h"
static void float_mode_projection (GimpImage *image, gfloat pixels[256])
{
  gimp_pickable_flush (GIMP_PICKABLE (image));
  gegl_buffer_get (gimp_pickable_get_buffer (GIMP_PICKABLE (image)),
                   GEGL_RECTANGLE (0, 0, 8, 8), 1, babl_format ("R'G'B'A float"),
                   pixels, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
}
static void float_mode_standard_wire (GFile *file)
{
  gchar *contents = NULL; gsize length; GError *error = NULL;
  g_assert_true (g_file_load_contents (file, NULL, &contents, &length, NULL, &error));
  g_assert_no_error (error);
  const guint8 *raw = (const guint8 *) contents;
  guint version = (raw[10]-'0')*100 + (raw[11]-'0')*10 + raw[12]-'0';
  guint n = version >= 11 ? 8 : 4;
  gsize table = order_after_props (raw, length, version >= 4 ? 30 : 26);
  guint fallbacks = 0;
  while (order_offset (raw + table, n))
    {
      gsize layer_start = order_offset (raw + table, n);
      gsize begin = layer_start + 16 + order_u32 (raw + layer_start + 12);
      gsize end = order_after_props (raw, length, begin);
      for (gsize at = begin; at < end; at += 8 + order_u32 (raw + at + 4))
        if (order_u32 (raw + at) == 7)
          {
            guint32 mode = order_u32 (raw + at + 8);
            g_assert_true (mode < 64); /* No fork-specific runtime enum on wire. */
            if (mode == GIMP_LAYER_MODE_NORMAL_LEGACY) fallbacks++;
          }
      table += n;
    }
  g_assert_cmpuint (fallbacks, >, 0);
  g_free (contents);
}
static void fields_float_mode_transport (void)
{
  const GimpLayerMode modes[] = {
    GIMP_LAYER_MODE_PAINTER_FLOAT_ERASE, GIMP_LAYER_MODE_PAINTER_FLOAT_REPLACE,
    GIMP_LAYER_MODE_PAINTER_FLOAT_ANTI_ERASE, GIMP_LAYER_MODE_PAINTER_FLOAT_SRC_IN,
    GIMP_LAYER_MODE_PAINTER_FLOAT_DST_IN, GIMP_LAYER_MODE_PAINTER_FLOAT_SRC_OUT,
    GIMP_LAYER_MODE_PAINTER_FLOAT_DST_OUT, GIMP_LAYER_MODE_PAINTER_FLOAT_NORMAL,
    GIMP_LAYER_MODE_PAINTER_FLOAT_MULTIPLY
  };
  const Babl *format = babl_format ("R'G'B'A float");
  const GeglRectangle rect = {0, 0, 8, 8};
  gfloat backdrop[256], source[256], mask_pixels[64];
  for (guint p = 0; p < 64; p++)
    {
      backdrop[4*p] = -.37f + p*.00113f; backdrop[4*p+1] = 1.71f;
      backdrop[4*p+2] = .314159f; backdrop[4*p+3] = .713f;
      source[4*p] = 2.17f; source[4*p+1] = -.213f;
      source[4*p+2] = .271828f+p*.00071f; source[4*p+3] = .437f;
      mask_pixels[p] = .217f+p*.0031f;
    }
  for (guint kind = 0; kind < 3; kind++)
    for (guint k = 0; k < G_N_ELEMENTS (modes); k++)
      {
        GimpImage *image = gimp_image_new (gimp, 8, 8, GIMP_RGB,
                                           GIMP_PRECISION_FLOAT_NON_LINEAR);
        GFile *file = temporary_file ();
        GimpLayer *base = gimp_layer_new (image, 8, 8, format, "float backdrop",
                                         1, GIMP_LAYER_MODE_NORMAL);
        GimpLayer *target;
        gimp_image_add_layer (image, base, NULL, 0, FALSE);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (base)), &rect,
                         0, format, backdrop, GEGL_AUTO_ROWSTRIDE);
        if (kind == 1)
          target = gimp_clone_layer_new (image, base, 8, 8, "float target", .375,
                                         GIMP_LAYER_MODE_PAINTER_REPLACE);
        else if (kind == 2)
          {
            target = gimp_group_layer_new (image);
            gimp_object_set_name (GIMP_OBJECT (target), "float target");
            gimp_layer_set_mode (target, GIMP_LAYER_MODE_PAINTER_REPLACE, FALSE);
            gimp_layer_set_opacity (target, .375, FALSE);
          }
        else
          target = gimp_layer_new (image, 8, 8, format, "float target", .375,
                                   GIMP_LAYER_MODE_PAINTER_REPLACE);
        gimp_image_add_layer (image, target, NULL, 0, FALSE);
        if (kind != 1)
          {
            GimpLayer *paint = target;
            if (kind == 2)
              {
                paint = gimp_layer_new (image, 8, 8, format, "float child", 1,
                                        GIMP_LAYER_MODE_NORMAL);
                gimp_image_add_layer (image, paint, target, 0, FALSE);
              }
            gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (paint)), &rect,
                             0, format, source, GEGL_AUTO_ROWSTRIDE);
            gimp_drawable_update (GIMP_DRAWABLE (paint), 0, 0, 8, 8);
          }
        GimpLayerMask *mask = gimp_layer_create_mask (target, GIMP_ADD_MASK_WHITE, NULL);
        g_assert_nonnull (mask);
        gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (mask)), &rect,
                         0, babl_format ("Y float"), mask_pixels, GEGL_AUTO_ROWSTRIDE);
        g_assert_nonnull (gimp_layer_add_mask (target, mask, FALSE, NULL));
        gimp_drawable_update (GIMP_DRAWABLE (base), 0, 0, 8, 8);
        save (image, file);
        GimpImage *copy = load (file); g_object_unref (image); image = copy;
        target = layer (image, "float target"); g_assert_nonnull (target);
        GVariant *dict = capsule (GIMP_ITEM (target)); guint32 raw;
        g_assert_true (g_variant_lookup (dict, "legacy-mode", "u", &raw));
        g_assert_cmpuint (raw, ==, 24); g_variant_unref (dict);
        gimp_layer_set_mode (target, modes[k], FALSE);
        gchar *reason = NULL;
        gint version = gimp_image_get_xcf_version (image, FALSE, NULL, NULL, &reason);
        g_assert_nonnull (reason); g_assert_nonnull (strstr (reason, "gimp-painter"));
        g_free (reason);
        gimp_layer_set_mode (target, GIMP_LAYER_MODE_NORMAL_LEGACY, FALSE);
        g_assert_cmpint (gimp_image_get_xcf_version (image, FALSE, NULL, NULL, NULL), ==, version);
        gimp_layer_set_mode (target, modes[k], FALSE);
        g_assert_cmpint (gimp_layer_get_mode (target), ==, modes[k]);
        g_assert_false (gimp_painter_layer_mode_to_legacy (modes[k], &raw));
        g_assert_cmpint (xcf_painter_standard_mode (modes[k]), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
        gfloat expected[256]; float_mode_projection (image, expected);
        for (guint cycle = 0; cycle < 2; cycle++)
          {
            save (image, file); float_mode_standard_wire (file); copy = load (file); g_object_unref (image); image = copy;
            target = layer (image, "float target"); g_assert_nonnull (target);
            g_assert_cmpint (gimp_layer_get_mode (target), ==, modes[k]);
            g_assert_cmpfloat (gimp_layer_get_opacity (target), ==, .375);
            g_assert_cmpint (gimp_image_get_precision (image), ==, GIMP_PRECISION_FLOAT_NON_LINEAR);
            dict = capsule (GIMP_ITEM (target));
            GVariant *legacy = g_variant_lookup_value (dict, "legacy-mode", NULL);
            g_assert_null (legacy);
            guint32 capsule_version, policy;
            const gchar *space, *mode_name;
            g_assert_true (g_variant_lookup (dict, "version", "u", &capsule_version));
            g_assert_cmpuint (capsule_version, ==, 2);
            g_assert_true (g_variant_lookup (dict, "float-mode", "(u&s&s)", &policy, &space, &mode_name));
            g_assert_cmpuint (policy, ==, 1);
            g_assert_cmpstr (space, ==, "nonlinear-rgb");
            const gchar *names[] = {"erase", "replace", "anti-erase", "src-in", "dst-in", "src-out", "dst-out", "normal", "multiply"};
            g_assert_cmpstr (mode_name, ==, names[k]);
            g_variant_unref (dict);
            g_assert_cmpint (GIMP_IS_CLONE_LAYER (target), ==, kind == 1);
            g_assert_cmpint (GIMP_IS_GROUP_LAYER (target), ==, kind == 2);
            if (kind == 1)
              g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (target)) ==
                             layer (image, "float backdrop"));
            if (kind == 2)
              g_assert_true (gimp_item_get_parent (GIMP_ITEM (layer (image, "float child"))) ==
                             GIMP_ITEM (target));
            gfloat actual[256], actual_mask[64]; float_mode_projection (image, actual);
            for (guint i = 0; i < 256; i++)
              g_assert_cmpfloat_with_epsilon (actual[i], expected[i], 2e-6f);
            mask = gimp_layer_get_mask (target); g_assert_nonnull (mask);
            gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (mask)), &rect,
                             1, babl_format ("Y float"), actual_mask,
                             GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
            g_assert_cmpmem (actual_mask, sizeof actual_mask, mask_pixels, sizeof mask_pixels);
            const gchar *capture = g_getenv ("GIMP_TEST_FLOAT_XCF_CAPTURE");
            if (capture && cycle == 1)
              {
                g_assert_cmpint (g_mkdir_with_parents (capture, 0700), ==, 0);
                gchar *name = g_strdup_printf ("kind%u-mode%u.xcf", kind, k);
                gchar *filename = g_build_filename (capture, name, NULL);
                GFile *destination = g_file_new_for_path (filename); GError *error = NULL;
                g_assert_true (g_file_copy (file, destination, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error));
                g_assert_no_error (error); g_object_unref (destination);
                const GimpParasite *parasite = gimp_item_parasite_find (GIMP_ITEM (target), "gimp-painter-item");
                guint32 size; const gchar *bytes = gimp_parasite_get_data (parasite, &size);
                gchar *capsule_path = g_strconcat (filename, ".capsule", NULL);
                g_assert_true (g_file_set_contents (capsule_path, bytes, size, &error));
                g_assert_no_error (error); g_free (capsule_path); g_free (filename); g_free (name);
              }
            g_test_message ("float XCF kind%u mode%d round%u: identity, no old tag, projection and mask", kind, modes[k], cycle);
          }
        g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
      }
}

static GBytes *float_mode_schema (guint scene)
{
  const guint8 magic[12] = {'G','P','X','C','F',0,0,0,1,0,0,0};
  GVariantBuilder builder;
  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "version",
                         g_variant_new_uint32 (scene == 1 ? 3 : scene == 7 ? 1 : 2));
  g_variant_builder_add (&builder, "{sv}", "kind",
                         g_variant_new_string (scene == 9 ? "future-kind" : "ordinary"));
  g_variant_builder_add (&builder, "{sv}", "id", g_variant_new_uint32 (2));
  if (scene != 8)
    g_variant_builder_add (&builder, "{sv}", "float-mode", scene == 5
      ? g_variant_new_uint32 (73)
      : g_variant_new ("(uss)", scene == 2 ? 2u : 1u,
                        scene == 3 ? "unknown-space" : "nonlinear-rgb",
                        scene == 4 ? "unknown-mode" : "normal"));
  if (scene == 6)
    g_variant_builder_add (&builder, "{sv}", "legacy-mode", g_variant_new_uint32 (24));
  g_variant_builder_add (&builder, "{sv}", "future-independent-field",
                         g_variant_new_string ("retain this unknown field"));
  GVariant *value = g_variant_ref_sink (g_variant_builder_end (&builder));
#if G_BYTE_ORDER == G_BIG_ENDIAN
  GVariant *swapped = g_variant_byteswap (value); g_variant_unref (value); value = swapped;
#endif
  GByteArray *wire = g_byte_array_new ();
  g_byte_array_append (wire, magic, sizeof magic);
  g_byte_array_append (wire, g_variant_get_data (value), g_variant_get_size (value));
  g_variant_unref (value);
  return g_byte_array_free_to_bytes (wire);
}
static void fields_float_mode_schema (void)
{
  for (guint scene = 0; scene < 10; scene++)
    {
      GBytes *expected = float_mode_schema (scene);
      GFile *input = synthetic_capsule_file (expected), *output = temporary_file ();
      GimpImage *image = load (input);
      for (guint cycle = 0; cycle < 2; cycle++)
        {
          GimpLayer *item = layer (image, "L"); g_assert_nonnull (item);
          if (scene)
            {
              assert_opaque_capsule (item, expected);
              g_assert_cmpint (gimp_layer_get_mode (item), ==, GIMP_LAYER_MODE_NORMAL);
            }
          else
            {
              g_assert_cmpint (gimp_layer_get_mode (item), ==, GIMP_LAYER_MODE_PAINTER_FLOAT_NORMAL);
              GVariant *record = capsule (GIMP_ITEM (item)); const gchar *unknown;
              g_assert_true (g_variant_lookup (record, "future-independent-field", "&s", &unknown));
              g_assert_cmpstr (unknown, ==, "retain this unknown field");
              g_variant_unref (record);
            }
          save (image, output);
          GimpImage *copy = load (output); g_object_unref (image); image = copy;
        }
      GimpLayer *item = layer (image, "L");
      if (scene)
        {
          GError *error = NULL;
          GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
          gimp_layer_set_mode (item, GIMP_LAYER_MODE_PAINTER_FLOAT_REPLACE, FALSE);
          sentinel_write (output);
          g_assert_cmpint (file_save (gimp, image, NULL, output, proc, GIMP_RUN_NONINTERACTIVE,
                                     FALSE, FALSE, FALSE, &error), ==, GIMP_PDB_EXECUTION_ERROR);
          g_assert_nonnull (error);
          g_assert_nonnull (strstr (error->message, "cannot be overwritten"));
          g_clear_error (&error); sentinel_check (output);
          assert_opaque_capsule (item, expected);
        }
      else
        {
          const GimpLayerMode destinations[] = {GIMP_LAYER_MODE_PAINTER_REPLACE, GIMP_LAYER_MODE_NORMAL};
          for (guint n = 0; n < G_N_ELEMENTS (destinations); n++)
            {
              item = layer (image, "L");
              gimp_layer_set_mode (item, destinations[n], FALSE); save (image, output);
              GimpImage *copy = load (output); g_object_unref (image); image = copy;
              item = layer (image, "L"); g_assert_cmpint (gimp_layer_get_mode (item), ==, destinations[n]);
              GVariant *record = capsule (GIMP_ITEM (item)); guint32 version; const gchar *unknown;
              g_assert_true (g_variant_lookup (record, "version", "u", &version));
              g_assert_cmpuint (version, ==, 1);
              g_assert_null (g_variant_lookup_value (record, "float-mode", NULL));
              g_assert_true (g_variant_lookup (record, "future-independent-field", "&s", &unknown));
              g_assert_cmpstr (unknown, ==, "retain this unknown field");
              GVariant *legacy = g_variant_lookup_value (record, "legacy-mode", NULL);
              g_assert_cmpint (legacy != NULL, ==, n == 0);
              if (legacy) g_variant_unref (legacy);
              g_variant_unref (record);
            }
        }
      g_test_message ("float schema scene%u: %s survives native resave", scene,
                       scene ? "opaque exact bytes" : "known identity and unknown field");
      g_object_unref (image); g_file_delete (input, NULL, NULL); g_object_unref (input);
      g_file_delete (output, NULL, NULL); g_object_unref (output); g_bytes_unref (expected);
    }
}
