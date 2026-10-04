/* SPDX-License-Identifier: GPL-3.0-or-later
 * Field-level application assertions, included by the real XCF roundtrip harness.
 * Generated modern scenes are deliberately distinct from genuine old-writer fixtures.
 */
#include "libgimpcolor/gimpcolor.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/gimpchannel.h"
#include "core/gimpcontext.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdrawable-filters.h"
#include "core/gimpdrawablefilter.h"
#include "core/gimpfilter.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpimage-colormap.h"
#include "core/gimpimage-color-profile.h"
#include "core/gimpimage-metadata.h"
#include "core/gimpimage-symmetry.h"
#include "core/gimpimage-sample-points.h"
#include "core/gimpsamplepoint.h"
#include "core/gimpsymmetry-mirror.h"
#include "vectors/gimpstroke.h"
#include "vectors/gimpanchor.h"
#include "core/gimpitemlist.h"
#include "core/gimplayermask.h"
#include "core/gimpunit.h"
#include "text/text-types.h"
#include "text/gimptext.h"
#include "text/gimptextlayer.h"

static GimpImage *fields_image (void)
{ return gimp_image_new (gimp, 96, 80, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR); }

static void fields_parasite (GimpItem *item)
{
  static const guint8 bytes[] = {0, 255, 31, 0, 129, 73};
  GimpParasite *p = gimp_parasite_new ("field-binary", GIMP_PARASITE_PERSISTENT,
                                     sizeof bytes, bytes);
  gimp_item_parasite_attach (item, p, FALSE); gimp_parasite_free (p);
}

static void fields_assert_parasite (GimpItem *item)
{
  static const guint8 bytes[] = {0, 255, 31, 0, 129, 73};
  const GimpParasite *p = gimp_item_parasite_find (item, "field-binary");
  guint32 size;
  const void *data;
  g_assert_nonnull (p); data = gimp_parasite_get_data (p, &size);
  g_assert_cmpmem (data, size, bytes, sizeof bytes);
  g_assert_cmpuint (gimp_parasite_get_flags (p), ==, GIMP_PARASITE_PERSISTENT);
}

static void fields_attributes (GimpItem *item)
{
  gimp_item_set_visible (item, FALSE, FALSE);
  gimp_item_set_color_tag (item, GIMP_COLOR_TAG_VIOLET, FALSE);
  if (gimp_item_can_lock_content (item)) gimp_item_set_lock_content (item, TRUE, FALSE);
  if (gimp_item_can_lock_position (item)) gimp_item_set_lock_position (item, TRUE, FALSE);
  if (gimp_item_can_lock_visibility (item)) gimp_item_set_lock_visibility (item, TRUE, FALSE);
  fields_parasite (item);
}

static void fields_assert_attributes (GimpItem *item)
{
  g_test_message ("Checking attributes for %s (%s)", gimp_object_get_name (item), G_OBJECT_TYPE_NAME (item));
  g_assert_false (gimp_item_get_visible (item));
  g_assert_cmpint (gimp_item_get_color_tag (item), ==, GIMP_COLOR_TAG_VIOLET);
  if (gimp_item_can_lock_content (item)) g_assert_true (gimp_item_get_lock_content (item));
  if (gimp_item_can_lock_position (item)) g_assert_true (gimp_item_get_lock_position (item));
  if (gimp_item_can_lock_visibility (item)) g_assert_true (gimp_item_get_lock_visibility (item));
  fields_assert_parasite (item);
}

static void fields_all_owner_attributes (void)
{
  GimpImage *image = fields_image ();
  GFile *file = temporary_file ();
  GimpLayer *ordinary = gimp_layer_new (image, 20, 18, gimp_image_get_layer_format (image, TRUE),
                                       "field ordinary", .625, GIMP_LAYER_MODE_MULTIPLY);
  GimpLayer *group = gimp_group_layer_new (image);
  GimpLayer *clone = gimp_clone_layer_new (image, ordinary, 20, 18, "field clone", .375, GIMP_LAYER_MODE_NORMAL);
  GimpLayer *filter = gimp_filter_layer_new (image, 20, 18, "field filter", .875, GIMP_LAYER_MODE_NORMAL);
  GimpChannel *channel;
  GeglColor *color = gegl_color_new (NULL);
  GimpPath *path_item = gimp_path_new (image, "field path");
  GimpLayerMask *mask;
  GList *members = NULL;
  GimpItemList *set;
  GimpTattoo tattoos[7];
  gimp_image_add_layer (image, ordinary, NULL, 0, FALSE);
  gimp_image_add_layer (image, group, NULL, 0, FALSE); gimp_object_set_name (GIMP_OBJECT (group), "field group");
  gimp_viewable_set_expanded (GIMP_VIEWABLE (group), TRUE);
  gimp_image_add_layer (image, clone, NULL, 0, FALSE);
  gimp_image_add_layer (image, filter, NULL, 0, FALSE);
  gimp_item_set_offset (GIMP_ITEM (ordinary), -3, 7);
  gimp_layer_set_lock_alpha (ordinary, TRUE, FALSE);
  gimp_layer_set_blend_space (ordinary, GIMP_LAYER_COLOR_SPACE_RGB_LINEAR, FALSE);
  gimp_layer_set_composite_space (ordinary, GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR, FALSE);
  gimp_layer_set_composite_mode (ordinary, GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP, FALSE);
  mask = gimp_layer_create_mask (ordinary, GIMP_ADD_MASK_WHITE, NULL);
  gimp_layer_add_mask (ordinary, mask, FALSE, NULL);
  gimp_layer_set_apply_mask (ordinary, FALSE, FALSE);
  gimp_layer_set_edit_mask (ordinary, TRUE);
  gimp_layer_set_show_mask (ordinary, TRUE, FALSE);
  { const gdouble rgba[] = {.1, .3, .7, .45};
    gegl_color_set_pixel (color, babl_format ("R'G'B'A double"), rgba); }
  channel = gimp_channel_new (image, 96, 80, "field channel", color); g_object_unref (color);
  gimp_image_add_channel (image, channel, NULL, 0, FALSE);
  gimp_channel_set_show_masked (channel, TRUE);
  gimp_image_add_path (image, path_item, NULL, 0, FALSE);
  {
    GimpItem *items[] = {GIMP_ITEM (ordinary), GIMP_ITEM (group), GIMP_ITEM (clone),
                         GIMP_ITEM (filter), GIMP_ITEM (channel), GIMP_ITEM (mask), GIMP_ITEM (path_item)};
    for (guint i = 0; i < G_N_ELEMENTS (items); ++i)
      { fields_attributes (items[i]); tattoos[i] = gimp_item_get_tattoo (items[i]); }
  }
  members = g_list_append (members, ordinary); members = g_list_append (members, clone);
  set = gimp_item_list_named_new (image, GIMP_TYPE_LAYER, "field named set", members);
  gimp_image_store_item_set (image, set); g_list_free (members);
  set = gimp_item_list_pattern_new (image, GIMP_TYPE_CHANNEL, GIMP_SELECT_PLAIN_TEXT, "field");
  gimp_image_store_item_set (image, set);
  members = g_list_append (NULL, path_item);
  set = gimp_item_list_named_new (image, GIMP_TYPE_PATH, "field path set", members);
  gimp_image_store_item_set (image, set); g_list_free (members);
  for (guint repeat = 0; repeat < 2; ++repeat)
    {
      GimpImage *copy;
      GList *paths, *channels, *sets, *items;
      save (image, file); copy = load (file); g_object_unref (image); image = copy;
      ordinary = layer (image, "field ordinary"); group = layer (image, "field group");
      clone = layer (image, "field clone"); filter = layer (image, "field filter");
      paths = gimp_image_get_path_list (image); channels = gimp_image_get_channel_list (image);
      g_assert_nonnull (ordinary); g_assert_true (GIMP_IS_GROUP_LAYER (group));
      g_assert_true (GIMP_IS_CLONE_LAYER (clone)); g_assert_true (GIMP_IS_FILTER_LAYER (filter));
      g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == ordinary);
      g_assert_cmpuint (g_list_length (paths), ==, 1); g_assert_cmpuint (g_list_length (channels), ==, 1);
      mask = gimp_layer_get_mask (ordinary); g_assert_nonnull (mask);
      {
        GimpItem *owners[] = {GIMP_ITEM (ordinary), GIMP_ITEM (group), GIMP_ITEM (clone),
                              GIMP_ITEM (filter), channels->data, GIMP_ITEM (mask), paths->data};
        for (guint i = 0; i < G_N_ELEMENTS (owners); ++i)
          { fields_assert_attributes (owners[i]); g_assert_cmpuint (gimp_item_get_tattoo (owners[i]), ==, tattoos[i]); }
      }
      g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (ordinary)), ==, -3);
      g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (ordinary)), ==, 7);
      g_assert_true (gimp_layer_get_lock_alpha (ordinary));
      g_assert_false (gimp_layer_get_apply_mask (ordinary));
      g_assert_true (gimp_layer_get_edit_mask (ordinary)); g_assert_true (gimp_layer_get_show_mask (ordinary));
      g_assert_cmpfloat (gimp_layer_get_opacity (ordinary), ==, .625);
      g_assert_cmpfloat (gimp_layer_get_opacity (clone), ==, .375);
      g_assert_cmpfloat (gimp_layer_get_opacity (filter), ==, .875);
      g_assert_true (gimp_viewable_get_expanded (GIMP_VIEWABLE (group)));
      g_assert_cmpint (gimp_layer_get_mode (ordinary), ==, GIMP_LAYER_MODE_MULTIPLY);
      g_assert_cmpint (gimp_layer_get_blend_space (ordinary), ==, GIMP_LAYER_COLOR_SPACE_RGB_LINEAR);
      g_assert_cmpint (gimp_layer_get_composite_space (ordinary), ==, GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR);
      g_assert_cmpint (gimp_layer_get_composite_mode (ordinary), ==, GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP);
      g_assert_true (gimp_channel_get_show_masked (channels->data));
      g_assert_cmpfloat_with_epsilon (gimp_channel_get_opacity (channels->data), .45, 1e-6);
      { gdouble rgba[4]; GeglColor *color = gimp_channel_get_color (channels->data);
        gegl_color_get_pixel (color, babl_format ("R'G'B'A double"), rgba);
        g_assert_cmpfloat_with_epsilon (rgba[0], .1, 1e-6);
        g_assert_cmpfloat_with_epsilon (rgba[1], .3, 1e-6);
        g_assert_cmpfloat_with_epsilon (rgba[2], .7, 1e-6); }
      sets = gimp_image_get_stored_item_sets (image, GIMP_TYPE_LAYER); g_assert_cmpuint (g_list_length (sets), ==, 1);
      g_assert_cmpstr (gimp_object_get_name (sets->data), ==, "field named set");
      items = gimp_item_list_get_items (sets->data, NULL); g_assert_cmpuint (g_list_length (items), ==, 2);
      g_assert_nonnull (g_list_find (items, ordinary)); g_assert_nonnull (g_list_find (items, clone)); g_list_free (items);
      sets = gimp_image_get_stored_item_sets (image, GIMP_TYPE_CHANNEL); g_assert_cmpuint (g_list_length (sets), ==, 1);
      g_assert_true (gimp_item_list_is_pattern (sets->data, NULL));
      g_assert_cmpstr (gimp_object_get_name (sets->data), ==, "field");
      sets = gimp_image_get_stored_item_sets (image, GIMP_TYPE_PATH); g_assert_cmpuint (g_list_length (sets), ==, 1);
      g_assert_cmpstr (gimp_object_get_name (sets->data), ==, "field path set");
      items = gimp_item_list_get_items (sets->data, NULL); g_assert_true (items && items->data == paths->data && !items->next); g_list_free (items);
      g_list_free (paths); g_list_free (channels);
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_legacy_text_and_units (void)
{
  GimpImage *image;
  gdouble saved_path_points[12] = {0};
  GFile *file = temporary_file ();
  gimp_data_factory_data_init (gimp->font_factory, gimp_get_user_context (gimp), FALSE);
  gimp_data_factory_data_wait (gimp->font_factory);
  g_assert_false (gimp_container_is_empty (gimp_data_factory_get_container (gimp->font_factory)));
  image = fixture ("legacy-xcf-fields/rgb-fields.xcf");
  for (guint repeat = 0; repeat < 3; ++repeat)
    {
      GimpLayer *text_layer = layer (image, "field text");
      GimpLayer *ordinary = layer (image, "field layer");
      GimpText *text;
      GimpUnit *unit = gimp_image_get_unit (image);
      GimpImage *copy;
      g_assert_true (GIMP_IS_TEXT_LAYER (text_layer)); text = gimp_text_layer_get_text (GIMP_TEXT_LAYER (text_layer));
      g_assert_cmpstr (text->text, ==, repeat < 2 ? "Editable old text" : "Edited after old Open");
      g_assert_cmpfloat (text->font_size, ==, repeat < 2 ? 17 : 23.5);
      g_assert_true (text->kerning); g_assert_cmpstr (text->language, ==, "en-US");
      g_assert_cmpfloat (text->indent, ==, 2.5); g_assert_cmpfloat (text->line_spacing, ==, 3.25);
      g_assert_cmpfloat (text->letter_spacing, ==, 1.5);
      g_assert_cmpfloat (gimp_unit_get_factor (unit), ==, 3.5); g_assert_cmpint (gimp_unit_get_digits (unit), ==, 3);
      g_assert_cmpstr (gimp_unit_get_symbol (unit), ==, "fu"); g_assert_cmpstr (gimp_unit_get_abbreviation (unit), ==, "fu");
      g_assert_true (gimp_item_get_lock_content (GIMP_ITEM (ordinary))); g_assert_true (gimp_layer_get_lock_alpha (ordinary));
      g_assert_false (gimp_layer_get_apply_mask (ordinary)); g_assert_true (gimp_layer_get_show_mask (ordinary));
      g_assert_true (gimp_layer_get_edit_mask (ordinary));
      g_assert_nonnull (gimp_item_parasite_find (GIMP_ITEM (ordinary), "field-layer-data"));
      g_assert_nonnull (gimp_image_parasite_find (image, "field-image-data"));
      g_assert_cmpuint (g_list_length (gimp_image_get_stored_item_sets (image, GIMP_TYPE_LAYER)), ==, repeat ? 2 : 1);
      g_assert_cmpuint (g_list_length (gimp_image_get_stored_item_sets (image, GIMP_TYPE_CHANNEL)), ==, 1);
      {
        GList *paths = gimp_image_get_path_list (image);
        GList *sets = gimp_image_get_stored_item_sets (image, GIMP_TYPE_PATH), *items;
        GimpStroke *stroke; GArray *points; gboolean closed;
        g_assert_cmpuint (g_list_length (paths), ==, 1); g_assert_cmpuint (g_list_length (sets), ==, 1);
        g_assert_cmpstr (gimp_object_get_name (paths->data), ==, "field path");
        g_assert_true (gimp_item_get_visible (paths->data));
        g_assert_nonnull (gimp_item_parasite_find (paths->data, "field-path-data"));
        items = gimp_item_list_get_items (sets->data, NULL); g_assert_true (items && items->data == paths->data && !items->next);
        g_list_free (items); g_assert_cmpint (gimp_path_get_n_strokes (paths->data), ==, 1);
        stroke = gimp_path_stroke_get_next (paths->data, NULL); points = gimp_stroke_control_points_get (stroke, &closed);
        g_assert_false (closed); g_assert_cmpuint (points->len, ==, 6);
        for (guint i = 0; i < points->len; ++i)
          {
            GimpAnchor *anchor = &g_array_index (points, GimpAnchor, i);
            g_assert_true ((anchor->position.x == 5 && anchor->position.y == 7) ||
                           (anchor->position.x == 13 && anchor->position.y == 17));
            if (!repeat) { saved_path_points[2*i] = anchor->position.x; saved_path_points[2*i+1] = anchor->position.y; }
            else { g_assert_cmpfloat (anchor->position.x, ==, saved_path_points[2*i]);
                   g_assert_cmpfloat (anchor->position.y, ==, saved_path_points[2*i+1]); }
          }
        g_array_unref (points); g_list_free (paths);
      }
      if (!repeat)
        {
          GList *items = g_list_append (NULL, text_layer);
          GimpItemList *set = gimp_item_list_named_new (image, GIMP_TYPE_LAYER, "editable texts", items);
          gimp_image_store_item_set (image, set); g_list_free (items);
        }
      else
        {
          GList *sets = gimp_image_get_stored_item_sets (image, GIMP_TYPE_LAYER);
          gboolean found = FALSE;
          for (GList *p = sets; p; p = p->next)
            if (!g_strcmp0 (gimp_object_get_name (p->data), "editable texts"))
              { GList *items = gimp_item_list_get_items (p->data, NULL);
                g_assert_true (items && items->data == text_layer && !items->next);
                found = TRUE; g_list_free (items); }
          g_assert_true (found);
        }
      if (repeat == 1)
        {
          const gint width = gimp_item_get_width (GIMP_ITEM (text_layer));
          gimp_text_layer_set (GIMP_TEXT_LAYER (text_layer), "Edit saved text", "text", "Edited after old Open", "font-size", 23.5, NULL);
          g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (text_layer)), >, width);
          g_assert_true (gimp_item_is_text_layer (GIMP_ITEM (text_layer)));
        }
      save (image, file); copy = load (file);
      g_assert_true (gimp_config_is_equal_to (GIMP_CONFIG (text),
                     GIMP_CONFIG (gimp_text_layer_get_text (GIMP_TEXT_LAYER (layer (copy, "field text"))))));
      g_object_unref (image); image = copy;
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_profiles_palettes_metadata (void)
{
  GimpImage *image = fixture ("legacy-xcf-fields/indexed-fields.xcf");
  GFile *file = temporary_file ();
  static const guint8 colors[] = {0,0,0,255,13,29,47,181,83,251,239,211};
  for (guint repeat = 0; repeat < 2; ++repeat)
    {
      GimpImage *copy; gint count; guchar *actual = _gimp_image_get_colormap (image, &count);
      g_assert_cmpint (count, ==, 4); g_assert_cmpmem (actual, count * 3, colors, sizeof colors); g_free (actual);
      g_assert_true (gimp_image_get_unit (image) == gimp_unit_mm ());
      save (image, file); copy = load (file); g_object_unref (image); image = copy;
    }
  g_object_unref (image); image = fields_image ();
  {
    GimpLayer *base = gimp_layer_new (image, 96, 80, gimp_image_get_layer_format (image, TRUE), "profiles", 1, GIMP_LAYER_MODE_NORMAL);
    GimpColorProfile *profile = gimp_color_profile_new_rgb_adobe ();
    GimpMetadata *metadata = gimp_metadata_new ();
    GimpSymmetry *symmetry;
    gchar *symmetry_config;
    GimpSamplePoint *sample;
    gchar *original_title;
    GimpUnit *unit = _gimp_unit_new (gimp, "native unit", 7.25, 4, "nu", "n.u.");
    gimp_image_add_layer (image, base, NULL, 0, FALSE);
    g_assert_true (gimp_image_set_color_profile (image, profile, NULL));
    g_assert_true (gimp_image_set_simulation_profile (image, profile));
    gimp_image_set_simulation_intent (image, GIMP_COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC);
    gimp_image_set_simulation_bpc (image, TRUE);
    g_assert_true (gexiv2_metadata_try_set_tag_string (GEXIV2_METADATA (metadata), "Xmp.dc.title", "Field metadata title", NULL));
    original_title = gexiv2_metadata_try_get_tag_string (GEXIV2_METADATA (metadata), "Xmp.dc.title", NULL);
    gimp_image_set_metadata (image, metadata, FALSE); g_object_unref (metadata);
    gimp_image_set_unit (image, unit);
    g_assert_true (gimp_image_set_active_symmetry (image, GIMP_TYPE_MIRROR));
    symmetry = g_object_ref (gimp_image_get_active_symmetry (image));
    g_object_set (symmetry, "horizontal-symmetry", TRUE, "vertical-symmetry", TRUE,
                  "point-symmetry", TRUE, "disable-transformation", TRUE,
                  "mirror-position-x", 23.5, "mirror-position-y", 37.25, NULL);
    symmetry_config = gimp_config_serialize_to_string (GIMP_CONFIG (symmetry), NULL);
    g_object_unref (symmetry);
    sample = gimp_image_add_sample_point_at_pos (image, 11, 19, FALSE);
    gimp_image_set_sample_point_pick_mode (image, sample, GIMP_COLOR_PICK_MODE_LCH, FALSE);
    for (guint repeat = 0; repeat < 2; ++repeat)
      {
        GimpImage *copy; gchar *title;
        save (image, file); copy = load (file); g_object_unref (image); image = copy;
        g_assert_true (gimp_color_profile_is_equal (profile, gimp_image_get_color_profile (image)));
        g_assert_true (gimp_color_profile_is_equal (profile, gimp_image_get_simulation_profile (image)));
        g_assert_cmpint (gimp_image_get_simulation_intent (image), ==, GIMP_COLOR_RENDERING_INTENT_ABSOLUTE_COLORIMETRIC);
        g_assert_true (gimp_image_get_simulation_bpc (image));
        g_assert_nonnull (gimp_image_get_metadata (image));
        title = gexiv2_metadata_try_get_tag_string (GEXIV2_METADATA (gimp_image_get_metadata (image)), "Xmp.dc.title", NULL);
        g_assert_cmpstr (title, ==, original_title); g_free (title);
        g_assert_cmpstr (gimp_unit_get_name (gimp_image_get_unit (image)), ==, "native unit");
        g_assert_cmpfloat (gimp_unit_get_factor (gimp_image_get_unit (image)), ==, 7.25);
        g_assert_cmpint (gimp_unit_get_digits (gimp_image_get_unit (image)), ==, 4);
        g_assert_cmpstr (gimp_unit_get_symbol (gimp_image_get_unit (image)), ==, "nu");
        g_assert_cmpstr (gimp_unit_get_abbreviation (gimp_image_get_unit (image)), ==, "n.u.");
        g_assert_true (GIMP_IS_MIRROR (gimp_image_get_active_symmetry (image)));
        { gchar *actual = gimp_config_serialize_to_string (GIMP_CONFIG (gimp_image_get_active_symmetry (image)), NULL);
          g_assert_cmpstr (actual, ==, symmetry_config); g_free (actual); }
        { GList *points = gimp_image_get_sample_points (image); gint x, y;
          g_assert_cmpuint (g_list_length (points), ==, 1);
          g_assert_cmpint (gimp_sample_point_get_pick_mode (points->data), ==, GIMP_COLOR_PICK_MODE_LCH);
          gimp_sample_point_get_position (points->data, &x, &y); g_assert_cmpint (x, ==, 11); g_assert_cmpint (y, ==, 19); }
      }
    g_free (symmetry_config);
    g_free (original_title); g_object_unref (profile);
  }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_wrong_owner_capsules (void)
{
  GFile *file = temporary_file ();
  GimpParasite *semantics[2];
  const gchar *fixtures[] = {"legacy-runtime/clone-normal-in-group.xcf", "legacy-runtime/filter-edge.xcf"};
  gimp_data_factory_data_init (gimp->font_factory, gimp_get_user_context (gimp), FALSE);
  gimp_data_factory_data_wait (gimp->font_factory);
  for (guint i = 0; i < 2; ++i)
    {
      GimpImage *original = fixture (fixtures[i]), *seed;
      GList *items;
      save (original, file); seed = load (file); items = gimp_image_get_layer_list (seed);
      semantics[i] = NULL;
      for (GList *p = items; p; p = p->next)
        if (i ? GIMP_IS_FILTER_LAYER (p->data) : GIMP_IS_CLONE_LAYER (p->data))
          semantics[i] = gimp_parasite_copy (gimp_item_parasite_find (p->data, "gimp-painter-item"));
      g_assert_nonnull (semantics[i]); g_list_free (items); g_object_unref (seed); g_object_unref (original);
    }
  for (guint test = 0; test < 10; ++test)
    {
      const guint scene = test % 5;
      GimpParasite *semantic = semantics[test / 5];
      GimpImage *image = fields_image ();
      GimpLayer *base = gimp_layer_new (image, 96, 80, gimp_image_get_layer_format (image, TRUE), "owner base", 1, GIMP_LAYER_MODE_NORMAL);
      GimpItem *owner = NULL;
      gimp_image_add_layer (image, base, NULL, 0, FALSE);
      if (scene == 0)
        { GimpLayer *group = gimp_group_layer_new (image); gimp_object_set_name (GIMP_OBJECT (group), "wrong owner");
          gimp_image_add_layer (image, group, NULL, 0, FALSE); owner = GIMP_ITEM (group); }
      else if (scene == 1)
        { GeglColor *color = gegl_color_new ("red"); GimpChannel *channel = gimp_channel_new (image, 96, 80, "wrong owner", color);
          g_object_unref (color); gimp_image_add_channel (image, channel, NULL, 0, FALSE); owner = GIMP_ITEM (channel); }
      else if (scene == 2)
        { GimpPath *p = gimp_path_new (image, "wrong owner"); gimp_image_add_path (image, p, NULL, 0, FALSE); owner = GIMP_ITEM (p); }
      else if (scene == 3)
        { GimpLayerMask *mask = gimp_layer_create_mask (base, GIMP_ADD_MASK_WHITE, NULL);
          gimp_layer_add_mask (base, mask, FALSE, NULL); owner = GIMP_ITEM (mask); }
      else
        {
          GimpText *text = g_object_new (GIMP_TYPE_TEXT, "gimp", gimp, "text", "Owner text", NULL);
          GimpLayer *text_layer = gimp_text_layer_new (image, text);
          g_assert_nonnull (text_layer); g_object_unref (text);
          gimp_image_add_layer (image, text_layer, NULL, 0, FALSE);
          gimp_object_set_name (GIMP_OBJECT (text_layer), "wrong owner"); owner = GIMP_ITEM (text_layer);
        }
      g_test_message ("Wrong-owner kind%u scene%u: %s", test / 5, scene, G_OBJECT_TYPE_NAME (owner));
      gimp_item_parasite_attach (owner, semantic, FALSE);
      for (guint repeat = 0; repeat < 2; ++repeat)
        {
          GimpImage *copy; GList *items = NULL;
          guint32 wanted_size, actual_size;
          const void *wanted = gimp_parasite_get_data (semantic, &wanted_size), *actual;
          save (image, file); copy = load (file); g_object_unref (image); image = copy;
          if (scene == 0 || scene == 4) owner = GIMP_ITEM (layer (image, "wrong owner"));
          else if (scene == 1) { items = gimp_image_get_channel_list (image); owner = items->data; }
          else if (scene == 2) { items = gimp_image_get_path_list (image); owner = items->data; }
          else owner = GIMP_ITEM (gimp_layer_get_mask (layer (image, "owner base")));
          g_assert_false (GIMP_IS_CLONE_LAYER (owner)); g_assert_false (GIMP_IS_FILTER_LAYER (owner));
          if (scene == 4) g_assert_true (GIMP_IS_TEXT_LAYER (owner));
          actual = gimp_parasite_get_data (gimp_item_parasite_find (owner, "gimp-painter-item"), &actual_size);
          g_assert_cmpmem (actual, actual_size, wanted, wanted_size);
          g_assert_nonnull (gimp_item_parasite_find (owner, "gimp-painter-origin"));
          g_list_free (items);
        }
      g_object_unref (image);
    }
  gimp_parasite_free (semantics[0]); gimp_parasite_free (semantics[1]);
  g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_retained_metadata_bound (void)
{
  const gsize size = 256u * 1024u * 1024u + 1u;
  GimpImage *image = fields_image ();
  GimpLayer *item = gimp_layer_new (image, 1, 1, gimp_image_get_layer_format (image, TRUE), "large retained origin", 1, GIMP_LAYER_MODE_NORMAL);
  GFile *file = temporary_file ();
  GError *error = NULL;
  GMappedFile *mapping;
  GFileIOStream *io;
  GFile *large;
  gchar *filename;
  GBytes *bytes;
  gpointer state;
  /* Explicit inline-only opt-out still refuses a real oversized origin.
   * Ordinary Save now uses multipart and is covered by the large public-PDB
   * roundtrip; mmap keeps this legacy preflight refusal test inexpensive. */
  large = g_file_new_tmp ("painter-retained-origin-XXXXXX", &io, NULL);
  g_assert_nonnull (large);
  g_assert_true (g_seekable_truncate (G_SEEKABLE (io), size, NULL, &error));
  g_assert_no_error (error); g_io_stream_close (G_IO_STREAM (io), NULL, NULL); g_object_unref (io);
  filename = g_file_get_path (large);
  mapping = g_mapped_file_new (filename, FALSE, &error); g_free (filename);
  g_assert_no_error (error); g_assert_nonnull (mapping);
  bytes = g_mapped_file_get_bytes (mapping);
  gimp_image_add_layer (image, item, NULL, 0, FALSE);
  g_assert_true (gimp_painter_provenance_set_bytes (G_OBJECT (item), GIMP_PAINTER_PROVENANCE_PROPERTIES, bytes));
  sentinel_write (file);
  state = xcf_painter_prepare_save_full (image, NULL, FALSE, &error);
  g_assert_null (state); g_assert_nonnull (error); g_assert_nonnull (strstr (error->message, "limit"));
  g_clear_error (&error); sentinel_check (file);
  {
    g_assert_false (xcf_save_file_with_painter_transport (gimp, image, file, NULL, FALSE, NULL, &error));
    g_assert_nonnull (error); g_clear_error (&error); sentinel_check (file);
  }
  g_object_unref (image); g_bytes_unref (bytes); g_mapped_file_unref (mapping);
  g_file_delete (large, NULL, NULL); g_object_unref (large);
  g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_modern_effect_records (void)
{
  GimpImage *image = fields_image ();
  GFile *file = temporary_file ();
  GimpLayer *base = gimp_layer_new (image, 96, 80, gimp_image_get_layer_format (image, TRUE), "effect owner", 1, GIMP_LAYER_MODE_NORMAL);
  GeglNode *node = gegl_node_new_child (NULL, "operation", "gegl:brightness-contrast", "brightness", .125, "contrast", .375, NULL);
  GimpDrawableFilter *effect;
  guint8 raw[] = {0x6f, 0x70, 0x71, 0x72, 0, 0, 0, 5, 0, 0xff, 0x51, 0, 0x17};
  guint8 future[33] = {0}, mismatched[31] = {0};
  GPtrArray *records = g_ptr_array_new_with_free_func ((GDestroyNotify) g_bytes_unref);
  wire32 (future, 0, 45); wire32 (future, 4, 25); wire32 (future, 8, 13);
  memcpy (future + 12, "future-value", 13); wire32 (future, 25, 99); wire32 (future, 29, 0x0badcafe);
  g_ptr_array_add (records, g_bytes_new (raw, sizeof raw));
  g_ptr_array_add (records, g_bytes_new (raw, sizeof raw)); /* Ordered duplicates remain distinct. */
  g_ptr_array_add (records, g_bytes_new (future, sizeof future));
  wire32 (mismatched, 0, 45); wire32 (mismatched, 4, 23); wire32 (mismatched, 8, 11);
  memcpy (mismatched + 12, "brightness", 11); wire32 (mismatched, 23, 1); wire32 (mismatched, 27, 123);
  g_ptr_array_add (records, g_bytes_new (mismatched, sizeof mismatched));
  gimp_image_add_layer (image, base, NULL, 0, FALSE);
  effect = gimp_drawable_filter_new (GIMP_DRAWABLE (base), "field effect", node, "gimp-gegl");
  g_object_unref (node);
  gimp_drawable_filter_set_opacity (effect, .625);
  gimp_drawable_filter_set_mode (effect, GIMP_LAYER_MODE_MULTIPLY, GIMP_LAYER_COLOR_SPACE_RGB_LINEAR,
                                GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR, GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP);
  gimp_drawable_filter_set_clip (effect, TRUE);
  gimp_drawable_filter_set_region (effect, GIMP_FILTER_REGION_DRAWABLE);
  gimp_drawable_filter_apply (effect, NULL);
  g_assert_true (gimp_drawable_filter_commit (effect, TRUE, NULL, FALSE));
  gimp_drawable_filter_layer_mask_freeze (effect);
  fields_attributes (GIMP_ITEM (gimp_drawable_filter_get_mask (effect)));
  gimp_filter_set_active (GIMP_FILTER (effect), FALSE);
  g_assert_true (gimp_painter_provenance_set_records (G_OBJECT (effect), records));
  g_ptr_array_unref (records); g_object_unref (effect);
  for (guint repeat = 0; repeat < 3; ++repeat)
    {
      GimpImage *copy;
      GimpContainer *filters;
      gdouble brightness, contrast;
      save (image, file); copy = load (file); g_object_unref (image); image = copy;
      base = layer (image, "effect owner");
      filters = gimp_drawable_get_filters (GIMP_DRAWABLE (base));
      g_assert_cmpint (gimp_container_get_n_children (filters), ==, 1);
      effect = GIMP_DRAWABLE_FILTER (gimp_container_get_child_by_index (filters, 0));
      g_assert_cmpstr (gimp_object_get_name (effect), ==, "field effect");
      g_assert_false (gimp_filter_get_active (GIMP_FILTER (effect)));
      g_assert_cmpfloat (gimp_drawable_filter_get_opacity (effect), ==, .625);
      g_assert_true (gimp_drawable_filter_get_clip (effect));
      g_assert_cmpint (gimp_drawable_filter_get_region (effect), ==, GIMP_FILTER_REGION_DRAWABLE);
      g_assert_cmpint (gimp_drawable_filter_get_paint_mode (effect), ==, GIMP_LAYER_MODE_MULTIPLY);
      g_assert_cmpint (gimp_drawable_filter_get_blend_space (effect), ==, GIMP_LAYER_COLOR_SPACE_RGB_LINEAR);
      g_assert_cmpint (gimp_drawable_filter_get_composite_space (effect), ==, GIMP_LAYER_COLOR_SPACE_RGB_NON_LINEAR);
      g_assert_cmpint (gimp_drawable_filter_get_composite_mode (effect), ==, GIMP_LAYER_COMPOSITE_CLIP_TO_BACKDROP);
      g_assert_nonnull (gimp_drawable_filter_get_mask (effect));
      fields_assert_attributes (GIMP_ITEM (gimp_drawable_filter_get_mask (effect)));
      g_assert_cmpuint (gimp_item_get_tattoo (GIMP_ITEM (gimp_drawable_filter_get_mask (effect))), >, 0);
      node = gimp_drawable_filter_get_operation (effect);
      g_assert_cmpstr (gegl_node_get_operation (node), ==, "gegl:brightness-contrast");
      gegl_node_get (node, "brightness", &brightness, "contrast", &contrast, NULL);
      g_assert_cmpfloat (brightness, ==, repeat ? .25 : .125); g_assert_cmpfloat (contrast, ==, .375);
      records = gimp_painter_provenance_ref_records (G_OBJECT (effect));
      g_assert_nonnull (records); g_assert_cmpuint (records->len, ==, 4);
      for (guint i = 0; i < records->len; ++i)
        { gsize size; const void *bytes = g_bytes_get_data (g_ptr_array_index (records, i), &size);
          if (i == 2) g_assert_cmpmem (bytes, size, future, sizeof future);
          else if (i == 3) g_assert_cmpmem (bytes, size, mismatched, sizeof mismatched);
          else g_assert_cmpmem (bytes, size, raw, sizeof raw); }
      g_ptr_array_unref (records);
      if (!repeat)
        {
          GeglNode *copy_node;
          GimpDrawableFilter *duplicate = gimp_drawable_filter_duplicate (GIMP_DRAWABLE (base), effect);
          g_assert_nonnull (duplicate);
          records = gimp_painter_provenance_ref_records (G_OBJECT (duplicate));
          g_assert_nonnull (records); g_assert_cmpuint (records->len, ==, 4); g_ptr_array_unref (records);
          copy_node = gimp_drawable_filter_get_operation (duplicate);
          gegl_node_set (copy_node, "brightness", .25, NULL);
          gimp_drawable_remove_filter (GIMP_DRAWABLE (base), GIMP_FILTER (effect));
          gimp_drawable_filter_apply (duplicate, NULL);
          g_assert_true (gimp_drawable_filter_commit (duplicate, TRUE, NULL, FALSE));
          gimp_drawable_filter_layer_mask_freeze (duplicate);
          gimp_filter_set_active (GIMP_FILTER (duplicate), FALSE); g_object_unref (duplicate);
        }
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_unsupported_effect_refuses_save (void)
{
  GimpImage *image = fields_image ();
  GimpLayer *base = gimp_layer_new (image, 4, 4, gimp_image_get_layer_format (image, TRUE), "unsupported effect owner", 1, GIMP_LAYER_MODE_NORMAL);
  GeglNode *node = gegl_node_new_child (NULL, "operation", "gegl:brightness-contrast", NULL);
  GimpDrawableFilter *effect;
  GFile *file = temporary_file (), *output = temporary_file ();
  gchar *contents;
  gsize size;
  const gchar *operation = "gegl:brightness-contrast";
  gboolean changed = FALSE;
  gimp_image_add_layer (image, base, NULL, 0, FALSE);
  effect = gimp_drawable_filter_new (GIMP_DRAWABLE (base), "unavailable effect", node, "gimp-gegl");
  g_object_unref (node); gimp_drawable_filter_apply (effect, NULL);
  g_assert_true (gimp_drawable_filter_commit (effect, TRUE, NULL, FALSE));
  gimp_drawable_filter_layer_mask_freeze (effect); g_object_unref (effect);
  save (image, file); g_object_unref (image);
  g_assert_true (g_file_load_contents (file, NULL, &contents, &size, NULL, NULL));
  for (gsize i = 0; i + strlen (operation) < size; ++i)
    if (!memcmp (contents + i, operation, strlen (operation) + 1))
      { memset (contents + i, 'x', strlen (operation)); changed = TRUE; break; }
  g_assert_true (changed);
  g_assert_true (g_file_replace_contents (file, contents, size, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, NULL)); g_free (contents);
  image = load (file);
  for (guint duplicate = 0; duplicate < 2; ++duplicate)
    {
      GError *error = NULL;
      GimpPlugInProcedure *proc = GIMP_PLUG_IN_PROCEDURE (gimp_pdb_lookup_procedure (gimp->pdb, "gimp-xcf-save"));
      sentinel_write (output);
      g_assert_cmpint (file_save (gimp, image, NULL, output, proc, GIMP_RUN_NONINTERACTIVE, FALSE, FALSE, FALSE, &error), !=, GIMP_PDB_SUCCESS);
      g_assert_nonnull (error); g_assert_nonnull (strstr (error->message, "unsupported native GEGL effect")); g_clear_error (&error);
      sentinel_check (output);
      if (!duplicate)
        { GimpImage *copy = gimp_image_duplicate (image); g_object_unref (image); image = copy; }
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_file_delete (output, NULL, NULL);
  g_object_unref (file); g_object_unref (output);
}

static void fields_modern_effect_argument_families (void)
{
  const gchar *operations[] = {"gegl:noise-rgb", "gegl:color", "gegl:text", "gegl:gaussian-blur", "gimp:levels"};
  guint coverage = 0;
  for (guint scene = 0; scene < G_N_ELEMENTS (operations); ++scene)
    {
      GimpImage *image = fields_image (), *copy;
      GimpLayer *base = gimp_layer_new (image, 4, 4, gimp_image_get_layer_format (image, TRUE), "argument owner", 1, GIMP_LAYER_MODE_NORMAL);
      GeglNode *node = gegl_node_new_child (NULL, "operation", operations[scene], NULL), *restored;
      GimpDrawableFilter *effect;
      GParamSpec **specs;
      guint count;
      GValue *expected;
      guint *families;
      GFile *file = temporary_file ();
      g_assert_true (gegl_has_operation (operations[scene]));
      specs = gegl_operation_list_properties (operations[scene], &count);
      expected = g_new0 (GValue, count); families = g_new0 (guint, count);
      for (guint i = 0; i < count; ++i)
        {
          GParamSpec *spec = specs[i]; GValue value = G_VALUE_INIT;
          if (!(spec->flags & G_PARAM_WRITABLE)) continue;
          g_value_init (&value, spec->value_type); gegl_node_get_property (node, spec->name, &value);
          if (G_VALUE_HOLDS_INT (&value))
            { families[i] = 1; g_value_set_int (&value, CLAMP (3, G_PARAM_SPEC_INT (spec)->minimum, G_PARAM_SPEC_INT (spec)->maximum)); }
          else if (G_VALUE_HOLDS_BOOLEAN (&value))
            { families[i] = 2; g_value_set_boolean (&value, !g_value_get_boolean (&value)); }
          else if (G_VALUE_HOLDS_DOUBLE (&value))
            { families[i] = 4; g_value_set_double (&value, CLAMP (.25, G_PARAM_SPEC_DOUBLE (spec)->minimum, G_PARAM_SPEC_DOUBLE (spec)->maximum)); }
          else if (G_VALUE_HOLDS_FLOAT (&value))
            { families[i] = 4; g_value_set_float (&value, CLAMP (.25, G_PARAM_SPEC_FLOAT (spec)->minimum, G_PARAM_SPEC_FLOAT (spec)->maximum)); }
          else if (G_VALUE_HOLDS_STRING (&value))
            { families[i] = 8; g_value_set_string (&value, !strcmp (spec->name, "font") ? "Sans" : "Field text"); }
          else if (G_VALUE_HOLDS_ENUM (&value))
            { GEnumClass *klass = g_type_class_ref (spec->value_type); families[i] = 16;
              g_value_set_enum (&value, klass->values[klass->n_values > 1 ? 1 : 0].value); g_type_class_unref (klass); }
          else if (g_type_is_a (spec->value_type, GIMP_TYPE_CONFIG))
            { GObject *config = g_object_new (spec->value_type, NULL); families[i] = 32; g_value_take_object (&value, config); }
          else if (G_VALUE_HOLDS_UINT (&value))
            { families[i] = 64; g_value_set_uint (&value, CLAMP (123u, G_PARAM_SPEC_UINT (spec)->minimum, G_PARAM_SPEC_UINT (spec)->maximum)); }
          else if (g_type_is_a (spec->value_type, GEGL_TYPE_COLOR))
            { families[i] = 128; g_value_take_object (&value, gegl_color_new ("rgba(0.125,0.375,0.625,0.875)")); }
          if (families[i])
            { gegl_node_set_property (node, spec->name, &value); g_value_init (&expected[i], spec->value_type);
              gegl_node_get_property (node, spec->name, &expected[i]); coverage |= families[i]; }
          g_value_unset (&value);
        }
      gimp_image_add_layer (image, base, NULL, 0, FALSE);
      effect = gimp_drawable_filter_new (GIMP_DRAWABLE (base), operations[scene], node, "gimp-gegl");
      gimp_filter_set_active (GIMP_FILTER (effect), FALSE); gimp_drawable_filter_apply (effect, NULL);
      g_assert_true (gimp_drawable_filter_commit (effect, TRUE, NULL, FALSE));
      gimp_drawable_filter_layer_mask_freeze (effect); g_object_unref (effect); g_object_unref (node);
      save (image, file); copy = load (file);
      effect = GIMP_DRAWABLE_FILTER (gimp_container_get_child_by_index (gimp_drawable_get_filters (GIMP_DRAWABLE (layer (copy, "argument owner"))), 0));
      g_assert_nonnull (effect); restored = gimp_drawable_filter_get_operation (effect);
      for (guint i = 0; i < count; ++i)
        if (families[i])
          {
            GValue actual = G_VALUE_INIT; g_value_init (&actual, specs[i]->value_type);
            gegl_node_get_property (restored, specs[i]->name, &actual);
            g_test_message ("Effect argument %s/%s: %s", operations[scene], specs[i]->name, g_type_name (specs[i]->value_type));
            if (families[i] == 4 && G_VALUE_HOLDS_DOUBLE (&actual))
              g_assert_cmpfloat (g_value_get_double (&actual), ==, (gfloat) g_value_get_double (&expected[i]));
            else if (families[i] == 32)
              g_assert_true (gimp_config_is_equal_to (g_value_get_object (&actual), g_value_get_object (&expected[i])));
            else if (families[i] == 128)
              { gdouble a[4], b[4]; gegl_color_get_pixel (g_value_get_object (&actual), babl_format ("RGBA double"), a);
                gegl_color_get_pixel (g_value_get_object (&expected[i]), babl_format ("RGBA double"), b);
                for (guint j = 0; j < 4; ++j) g_assert_cmpfloat_with_epsilon (a[j], b[j], 1e-12); }
            else g_assert_cmpint (g_param_values_cmp (specs[i], &actual, &expected[i]), ==, 0);
            g_value_unset (&actual); g_value_unset (&expected[i]);
          }
      g_free (families); g_free (expected); g_free (specs); g_object_unref (copy); g_object_unref (image);
      g_file_delete (file, NULL, NULL); g_object_unref (file);
    }
  g_assert_cmpuint (coverage, ==, 255);
}

static void fields_clone_duplicate_name_identity (void)
{
  GimpImage *image = fields_image ();
  GFile *file = temporary_file ();
  GimpLayer *first = gimp_layer_new (image, 4, 4, gimp_image_get_layer_format (image, TRUE), "equal name", 1, GIMP_LAYER_MODE_NORMAL);
  GimpLayer *second = gimp_layer_new (image, 4, 4, gimp_image_get_layer_format (image, TRUE), "equal name", 1, GIMP_LAYER_MODE_NORMAL);
  GimpLayer *clone;
  const guint8 first_pixel[] = {1, 3, 5, 255}, chosen_pixel[] = {17, 31, 47, 255}, edit[] = {71, 103, 149, 255};
  GimpTattoo chosen_tattoo;
  gimp_image_add_layer (image, first, NULL, 0, FALSE);
  gimp_image_add_layer (image, second, NULL, 1, FALSE);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (first)), GEGL_RECTANGLE (0,0,1,1), 0, babl_format ("R'G'B'A u8"), first_pixel, GEGL_AUTO_ROWSTRIDE);
  gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (second)), GEGL_RECTANGLE (0,0,1,1), 0, babl_format ("R'G'B'A u8"), chosen_pixel, GEGL_AUTO_ROWSTRIDE);
  clone = gimp_clone_layer_new (image, second, 4, 4, "identity clone", 1, GIMP_LAYER_MODE_NORMAL);
  gimp_image_add_layer (image, clone, NULL, 0, FALSE);
  chosen_tattoo = gimp_item_get_tattoo (GIMP_ITEM (second));
  /* Deliberately construct the historical duplicate-name situation after
   * insertion's native unique-name policy. The saved reference must use ID. */
  gimp_object_set_name (GIMP_OBJECT (second), "equal name");
  g_assert_cmpstr (gimp_object_get_name (first), ==, gimp_object_get_name (second));
  for (guint repeat = 0; repeat < 2; ++repeat)
    {
      GimpImage *copy; GList *layers; guint seen = 0;
      GimpLayer *chosen;
      save (image, file); copy = load (file); g_object_unref (image); image = copy;
      clone = layer (image, "identity clone"); g_assert_true (GIMP_IS_CLONE_LAYER (clone));
      chosen = gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)); g_assert_nonnull (chosen);
      g_assert_cmpuint (gimp_item_get_tattoo (GIMP_ITEM (chosen)), ==, chosen_tattoo);
      layers = gimp_image_get_layer_list (image);
      for (GList *p = layers; p; p = p->next)
        if (g_str_has_prefix (gimp_object_get_name (p->data), "equal name"))
          { if (!seen) { g_assert_true (p->data != chosen); pixel (p->data, 1,3,5,255); }
            else g_assert_true (p->data == chosen);
            ++seen; }
      g_assert_cmpuint (seen, ==, 2); g_list_free (layers);
      pixel (clone, repeat ? 71 : 17, repeat ? 103 : 31, repeat ? 149 : 47, 255);
      gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (chosen)), GEGL_RECTANGLE (0,0,1,1), 0, babl_format ("R'G'B'A u8"), edit, GEGL_AUTO_ROWSTRIDE);
      gimp_drawable_update (GIMP_DRAWABLE (chosen), 0,0,1,1); pixel (clone,71,103,149,255);
      if (!repeat) gimp_object_set_name (GIMP_OBJECT (chosen), "equal name");
    }
  g_object_unref (image); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_absent_native_properties (void)
{
  static const guint8 magic[] = {'G','P','X','C','F',0,0,0,1,0,0,0};
  GVariant *dict = g_variant_ref_sink (g_variant_new_parsed ("{'version': <uint32 1>, 'kind': <'ordinary'>}"));
  GByteArray *wire = g_byte_array_new ();
  GBytes *bytes;
  GFile *file;
  GimpImage *image, *defaults = fields_image ();
  gdouble default_x, default_y;
#if G_BYTE_ORDER == G_BIG_ENDIAN
  { GVariant *swapped = g_variant_byteswap (dict); g_variant_unref (dict); dict = swapped; }
#endif
  g_byte_array_append (wire, magic, sizeof magic);
  g_byte_array_append (wire, g_variant_get_data (dict), g_variant_get_size (dict));
  g_variant_unref (dict); bytes = g_byte_array_free_to_bytes (wire);
  file = synthetic_capsule_file (bytes); g_bytes_unref (bytes); image = load (file);
  gimp_image_get_resolution (defaults, &default_x, &default_y);
  for (guint repeat = 0; repeat < 2; ++repeat)
    {
      GimpLayer *item = layer (image, "L"); GimpImage *copy;
      gdouble x, y;
      g_assert_true (G_OBJECT_TYPE (item) == GIMP_TYPE_LAYER);
      g_assert_cmpint (gimp_item_get_width (GIMP_ITEM (item)), ==, 1);
      g_assert_cmpint (gimp_item_get_height (GIMP_ITEM (item)), ==, 1);
      g_assert_true (gimp_item_get_visible (GIMP_ITEM (item)));
      g_assert_cmpfloat (gimp_layer_get_opacity (item), ==, 1);
      g_assert_cmpint (gimp_layer_get_mode (item), ==, GIMP_LAYER_MODE_NORMAL);
      g_assert_cmpint (gimp_layer_get_blend_space (item), ==, GIMP_LAYER_COLOR_SPACE_AUTO);
      g_assert_cmpint (gimp_layer_get_composite_space (item), ==, GIMP_LAYER_COLOR_SPACE_AUTO);
      g_assert_cmpint (gimp_layer_get_composite_mode (item), ==, GIMP_LAYER_COMPOSITE_AUTO);
      g_assert_cmpint (gimp_item_get_color_tag (GIMP_ITEM (item)), ==, GIMP_COLOR_TAG_NONE);
      g_assert_false (gimp_layer_get_lock_alpha (item));
      g_assert_false (gimp_item_get_lock_content (GIMP_ITEM (item)));
      g_assert_false (gimp_item_get_lock_position (GIMP_ITEM (item)));
      g_assert_false (gimp_item_get_lock_visibility (GIMP_ITEM (item)));
      g_assert_cmpint (gimp_item_get_offset_x (GIMP_ITEM (item)), ==, 0);
      g_assert_cmpint (gimp_item_get_offset_y (GIMP_ITEM (item)), ==, 0);
      g_assert_null (gimp_layer_get_mask (item));
      g_assert_true (gimp_image_get_unit (image) == gimp_image_get_unit (defaults));
      gimp_image_get_resolution (image, &x, &y); g_assert_cmpfloat (x, ==, default_x); g_assert_cmpfloat (y, ==, default_y);
      /* The registered Open/Save path may attach technical metadata even
       * when the wire has no metadata property. It must not invent user tags. */
      if (gimp_image_get_metadata (image))
        g_assert_false (gexiv2_metadata_try_has_tag (GEXIV2_METADATA (gimp_image_get_metadata (image)), "Xmp.dc.title", NULL));
      g_assert_null (gimp_image_get_simulation_profile (image));
      g_assert_cmpint (gimp_image_get_simulation_intent (image), ==, GIMP_COLOR_RENDERING_INTENT_RELATIVE_COLORIMETRIC);
      g_assert_false (gimp_image_get_simulation_bpc (image));
      g_assert_null (gimp_image_get_stored_item_sets (image, GIMP_TYPE_LAYER));
      pixel (item,9,8,7,255);
      save (image, file); copy = load (file); g_object_unref (image); image = copy;
    }
  g_object_unref (image); g_object_unref (defaults); g_file_delete (file, NULL, NULL); g_object_unref (file);
}

static void fields_native_precision_and_compression (void)
{
  const GimpPrecision precisions[] = {
    GIMP_PRECISION_U8_LINEAR, GIMP_PRECISION_U8_NON_LINEAR, GIMP_PRECISION_U8_PERCEPTUAL,
    GIMP_PRECISION_U16_LINEAR, GIMP_PRECISION_U16_NON_LINEAR, GIMP_PRECISION_U16_PERCEPTUAL,
    GIMP_PRECISION_U32_LINEAR, GIMP_PRECISION_U32_NON_LINEAR, GIMP_PRECISION_U32_PERCEPTUAL,
    GIMP_PRECISION_HALF_LINEAR, GIMP_PRECISION_HALF_NON_LINEAR, GIMP_PRECISION_HALF_PERCEPTUAL,
    GIMP_PRECISION_FLOAT_LINEAR, GIMP_PRECISION_FLOAT_NON_LINEAR, GIMP_PRECISION_FLOAT_PERCEPTUAL,
    GIMP_PRECISION_DOUBLE_LINEAR, GIMP_PRECISION_DOUBLE_NON_LINEAR, GIMP_PRECISION_DOUBLE_PERCEPTUAL };
  const gdouble input[] = {.125,.25,.375,.5, .75,.625,.5,1, .03125,.875,.4375,.75, 1,0,.0625,.25};
  for (guint base_type = 0; base_type < 2; ++base_type)
    for (guint p = 0; p < G_N_ELEMENTS (precisions); ++p)
      for (guint compression = 0; compression < 2; ++compression)
        {
          GimpImage *image = gimp_image_new (gimp, 2, 2, base_type ? GIMP_GRAY : GIMP_RGB, precisions[p]), *copy;
          const Babl *format = gimp_image_get_layer_format (image, TRUE);
          const gsize size = 4 * babl_format_get_bytes_per_pixel (format);
          guint8 *expected = g_malloc (size), *actual = g_malloc (size);
          GimpLayer *item = gimp_layer_new (image, 2, 2, format, "native precision", 1, GIMP_LAYER_MODE_NORMAL);
          GFile *file = temporary_file ();
          gimp_image_add_layer (image, item, NULL, 0, FALSE);
          babl_process (babl_fish (babl_format ("RGBA double"), format), input, expected, 4);
          gegl_buffer_set (gimp_drawable_get_buffer (GIMP_DRAWABLE (item)), GEGL_RECTANGLE (0,0,2,2), 0, format, expected, GEGL_AUTO_ROWSTRIDE);
          gimp_image_set_xcf_compression (image, compression);
          save (image, file); copy = load (file); item = layer (copy, "native precision");
          g_assert_cmpint (gimp_image_get_precision (copy), ==, precisions[p]);
          g_assert_cmpint (gimp_image_get_base_type (copy), ==, base_type ? GIMP_GRAY : GIMP_RGB);
          g_assert_cmpint (gimp_image_get_xcf_compression (copy), ==, compression);
          gegl_buffer_get (gimp_drawable_get_buffer (GIMP_DRAWABLE (item)), GEGL_RECTANGLE (0,0,2,2), 1,
                           gimp_drawable_get_format (GIMP_DRAWABLE (item)), actual, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
          g_assert_cmpmem (actual, size, expected, size);
          g_test_message ("Saved native base%u precision%d compression%u exact %zu bytes", base_type, precisions[p], compression, size);
          g_free (actual); g_free (expected); g_object_unref (copy); g_object_unref (image);
          g_file_delete (file, NULL, NULL); g_object_unref (file);
        }
}
