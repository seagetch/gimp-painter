/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Compiled as C11 in the real canvas factory executable. */
static GimpPerspectiveGuide *explicit_model (guint id, gdouble x, gdouble y) {
  GimpPerspectiveGuide *model = gimp_perspective_guide_new (id);
  g_assert_cmpint (gimp_perspective_guide_add_vanish_points (model, x, y), ==, 0);
  return model;
}
static void assert_model (GimpCanvasPerspectiveGuide *item, GimpPerspectiveGuide *model) {
  GimpPerspectiveGuide *actual = NULL;
  g_object_get (item, "guide", &actual, NULL);
  g_assert_true (actual == model);
  g_clear_object (&actual);
}
static void explicit_signature_property (void) {
  GimpCanvasPerspectiveGuide *(*factory) (GimpDisplayShell *, GimpPerspectiveGuide *) = gimp_canvas_perspective_guide_new;
  GimpCanvasPerspectiveGuide *item;
  GParamSpec *spec;
  GObject *wrong;
  setup_shell ();
  item = factory (shell, NULL); g_assert_nonnull (item);
  g_assert_false (g_object_is_floating (item));
  spec = g_object_class_find_property (G_OBJECT_GET_CLASS (item), "guide");
  g_assert_nonnull (spec); g_assert_cmpuint (spec->value_type, ==, GIMP_TYPE_PERSPECTIVE_GUIDE);
  g_assert_true ((spec->flags & (G_PARAM_READWRITE | G_PARAM_CONSTRUCT)) == (G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_assert_false (spec->flags & G_PARAM_CONSTRUCT_ONLY);
  assert_model (item, NULL); g_assert_null (gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item)));
  g_object_unref (item);
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_DISPLAY_SHELL*failed*");
  g_assert_null (factory (NULL, NULL)); g_test_assert_expected_messages ();
  wrong = g_object_new (G_TYPE_OBJECT, NULL);
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_PERSPECTIVE_GUIDE*failed*");
  g_assert_null (factory (shell, (GimpPerspectiveGuide *) wrong)); g_test_assert_expected_messages ();
  g_assert_cmpuint (wrong->ref_count, ==, 1); g_object_unref (wrong);
}
static void explicit_identity_and_lifetime (void) {
  GimpPerspectiveGuide *model, *other, *got = NULL;
  GimpCanvasPerspectiveGuide *item;
  GimpImage *image;
  gpointer weak;
  cairo_region_t *before, *after;
  setup_shell (); image = gimp_display_get_image (shell->display);
  model = explicit_model (11, 20, 30); other = explicit_model (12, 180, 190);
  weak = model; g_object_add_weak_pointer (G_OBJECT (model), &weak);
  item = gimp_canvas_perspective_guide_new (shell, model);
  g_assert_cmpuint (G_OBJECT (model)->ref_count, ==, 2);
  g_object_unref (model); g_assert_nonnull (weak);
  g_object_get (item, "guide", &got, NULL); g_assert_true (got == weak);
  before = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item)); g_assert_nonnull (before);
  gimp_image_set_perspective_guide (image, other);
  g_object_notify (G_OBJECT (shell), "display");
  g_object_notify (G_OBJECT (shell->display), "image");
  after = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item));
  g_assert_true (cairo_region_equal (before, after)); cairo_region_destroy (after);
  assert_model (item, got);
  gimp_perspective_guide_set_vanish_points (got, 0, 80, 90);
  after = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item));
  g_assert_false (cairo_region_equal (before, after));
  cairo_region_destroy (before); cairo_region_destroy (after);
  g_object_unref (item); g_assert_nonnull (weak); /* getter owns its returned ref */
  g_object_unref (got); g_assert_null (weak);
  gimp_image_set_perspective_guide (image, NULL); g_object_unref (other);
}
static void capture_explicit_scene (GimpCanvasItem *item) {
  const gchar *path = g_getenv ("GIMP_TEST_EXPLICIT_GUIDE_SCREENSHOT");
  GtkWidget *window;
  GdkWindow *native;
  GdkPixbuf *pixels;
  GError *error = NULL;
  gint64 deadline;
  if (!path) return;
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  gimp_display_shell_add_item (shell, item);
  window = gtk_widget_get_toplevel (GTK_WIDGET (shell));
  gtk_window_resize (GTK_WINDOW (window), 1040, 1800); gtk_window_present (GTK_WINDOW (window));
  deadline = g_get_monotonic_time () + G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time () < deadline) { g_main_context_iteration (NULL, FALSE); g_usleep (10000); }
  native = gtk_widget_get_window (window);
  pixels = gdk_pixbuf_get_from_window (native, 0, 0, gdk_window_get_width (native), gdk_window_get_height (native));
  g_assert_nonnull (pixels); g_assert_true (gdk_pixbuf_save (pixels, path, "png", &error, NULL));
  g_assert_no_error (error); g_object_unref (pixels);
  gimp_display_shell_remove_item (shell, item);
}
static void explicit_replace_and_render (void) {
  GimpPerspectiveGuide *first, *second;
  GimpCanvasPerspectiveGuide *item;
  cairo_region_t *before, *after;
  cairo_surface_t *surface;
  cairo_t *cr;
  guchar *pixels;
  gboolean drawn = FALSE;
  gint stride, height;
  gdouble x, y;
  setup_shell ();
  first = explicit_model (21, 20, 30); second = explicit_model (22, 80, 90);
  item = gimp_canvas_perspective_guide_new (shell, first);
  g_object_set (item, "guide", second, NULL); assert_model (item, second);
  before = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item)); g_assert_nonnull (before);
  gimp_perspective_guide_set_vanish_points (first, 0, 200, 200);
  after = gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item));
  g_assert_true (cairo_region_equal (before, after)); cairo_region_destroy (after);
  gimp_canvas_item_transform_xy_f (GIMP_CANVAS_ITEM (item), 80, 90, &x, &y);
  g_assert_true (cairo_region_contains_point (before, (gint) x, (gint) y));
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1080, 1920);
  cr = cairo_create (surface); gimp_canvas_item_draw (GIMP_CANVAS_ITEM (item), cr); cairo_destroy (cr);
  cairo_surface_flush (surface); pixels = cairo_image_surface_get_data (surface);
  stride = cairo_image_surface_get_stride (surface); height = cairo_image_surface_get_height (surface);
  for (gint i = 0; i < stride * height; ++i) if (pixels[i]) { drawn = TRUE; break; }
  g_assert_true (drawn); cairo_surface_destroy (surface); cairo_region_destroy (before);
  capture_explicit_scene (GIMP_CANVAS_ITEM (item));
  g_object_set (item, "guide", NULL, NULL); assert_model (item, NULL);
  g_assert_null (gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item)));
  g_object_unref (item); g_object_unref (first); g_object_unref (second);
}
typedef struct { GimpCanvasPerspectiveGuide *item; GimpPerspectiveGuide *replacement; guint calls; gboolean close; } ExplicitReentry;
static void explicit_finalizer (gpointer data, GObject *dead) {
  ExplicitReentry *state = data;
  (void) dead; ++state->calls;
  g_object_set (state->item, "guide", state->replacement, NULL);
}
static void explicit_update (GimpCanvasItem *item, cairo_region_t *region, ExplicitReentry *state) {
  (void) region;
  if (state->calls++) return;
  if (state->close) g_object_run_dispose (G_OBJECT (item));
  else g_object_set (item, "guide", state->replacement, NULL);
}
static void explicit_reentrant_finalizer (void) {
  GimpPerspectiveGuide *first, *middle, *last;
  GimpCanvasPerspectiveGuide *item;
  ExplicitReentry state;
  setup_shell ();
  first = explicit_model (31, 20, 30); middle = explicit_model (32, 60, 70); last = explicit_model (33, 100, 110);
  item = gimp_canvas_perspective_guide_new (shell, first);
  state = (ExplicitReentry) {item, last, 0, FALSE};
  g_object_weak_ref (G_OBJECT (first), explicit_finalizer, &state); g_object_unref (first);
  g_object_set (item, "guide", middle, NULL);
  g_assert_cmpuint (state.calls, ==, 1); assert_model (item, last);
  g_object_unref (item); g_object_unref (middle); g_object_unref (last);
}
static void explicit_reentrant_update (gconstpointer data) {
  GimpPerspectiveGuide *first, *middle, *last;
  GimpCanvasPerspectiveGuide *item;
  ExplicitReentry state;
  setup_shell ();
  first = explicit_model (41, 20, 30); middle = explicit_model (42, 60, 70); last = explicit_model (43, 100, 110);
  item = gimp_canvas_perspective_guide_new (shell, first);
  state = (ExplicitReentry) {item, last, 0, GPOINTER_TO_INT (data)};
  g_signal_connect (item, "update", G_CALLBACK (explicit_update), &state);
  g_object_set (item, "guide", middle, NULL);
  g_assert_cmpuint (state.calls, >, 0);
  if (state.close) g_assert_true (painter_canvas_store_closed (G_OBJECT (item)));
  else assert_model (item, last);
  g_object_unref (item); g_object_unref (first); g_object_unref (middle); g_object_unref (last);
}
static void explicit_notify (GObject *object, GParamSpec *spec, ExplicitReentry *state) {
  (void) spec;
  if (state->calls++) return;
  if (state->close) g_object_run_dispose (object);
  else g_object_set (object, "guide", state->replacement, NULL);
}
static void explicit_reentrant_notify (gconstpointer data) {
  GimpPerspectiveGuide *first, *middle, *last;
  GimpCanvasPerspectiveGuide *item;
  ExplicitReentry state;
  setup_shell ();
  first = explicit_model (44, 20, 30); middle = explicit_model (45, 60, 70); last = explicit_model (46, 100, 110);
  item = gimp_canvas_perspective_guide_new (shell, first);
  state = (ExplicitReentry) {item, last, 0, GPOINTER_TO_INT (data)};
  g_signal_connect (item, "notify::guide", G_CALLBACK (explicit_notify), &state);
  g_object_set (item, "guide", middle, NULL);
  g_assert_cmpuint (state.calls, >, 0);
  if (state.close) g_assert_true (painter_canvas_store_closed (G_OBJECT (item)));
  else assert_model (item, last);
  g_object_unref (item); g_object_unref (first); g_object_unref (middle); g_object_unref (last);
}
static void explicit_auto_override (void) {
  GimpImage *image;
  GimpPerspectiveGuide *first, *fixed, *later;
  GimpCanvasPerspectiveGuide *item;
  gboolean follows = TRUE;
  setup_shell (); image = gimp_display_get_image (shell->display);
  first = explicit_model (51, 20, 30); fixed = explicit_model (52, 60, 70); later = explicit_model (53, 100, 110);
  gimp_image_set_perspective_guide (image, first);
  item = (GimpCanvasPerspectiveGuide *) gimp_canvas_perspective_guide_new_for_image (shell);
  assert_model (item, first);
  gimp_image_set_perspective_guide (image, later); assert_model (item, later);
  g_object_set (item, "guide", fixed, NULL);
  g_object_get (item, "follow-image", &follows, NULL); g_assert_false (follows);
  gimp_image_set_perspective_guide (image, first); assert_model (item, fixed);
  g_object_unref (item); gimp_image_set_perspective_guide (image, NULL);
  g_object_unref (first); g_object_unref (fixed); g_object_unref (later);
}
static void explicit_fault (gconstpointer data) {
  gint spec = GPOINTER_TO_INT (data), stage = spec / 100, caller = (spec / 10) % 10, kind = spec % 10;
  GimpPerspectiveGuide *model;
  GimpCanvasPerspectiveGuide *item;
  setup_shell (); model = explicit_model (61, 20, 30);
  painter_canvas_fault_arm (kind, stage);
  item = caller ? g_object_new (GIMP_TYPE_CANVAS_PERSPECTIVE_GUIDE, "shell", shell, "guide", model, NULL) :
                 gimp_canvas_perspective_guide_new (shell, model);
  g_assert_true (painter_canvas_fault_consumed ());
  if (caller) {
    g_assert_nonnull (item); g_assert_true (item->binding_failed);
    g_assert_true (painter_canvas_store_closed (G_OBJECT (item)));
    g_assert_null (gimp_canvas_item_get_extents (GIMP_CANVAS_ITEM (item)));
    g_object_run_dispose (G_OBJECT (item)); g_object_unref (item);
  } else g_assert_null (item);
  g_assert_cmpuint (G_OBJECT (model)->ref_count, ==, 1);
  g_assert_cmpuint (painter_canvas_fault_live (), ==, 0);
  g_assert_true (painter_canvas_fault_disconnected ());
  g_object_unref (model);
}
static void explicit_handle (void) { setup_shell (); painter_canvas_explicit_handle_test (shell); }
static void register_explicit_tests (void) {
  g_test_add_func ("/canvas-explicit/signature-property", explicit_signature_property);
  g_test_add_func ("/canvas-explicit/identity-lifetime", explicit_identity_and_lifetime);
  g_test_add_func ("/canvas-explicit/replace-render", explicit_replace_and_render);
  g_test_add_func ("/canvas-explicit/finalizer-reentry", explicit_reentrant_finalizer);
  g_test_add_data_func ("/canvas-explicit/update-replace", NULL, explicit_reentrant_update);
  g_test_add_data_func ("/canvas-explicit/update-close", GINT_TO_POINTER (1), explicit_reentrant_update);
  g_test_add_data_func ("/canvas-explicit/notify-replace", NULL, explicit_reentrant_notify);
  g_test_add_data_func ("/canvas-explicit/notify-close", GINT_TO_POINTER (1), explicit_reentrant_notify);
  g_test_add_func ("/canvas-explicit/auto-override", explicit_auto_override);
  g_test_add_func ("/canvas-explicit/typed-handle", explicit_handle);
  for (gint stage = 0; stage <= 3; ++stage) {
    if (stage == 1) continue;
    for (gint caller = 0; caller < 2; ++caller)
      for (gint kind = 1; kind <= 4; ++kind) {
        gchar *path = g_strdup_printf ("/canvas-explicit/fault/stage%d/caller%d/kind%d", stage, caller, kind);
        g_test_add_data_func (path, GINT_TO_POINTER (stage * 100 + caller * 10 + kind), explicit_fault); g_free (path);
      }
  }
}
