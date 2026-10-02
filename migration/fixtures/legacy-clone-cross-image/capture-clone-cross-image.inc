/* Ordinary RGB-to-RGB copy and relocation observations. No core behavior edits. */
static GimpLayer *
cross_child (GimpLayer *group, const gchar *name)
{
  return GIMP_LAYER (gimp_container_get_child_by_name
    (gimp_viewable_get_children (GIMP_VIEWABLE (group)), name));
}
static void
cross_fill (GimpLayer *layer, gdouble r, gdouble g, gdouble b, gdouble a)
{
  GimpRGB color = {r, g, b, a};
  gimp_drawable_fill (GIMP_DRAWABLE (layer), &color, NULL);
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, 16, 16);
}
static void
cross_pixel (const gchar *step, const gchar *target, GimpLayer *layer)
{
  guchar value[4] = {0, 0, 0, 0};
  tile_manager_read_pixel_data_1 (gimp_drawable_get_tiles (GIMP_DRAWABLE (layer)),
                                 0, 0, value);
  g_print ("CROSS_PIXEL step=%s target=%s rgba=%u,%u,%u,%u wh=%d,%d xy=%d,%d\n",
           step, target, value[0], value[1], value[2], value[3],
           gimp_item_get_width (GIMP_ITEM (layer)), gimp_item_get_height (GIMP_ITEM (layer)),
           gimp_item_get_offset_x (GIMP_ITEM (layer)), gimp_item_get_offset_y (GIMP_ITEM (layer)));
}
static GimpLayer *
cross_clone (GimpImage *image, GimpLayer *parent, const gchar *name, GimpLayer *source)
{
  GimpLayer *clone = gimp_clone_layer_new (image, source, 16, 16, name, 1.0, GIMP_NORMAL_MODE);
  g_assert (clone != NULL);
  g_assert (gimp_image_add_layer (image, clone, parent, 0, FALSE));
  return clone;
}
static void
capture_clone_cross_image (Gimp *gimp)
{
  GimpImage *original = gimp_create_image (gimp, 64, 64, GIMP_RGB, FALSE);
  GimpImage *destination = gimp_create_image (gimp, 64, 64, GIMP_RGB, FALSE);
  GimpLayer *source = gimp_layer_new (original, 16, 16, GIMP_RGBA_IMAGE,
                                     "source", 1.0, GIMP_NORMAL_MODE);
  GimpLayer *clone, *copy, *group, *inside, *internal_clone, *group_copy;
  GimpLayer *copied_inside, *copied_internal, *copied_external;
  g_assert (gimp_image_add_layer (original, source, NULL, 0, FALSE));
  cross_fill (source, 0.8, 0.2, 0.4, 0.5);
  clone = cross_clone (original, NULL, "direct", source);
  gimp_item_set_offset (GIMP_ITEM (clone), 32, 30);
  copy = GIMP_LAYER (gimp_item_convert (GIMP_ITEM (clone), destination, GIMP_TYPE_CLONE_LAYER));
  g_assert (copy != NULL);
  g_assert (gimp_image_add_layer (destination, copy, NULL, 0, FALSE));
  g_print ("CROSS_REF step=direct-copy source_original=%d clone_destination=%d same_source=%d\n",
           gimp_item_get_image (GIMP_ITEM (source)) == original,
           gimp_item_get_image (GIMP_ITEM (copy)) == destination,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == source);
  cross_pixel ("direct-copy", "copy", copy);
  cross_fill (source, 1.0, 0.0, 0.0, 1.0);
  cross_pixel ("source-red", "original", clone);
  cross_pixel ("source-red", "copy", copy);

  group = gimp_group_layer_new (original);
  g_assert (gimp_image_add_layer (original, group, NULL, 0, FALSE));
  inside = gimp_layer_new (original, 16, 16, GIMP_RGBA_IMAGE, "inside", 1.0, GIMP_NORMAL_MODE);
  g_assert (gimp_image_add_layer (original, inside, group, 0, FALSE));
  cross_fill (inside, 0.0, 0.0, 1.0, 1.0);
  internal_clone = cross_clone (original, group, "internal", inside);
  cross_clone (original, group, "external", source);
  group_copy = GIMP_LAYER (gimp_item_convert (GIMP_ITEM (group), destination, GIMP_TYPE_GROUP_LAYER));
  g_assert (group_copy != NULL);
  g_assert (gimp_image_add_layer (destination, group_copy, NULL, 0, FALSE));
  copied_inside = cross_child (group_copy, "inside");
  copied_internal = cross_child (group_copy, "internal");
  copied_external = cross_child (group_copy, "external");
  g_assert (copied_inside && copied_internal && copied_external);
  g_print ("CROSS_REF step=group-copy inside_destination=%d internal_remapped=%d external_original=%d original_unchanged=%d\n",
           gimp_item_get_image (GIMP_ITEM (copied_inside)) == destination,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_internal)) == copied_inside,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_external)) == source,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (internal_clone)) == inside);
  cross_pixel ("group-copy", "internal", copied_internal);
  cross_pixel ("group-copy", "external", copied_external);
  cross_fill (inside, 1.0, 1.0, 0.0, 1.0);
  cross_pixel ("original-inside-yellow", "original-internal", internal_clone);
  cross_pixel ("original-inside-yellow", "copied-internal", copied_internal);
  cross_fill (copied_inside, 0.0, 1.0, 1.0, 1.0);
  cross_pixel ("copied-inside-cyan", "original-internal", internal_clone);
  cross_pixel ("copied-inside-cyan", "copied-internal", copied_internal);

  g_object_ref (source);
  gimp_image_remove_layer (original, source, FALSE, NULL);
  gimp_item_unset_removed (GIMP_ITEM (source));
  GIMP_ITEM_GET_CLASS (source)->convert (GIMP_ITEM (source), destination);
  g_assert (gimp_image_add_layer (destination, source, NULL, 0, FALSE));
  g_object_unref (source);
  g_print ("CROSS_REF step=source-relocated source_destination=%d original_same_source=%d copy_same_source=%d group_external_same_source=%d\n",
           gimp_item_get_image (GIMP_ITEM (source)) == destination,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (clone)) == source,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == source,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_external)) == source);
  cross_fill (source, 0.0, 1.0, 0.0, 1.0);
  cross_pixel ("source-relocated-green", "original", clone);
  cross_pixel ("source-relocated-green", "copy", copy);
  cross_pixel ("source-relocated-green", "group-external", copied_external);
  g_object_unref (original);
  g_print ("CROSS_REF step=original-closed copy_same_source=%d group_external_same_source=%d internal_remapped=%d\n",
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copy)) == source,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_external)) == source,
           gimp_clone_layer_get_source (GIMP_CLONE_LAYER (copied_internal)) == copied_inside);
  cross_fill (source, 1.0, 0.0, 1.0, 1.0);
  cross_pixel ("original-closed-magenta", "copy", copy);
  cross_pixel ("original-closed-magenta", "group-external", copied_external);
  cross_pixel ("original-closed-magenta", "group-internal", copied_internal);
  g_object_unref (destination);
  g_print ("CLONE_CROSS_IMAGE_CAPTURE_COMPLETE\n");
}
