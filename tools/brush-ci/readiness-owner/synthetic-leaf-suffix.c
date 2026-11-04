static void assert_heights (void)
{
  GtkWidgetClass *i = g_type_class_peek (image_type);
  assert (i->get_preferred_height == inert_image && i->baseline == inert_image);
  assert (((GtkCellRendererClass *) g_type_class_peek (text_type))->get_preferred_height == inert_text);
  assert (((GtkCellRendererClass *) g_type_class_peek (pixbuf_type))->get_preferred_height == inert_text);
}
int main (int argc, char **argv)
{
  const char *mode = argc > 1 ? argv[1] : "normal";
  GType existing, dormant, late, overridden = 0;
  GType existing_text, existing_pixbuf, late_text, late_pixbuf;
  GtkWidget *child, *late_object;
  gint a = -1, b = -1, x = -1, y = -1;
  GdkRectangle area = {1, 2, 3, 4};
  omit_owner = !strcmp (mode, "missing-owner");
  setup (!strcmp (mode, "null-base"));
  if (omit_owner) {
    leaf_observe_begin (image_object, harness_window);
    leaf_observe_finish ();
    assert (leaf_observation.enabled && leaf_observation.restore_complete);
    assert (leaf_observation.flags == LEAF_MISSING_OWNER_TYPE);
    return 0;
  }
  if (!strcmp (mode, "null-base")) {
    leaf_observe_begin (image_object, harness_window);
    assert (!leaf_observation.enabled);
    leaf_observe_finish ();
    assert (leaf_observation.flags == LEAF_MISSING_BASE);
    return 0;
  }
  existing = new_image_type ("ExistingImage", image_type, NULL, TRUE);
  existing_text = new_renderer_type ("ExistingText", text_type);
  existing_pixbuf = new_renderer_type ("ExistingPixbuf", pixbuf_type);
  g_object_unref (text_object); g_object_unref (pixbuf_object);
  text_object = g_object_new (existing_text, NULL);
  pixbuf_object = g_object_new (existing_pixbuf, NULL);
  dormant = new_image_type ("DormantImage", existing, NULL, FALSE);
  assert (!g_type_class_peek (dormant));
  child = g_object_new (existing, NULL); child->parent = target_window;
  if (!strcmp (mode, "override"))
    overridden = new_image_type ("OverrideImage", existing, override_init, TRUE);
  if (!strcmp (mode, "pre-dynamic"))
    dynamic_type = new_dynamic_image ("SyntheticDynamicImage", existing);
  if (!strcmp (mode, "pre-cap")) {
    for (guint n = 0; n < LEAF_TYPE_CAP; ++n) {
      char name[40]; snprintf (name, sizeof name, "EarlyImage%u", n);
      new_image_type (name, image_type, NULL, TRUE);
    }
  }
  assert_heights ();
  leaf_observe_begin (image_object, harness_window);
  if (!strcmp (mode, "pre-dynamic")) {
    assert (!leaf_observation.enabled);
    assert (((GtkWidgetClass *) g_type_class_peek (image_type))->get_preferred_width == original_image);
    assert (((GtkWidgetClass *) g_type_class_peek (dynamic_type))->get_preferred_width == original_image);
    leaf_observe_finish ();
    assert (leaf_observation.flags == LEAF_DYNAMIC && leaf_observation.restore_complete);
    return 0;
  }
  assert (!g_type_class_peek (dormant)); /* observation must not initialize it */
  assert_heights ();
  if (!strcmp (mode, "pre-cap")) {
    assert (!leaf_observation.enabled);
    assert (((GtkWidgetClass *) g_type_class_peek (image_type))->get_preferred_width == original_image);
    assert (((GtkWidgetClass *) g_type_class_peek (existing))->get_preferred_width == original_image);
    leaf_observe_finish ();
    assert (leaf_observation.flags & LEAF_AUDIT_BOUND);
    assert (leaf_observation.restore_complete);
    return 0;
  }
  assert (leaf_observation.enabled);
  assert (((GtkWidgetClass *) g_type_class_peek (existing))->get_preferred_width == leaf_image_width);
  assert (((GtkCellRendererClass *) g_type_class_peek (existing_text))->get_preferred_width == leaf_text_width);
  assert (((GtkCellRendererClass *) g_type_class_peek (existing_pixbuf))->get_size == leaf_pixbuf_size);
  cached_image = ((GtkWidgetClass *) g_type_class_peek (existing))->get_preferred_width;
  late = new_image_type ("LateImage", existing, NULL, TRUE);
  late_text = new_renderer_type ("LateText", existing_text);
  late_pixbuf = new_renderer_type ("LatePixbuf", existing_pixbuf);
  assert (((GtkCellRendererClass *) g_type_class_peek (late_text))->get_preferred_width == leaf_text_width);
  assert (((GtkCellRendererClass *) g_type_class_peek (late_pixbuf))->get_size == leaf_pixbuf_size);
  late_object = g_object_new (late, NULL); late_object->parent = target_window;
  assert (((GtkWidgetClass *) g_type_class_peek (late))->get_preferred_width == leaf_image_width);
  assert (g_type_class_ref (dormant));
  assert (((GtkWidgetClass *) g_type_class_peek (dormant))->get_preferred_width == leaf_image_width);
  if (!strcmp (mode, "late-dynamic"))
    dynamic_type = new_dynamic_image ("SyntheticLateDynamicImage", late);
  expected_area = &area;
  recurse = nested = 1;
  ((GtkWidgetClass *) G_OBJECT (child)->g_class)->get_preferred_width (child, &a, &b);
  assert (a == 11 && b == 12);
  assert (original_calls[0] == 2 && original_calls[1] == 1 && original_calls[2] == 1);
  ((GtkWidgetClass *) G_OBJECT (late_object)->g_class)->get_preferred_width (late_object, NULL, NULL);
  ((GtkCellRendererClass *) G_OBJECT (text_object)->g_class)->get_preferred_width
    (text_object, harness_window, &a, &b);
  assert (a == 41 && b == 42);
  ((GtkCellRendererClass *) G_OBJECT (pixbuf_object)->g_class)->get_size
    (pixbuf_object, NULL, &area, &x, &y, &a, &b);
  assert (x == 21 && y == 22 && a == 23 && b == 24);
  assert (leaf_observation.all.entries == 7 && leaf_observation.all.exits == 7);
  assert (leaf_observation.all.outer == 4 && leaf_observation.all.nested == 3);
  assert (leaf_observation.metrics[0][1].entries == 3);
  assert (leaf_observation.metrics[1][2].entries == 1);
  assert (leaf_observation.metrics[2][0].entries == 1);
  {
    GtkCellRenderer *saved = text_object;
    guint before = original_calls[1];
    text_object = g_object_new (late_text, NULL);
    ((GtkCellRendererClass *) G_OBJECT (text_object)->g_class)->get_preferred_width
      (text_object, image_object, &a, NULL);
    assert (a == 41 && original_calls[1] == before + 1);
    g_object_unref (text_object); text_object = saved;
    saved = pixbuf_object; before = original_calls[2];
    pixbuf_object = g_object_new (late_pixbuf, NULL);
    ((GtkCellRendererClass *) G_OBJECT (pixbuf_object)->g_class)->get_size
      (pixbuf_object, image_object, &area, NULL, &y, &a, NULL);
    assert (y == 22 && a == 23 && original_calls[2] == before + 1);
    g_object_unref (pixbuf_object); pixbuf_object = saved;
  }
  if (overridden) {
    GtkWidget *override_object = g_object_new (overridden, NULL);
    override_object->parent = target_window;
    ((GtkWidgetClass *) G_OBJECT (override_object)->g_class)->get_preferred_width (override_object, &a, &b);
    assert (override_calls == 1 && a == 11 && b == 12);
    assert (((GtkWidgetClass *) G_OBJECT (override_object)->g_class)->get_preferred_width == override_image);
    g_object_unref (override_object);
  }
  if (!strcmp (mode, "late-override")) {
    GType late_override = new_image_type ("LateOverrideImage", late, override_init, TRUE);
    GtkWidget *obj = g_object_new (late_override, NULL); obj->parent = target_window;
    ((GtkWidgetClass *) G_OBJECT (obj)->g_class)->get_preferred_width (obj, &a, &b);
    assert (override_calls == 1 && a == 11 && b == 12);
    g_object_unref (obj);
  }
  if (!strcmp (mode, "late-cap")) {
    for (guint n = 0; n < LEAF_TYPE_CAP; ++n) {
      char name[40]; snprintf (name, sizeof name, "LateOverflowImage%u", n);
      new_image_type (name, late, NULL, TRUE);
    }
  }
  if (!strcmp (mode, "conflict"))
    ((GtkWidgetClass *) g_type_class_peek (existing))->get_preferred_width = override_image;
  if (!strcmp (mode, "owner-bound")) {
    child->parent = child; /* synthetic only; never construct a GTK cycle */
    cached_image (child, &a, &b);
    child->parent = target_window;
    assert (leaf_observation.flags & LEAF_OWNER_BOUND);
  }
  if (!strcmp (mode, "saturation")) {
    leaf_observation.metrics[0][1].entries = LEAF_COUNT_CAP;
    cached_image (child, &a, &b);
    assert (leaf_observation.flags & LEAF_COUNTER_BOUND);
  }
  if (!strcmp (mode, "clock")) {
    clock_backwards = 1;
    cached_image (child, &a, &b);
    assert (leaf_observation.flags & LEAF_CLOCK_INVALID);
    fake_time = 2000;
  }
  child->parent = original_window;
  cached_image (child, &a, &b);
  assert (leaf_observation.metrics[0][3].entries == 1);
  /* Weak pointers clear without observation keeping these objects alive. */
  child->parent = late_object->parent = image_object->parent = NULL;
  g_object_unref (target_window);
  assert (leaf_observation.target == NULL);
  cached_image (child, &a, &b); /* now other, without dereferencing old root */
  assert (leaf_observation.metrics[0][4].entries == 1);
  if (!strcmp (mode, "unmatched"))
    leaf_observation.metrics[0][4].depth = leaf_observation.all.depth = 1;
  leaf_observe_finish ();
  assert_heights ();
  assert (((GtkWidgetClass *) g_type_class_peek (image_type))->get_preferred_width == original_image);
  if (strcmp (mode, "conflict"))
    assert (((GtkWidgetClass *) g_type_class_peek (existing))->get_preferred_width == original_image);
  assert (((GtkWidgetClass *) g_type_class_peek (late))->get_preferred_width == original_image);
  assert (((GtkWidgetClass *) g_type_class_peek (dormant))->get_preferred_width == original_image);
  assert (((GtkCellRendererClass *) g_type_class_peek (text_type))->get_preferred_width == original_text);
  assert (((GtkCellRendererClass *) g_type_class_peek (pixbuf_type))->get_size == original_pixbuf);
  assert (((GtkCellRendererClass *) g_type_class_peek (existing_text))->get_preferred_width == original_text);
  assert (((GtkCellRendererClass *) g_type_class_peek (late_text))->get_preferred_width == original_text);
  assert (((GtkCellRendererClass *) g_type_class_peek (existing_pixbuf))->get_size == original_pixbuf);
  assert (((GtkCellRendererClass *) g_type_class_peek (late_pixbuf))->get_size == original_pixbuf);
  assert (((GtkCellRendererClass *) g_type_class_peek (late_text))->get_preferred_height == inert_text);
  assert (((GtkCellRendererClass *) g_type_class_peek (late_pixbuf))->get_preferred_height == inert_text);
  assert (!leaf_observation.active);
  guint before = original_calls[0], entries = leaf_observation.all.entries;
  cached_image (child, &a, &b); /* private cached copies stay safe passthroughs */
  assert (original_calls[0] == before + 1 && leaf_observation.all.entries == entries);
  leaf_observe_finish (); leaf_observe_begin (child, harness_window); /* one-shot */
  assert (!leaf_observation.active);
  if (!strcmp (mode, "normal")) {
    assert (leaf_observation.flags == 0 && leaf_observation.coverage_complete);
    assert (leaf_observation.restore_complete);
    assert (leaf_observation.families[0].installed == 2);
    assert (leaf_observation.families[0].late == 2);
    assert (leaf_observation.families[0].restored == 4);
    for (guint family = 1; family < 3; ++family) {
      assert (leaf_observation.families[family].installed == 2);
      assert (leaf_observation.families[family].late == 1);
      assert (leaf_observation.families[family].restored == 3);
    }
  }
  if (!strcmp (mode, "override") || !strcmp (mode, "late-override"))
    assert (leaf_observation.flags == LEAF_UNKNOWN && leaf_observation.restore_complete);
  if (!strcmp (mode, "late-cap"))
    assert ((leaf_observation.flags & LEAF_AUDIT_BOUND) && !leaf_observation.restore_complete);
  if (!strcmp (mode, "conflict"))
    assert ((leaf_observation.flags & LEAF_RESTORE_CONFLICT) && !leaf_observation.restore_complete);
  if (!strcmp (mode, "late-dynamic")) {
    assert (leaf_observation.flags == LEAF_DYNAMIC && !leaf_observation.restore_complete);
    assert (((GtkWidgetClass *) g_type_class_peek (dynamic_type))->get_preferred_width == leaf_image_width);
  }
  g_object_unref (child); g_object_unref (late_object); g_object_unref (image_object);
  g_object_unref (harness_window); g_object_unref (text_object); g_object_unref (pixbuf_object);
  g_object_unref (original_window);
  return 0;
}
