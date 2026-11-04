static guint ordinal_for (GtkWidget *widget)
{
  for (guint i = 0; i < image_observation.images; ++i)
    if (image_observation.identities[i].object == (GObject *) widget) return i + 1;
  return 0;
}
static gboolean same_nested, cross_nested, destroy_original, destroy_only, backwards;
static GtkWidget *unknown_nested_image;
static void forward (GtkWidget *, gint *, gint *);
static void original (GtkWidget *widget, gint *minimum, gint *natural)
{
  guint before_getters = getter_calls;
  ++originals; fake_time += 10;
  if (same_nested) { same_nested = FALSE; forward (widget, NULL, NULL); }
  if (cross_nested) { cross_nested = FALSE; forward (internal_image, NULL, NULL); }
  if (unknown_nested_image) {
    GtkWidget *nested = unknown_nested_image; unknown_nested_image = NULL;
    forward (nested, NULL, NULL);
  }
  if (backwards) { backwards = FALSE; fake_time -= 1000; }
  if (destroy_original || destroy_only) {
    g_signal_emit_by_name (widget, "destroy");
    if (destroy_original) {
      g_object_unref (widget);
      forwarding_destroyed = TRUE;
    }
  }
  if (minimum) *minimum = 11;
  if (natural) *natural = 12;
  if (destroy_original || destroy_only) assert (before_getters == getter_calls);
}
static void forward (GtkWidget *widget, gint *minimum, gint *natural)
{
  ImageCall call = image_observe_enter (widget);
  in_forward = TRUE;
  original (widget, minimum, natural); /* exact original, unchanged arguments */
  image_observe_exit (call);
  in_forward = FALSE;
}
static void mutate_metadata (GtkWidget *image)
{
  image->storage = GTK_IMAGE_GICON; image->logical = INT32_MAX; image->pixel = INT32_MAX;
  image->visible = image->mapped = image->child_visible = image->realized = FALSE;
  image->state = UINT32_MAX; image->scale = INT32_MAX;
}
int main (int argc, char **argv)
{
  const gchar *mode = argc > 1 ? argv[1] : "normal";
  guint first, internal, uncalled;
  gint minimum = -1, natural = -1;
  setup (mode);
  if (!strcmp (mode, "windows")) {
    while (root_count <= IMAGE_WINDOW_CAP) roots[root_count++] = new_widget (window_type);
  } else if (!strcmp (mode, "toplevels")) {
    while (root_count <= IMAGE_TOPLEVEL_CAP) roots[root_count++] = new_widget (container_type);
  } else if (!strcmp (mode, "nodes")) {
    for (guint i = 0; i < IMAGE_NODE_CAP; ++i)
      add_child (window_object, new_widget (widget_type), FALSE);
  } else if (!strcmp (mode, "images")) {
    for (guint i = 0; i < IMAGE_CAP; ++i)
      add_child (window_object, new_widget (image_type), FALSE);
  } else if (!strcmp (mode, "depth")) {
    GtkWidget *parent = window_object;
    for (guint i = 0; i <= IMAGE_TREE_DEPTH_CAP; ++i) {
      GtkWidget *child = new_widget (container_type); add_child (parent, child, FALSE); parent = child;
    }
    add_child (parent, new_widget (image_type), TRUE);
  }
  image_observe_begin ();
  if (!strcmp (mode, "missing-type")) {
    assert (!image_observation.active && !image_observation.enabled);
    image_observe_finish (); return 0;
  }
  assert (image_observation.active && image_observation.enabled);
  if (!strcmp (mode, "queue-finalized") || !strcmp (mode, "queue-reparented")) {
    assert (image_observation.flags == IMAGE_SNAPSHOT_MUTATED);
    assert (image_observation.images == (!strcmp (mode, "queue-finalized") ? 2u : 3u));
    image_observe_finish (); return 0;
  }
  first = ordinal_for (first_image); internal = ordinal_for (internal_image); uncalled = ordinal_for (uncalled_image);
  if (!strcmp (mode, "windows") || !strcmp (mode, "toplevels") ||
      !strcmp (mode, "nodes") || !strcmp (mode, "images") || !strcmp (mode, "depth")) {
    assert (image_observation.flags);
    image_observe_finish (); return 0;
  }
  assert (first && internal && uncalled && first != internal);
  assert (image_observation.images == 3 && image_observation.nodes == 5);
  assert (weak_adds == 9 && weak_removes == 5); /* setup queue plus images/window */
  assert (image_observation.identities[internal - 1].snapshot.ancestor ==
          (!strcmp (mode, "missing-semantic") ? 6u : 1u));
  assert (image_observation.identities[internal - 1].snapshot.host == 1);
  if (!strcmp (mode, "destroy") || !strcmp (mode, "destroy-retained")) {
    destroy_original = !strcmp (mode, "destroy"); destroy_only = !destroy_original;
    forward (first_image, &minimum, &natural);
    assert (minimum == 11 && natural == 12 && originals == 1);
    assert (image_observation.identities[first - 1].destroyed);
    assert ((image_observation.identities[first - 1].object == NULL) == destroy_original);
    assert (image_observation.identities[first - 1].metrics[0].exits == 1);
    forwarding_destroyed = FALSE;
    destroy_original = destroy_only = FALSE;
  } else if (!strcmp (mode, "reuse")) {
    GtkWidget *old = first_image;
    g_object_unref (first_image);
    assert (!image_observation.identities[first - 1].object);
    /* Synthetic reused argument address, real weak finalization. Public GTK
     * signature stubs resolve it to a new object's metadata; the stale address
     * itself is never dereferenced. This does not claim allocator reuse. */
    reuse_object = new_widget (image_type); reuse_object->parent = tool_button;
    reuse_address = old;
    forward (reuse_address, NULL, NULL); forward (reuse_address, NULL, NULL);
    assert (!image_observation.identities[first - 1].metrics[0].entries);
    assert (image_observation.unknown.entries == 2);
  } else if (!strcmp (mode, "unknown-nested")) {
    unknown_nested_image = new_widget (image_type); unknown_nested_image->parent = tool_button;
    forward (first_image, NULL, NULL);
    assert (image_observation.unknown.entries == 1 && image_observation.unknown.nested == 1);
    assert (image_observation.identities[first - 1].metrics[0].total > image_observation.unknown.total);
  } else if (!strcmp (mode, "unknown")) {
    GtkWidget *late = new_widget (image_type), *outside = new_widget (image_type);
    GtkWidget *late_window = new_widget (window_type);
    late->parent = window_object;
    forward (late, NULL, NULL); forward (late, NULL, NULL);
    assert (image_observation.unknown_first.host == 1);
    late->parent = late_window; forward (late, NULL, NULL);
    assert (image_observation.unknown_last.host == IMAGE_WINDOW_CAP + 1);
    forward (outside, NULL, NULL);
    assert (image_observation.unknown.entries == 4 && image_observation.unknown_last.host == IMAGE_WINDOW_CAP + 2);
    assert (image_observation.images == 3);
  } else if (!strcmp (mode, "metadata")) {
    first_image->logical = 32768;
    forward (first_image, NULL, NULL);
    mutate_metadata (first_image);
    first_image->parent = new_widget (semantic_types[1]); first_image->parent->parent = window_object;
    forward (first_image, NULL, NULL);
    first_image->storage = GTK_IMAGE_PIXBUF;
    forward (first_image, NULL, NULL);
    ImageIdentity *identity = &image_observation.identities[first - 1];
    assert (identity->first.logical_size == 32768 && identity->last.logical_size == -1);
    assert (identity->last.pixel_size == INT32_MAX && identity->last.state == UINT32_MAX);
    assert (identity->last.scale == INT32_MAX && identity->last.ancestor == 2 && identity->changed == 3);
  } else if (!strcmp (mode, "storages")) {
    for (guint storage = 0; storage <= GTK_IMAGE_SURFACE; ++storage) {
      first_image->storage = storage; forward (first_image, NULL, NULL);
    }
  } else if (!strcmp (mode, "parent-bound")) {
    first_image->realized = FALSE;
    first_image->parent = first_image; forward (first_image, NULL, NULL);
    first_image->parent = tool_button;
  } else if (!strcmp (mode, "counter")) {
    image_observation.entries = IMAGE_COUNT_CAP;
    forward (first_image, NULL, NULL);
    assert (!image_observation.identities[first - 1].metrics[0].entries);
  } else if (!strcmp (mode, "duration")) {
    forward (first_image, NULL, NULL);
    image_observation.identities[first - 1].metrics[1].total = INT64_MAX - 5;
    forward (first_image, NULL, NULL);
  } else if (!strcmp (mode, "clock")) {
    backwards = TRUE; forward (first_image, NULL, NULL);
  } else if (!strcmp (mode, "unmatched")) {
    ImageCall call = image_observe_enter (first_image);
    image_observe_exit (call); image_observe_exit (call);
  } else if (!strcmp (mode, "inflight")) {
    ImageCall call = image_observe_enter (first_image); assert (call.active);
  } else if (!strcmp (mode, "invalid-metadata")) {
    first_image->pixel = -2; forward (first_image, NULL, NULL);
  } else if (!strcmp (mode, "window-finalized")) {
    first_image->parent = tool_button->parent = internal_image->parent = uncalled_image->parent = NULL;
    g_object_unref (window_object);
    assert (!image_observation.window_objects[0]);
    forward (first_image, NULL, NULL);
  } else {
    same_nested = cross_nested = TRUE;
    forward (first_image, &minimum, &natural);
    forward (first_image, NULL, NULL);
    assert (originals == 4 && minimum == 11 && natural == 12);
    ImageIdentity *identity = &image_observation.identities[first - 1];
    assert (identity->metrics[0].entries == 1 && identity->metrics[1].entries == 2);
    assert (identity->metrics[1].nested == 1);
    assert (image_observation.identities[internal - 1].metrics[0].nested == 1);
    assert (identity->metrics[0].total > identity->metrics[1].longest);
    assert (!image_observation.identities[uncalled - 1].metrics[0].entries);
  }
  image_observe_finish ();
  guint entries = image_observation.entries, adds = weak_adds, removes = weak_removes;
  assert (!image_observation.active);
  ImageCall inactive = image_observe_enter (first_image); assert (!inactive.active);
  image_observe_exit (inactive);
  image_observe_begin (); image_observe_finish ();
  assert (image_observation.entries == entries && weak_adds == adds && weak_removes == removes);
  if (!strcmp (mode, "normal")) {
    assert (!image_observation.flags && removes == adds);
    guint destroyed = image_observation.identities[first - 1].destroyed;
    g_signal_emit_by_name (first_image, "destroy");
    assert (image_observation.identities[first - 1].destroyed == destroyed);
  }
  return 0;
}
