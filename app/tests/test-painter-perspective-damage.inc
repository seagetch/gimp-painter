/* Original26.005: real incremental redraw after model mutations. */
#include "display/gimpdisplayshell-scroll.h"
static GdkPixbuf *perspective_damage_capture (void)
{
  gimp_test_run_temp_mainloop (120);
  gdk_display_sync (gtk_widget_get_display (shell->canvas));
  GdkPixbuf *pixels = gdk_pixbuf_get_from_window (gtk_widget_get_window (shell->canvas),
                                                0, 0, shell->disp_width, shell->disp_height);
  g_assert_nonnull (pixels);
  return pixels;
}
typedef struct
{
  cairo_region_t *region;
  guint          draws;
} PerspectiveDamageClip;

/* Record the real device-space clip, not the overlay's extent estimate. */
static gboolean
perspective_damage_record_clip (GtkWidget *widget, cairo_t *cr, gpointer data)
{
  PerspectiveDamageClip *clip = data;
  clip->draws++;
  cairo_rectangle_list_t *rectangles = cairo_copy_clip_rectangle_list (cr);

  g_assert_cmpint (rectangles->status, ==, CAIRO_STATUS_SUCCESS);
  for (gint i = 0; i < rectangles->num_rectangles; i++)
    {
      cairo_rectangle_t r = rectangles->rectangles[i];
      cairo_rectangle_int_t rect = { floor (r.x), floor (r.y),
        ceil (r.x + r.width) - floor (r.x),
        ceil (r.y + r.height) - floor (r.y) };
      cairo_region_union_rectangle (clip->region, &rect);
    }
  cairo_rectangle_list_destroy (rectangles);
  return FALSE;
}

/* Xlib's clipped image/border compositing is not pixel-identical to a full
 * redraw (nor to Cairo image surfaces). Establish its actual contribution
 * without the perspective item, using precisely the recorded window clip.
 * This is a causal control, not a larger allowed channel-error threshold. */
static void
perspective_damage_border_control (PerspectiveDamageClip *clip,
                                   GdkPixbuf **full, GdkPixbuf **clipped)
{
  GdkPixbuf *restored;
  PerspectiveDamageClip replay = { cairo_region_create (), 0 };
  gulong replay_handler;

  g_assert_false (cairo_region_is_empty (clip->region));
  /* A region union cannot reproduce the chronology of multiple frames. */
  g_assert_cmpuint (clip->draws, ==, 1);
  g_assert_true (gimp_canvas_item_get_visible (shell->perspective_guide));
  gimp_canvas_item_set_visible (shell->perspective_guide, FALSE);
  gtk_widget_queue_draw (shell->canvas);
  *full = perspective_damage_capture ();
  replay_handler = g_signal_connect (shell->canvas, "draw",
    G_CALLBACK (perspective_damage_record_clip), &replay);
  gtk_widget_queue_draw_region (shell->canvas, clip->region);
  *clipped = perspective_damage_capture ();
  g_signal_handler_disconnect (shell->canvas, replay_handler);
  g_assert_cmpuint (replay.draws, ==, 1);
  g_assert_true (cairo_region_equal (clip->region, replay.region));
  cairo_region_destroy (replay.region);
  gimp_canvas_item_set_visible (shell->perspective_guide, TRUE);
  gtk_widget_queue_draw (shell->canvas);
  /* Drain restoration before the next mutation: a pending full redraw must
   * never repair the next incremental capture and conceal missing damage. */
  restored = perspective_damage_capture ();
  g_object_unref (restored);
}

static gboolean
perspective_damage_control_matches (GdkPixbuf *full, GdkPixbuf *clipped,
                                    gint x, gint y,
                                    const guchar *actual, const guchar *reference)
{
  const guchar *f = gdk_pixbuf_get_pixels (full) +
    y * gdk_pixbuf_get_rowstride (full) + x * gdk_pixbuf_get_n_channels (full);
  const guchar *c = gdk_pixbuf_get_pixels (clipped) +
    y * gdk_pixbuf_get_rowstride (clipped) + x * gdk_pixbuf_get_n_channels (clipped);

  /* Require absolute RGB equality in both frames, not just a similar delta. */
  for (guint channel = 0; channel < 3; channel++)
    if (actual[channel] != c[channel] || reference[channel] != f[channel])
      return FALSE;
  return TRUE;
}

static cairo_surface_t *perspective_damage_ink (void)
{
  cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, shell->disp_width, shell->disp_height);
  cairo_t *cr = cairo_create (surface);
  if (shell->rotate_transform) cairo_transform (cr, shell->rotate_transform);
  gimp_canvas_item_draw (shell->perspective_guide, cr);
  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
  cairo_destroy (cr); cairo_surface_flush (surface);
  return surface;
}
static gboolean perspective_damage_mask_contains (cairo_surface_t *mask, gint x, gint y)
{
  for (gint dy = -1; dy <= 1; dy++) for (gint dx = -1; dx <= 1; dx++)
    if (((guint32 *) (cairo_image_surface_get_data (mask) + (y + dy) * cairo_image_surface_get_stride (mask)))[x + dx] >> 24) return TRUE;
  return FALSE;
}
static guint perspective_damage_difference (GdkPixbuf *a, GdkPixbuf *b,
                                            cairo_surface_t *old_ink, cairo_surface_t *new_ink)
{
  guint count = 0;
  gint width = gdk_pixbuf_get_width (a), height = gdk_pixbuf_get_height (a);
  gint channels = gdk_pixbuf_get_n_channels (a);
  g_assert_cmpint (gdk_pixbuf_get_width (b), ==, width);
  g_assert_cmpint (gdk_pixbuf_get_height (b), ==, height);
  /* Bounded image ROI excludes the oncanvas toolbar, not the guide pixels. */
  for (gint y = height / 2 - 330; y < height / 2 + 330; y++)
    for (gint x = width / 2 - 230; x < width / 2 + 230; x++)
      {
        /* Independent unclipped draw masks, not get_extents(), include both
         * removed and newly drawn ink plus one pixel of AA fringe. Unrelated
         * yellow layer-boundary clipping differences are outside this gate. */
        if (!perspective_damage_mask_contains (old_ink, x, y) &&
            !perspective_damage_mask_contains (new_ink, x, y)) continue;
        const guchar *p = gdk_pixbuf_get_pixels (a) + y * gdk_pixbuf_get_rowstride (a) + x * channels;
        const guchar *q = gdk_pixbuf_get_pixels (b) + y * gdk_pixbuf_get_rowstride (b) + x * channels;
        /* Full versus clipped Cairo compositing differs by at most two
         * channel levels in the preserved37-degree diagnostic. */
        if (ABS ((gint) p[0] - q[0]) > 2 || ABS ((gint) p[1] - q[1]) > 2 ||
            ABS ((gint) p[2] - q[2]) > 2) count++;
      }
  return count;
}
/* Keep global diagnostics. Only independently drawn pre-existing border ink
 * with an exact guide-free replay may explain a difference above two levels.
 * Guide pixels, unrelated pixels, and unmatched border pixels still fail. */
static void perspective_damage_global_audit (GdkPixbuf *a, GdkPixbuf *b,
                                             cairo_surface_t *old_ink, cairo_surface_t *new_ink,
                                             PerspectiveDamageClip *clip)
{
  GdkPixbuf *control_full = NULL, *control_clipped = NULL;
  cairo_surface_t *border = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, shell->disp_width, shell->disp_height);
  cairo_t *cr = cairo_create (border);
  if (shell->rotate_transform) cairo_transform (cr, shell->rotate_transform);
  gimp_canvas_item_draw (shell->layer_boundary, cr);
  gimp_canvas_item_draw (shell->canvas_boundary, cr);
  cairo_destroy (cr); cairo_surface_flush (border);
  guint all = 0, above_two = 0, guide = 0, classified_border = 0, unexplained = 0, maximum = 0;
  gint channels = gdk_pixbuf_get_n_channels (a);
  for (gint y = 1; y < shell->disp_height - 1; y++) for (gint x = 1; x < shell->disp_width - 1; x++)
    {
      const guchar *p = gdk_pixbuf_get_pixels (a) + y * gdk_pixbuf_get_rowstride (a) + x * channels;
      const guchar *q = gdk_pixbuf_get_pixels (b) + y * gdk_pixbuf_get_rowstride (b) + x * channels;
      guint delta = MAX (ABS ((gint) p[0] - q[0]), MAX (ABS ((gint) p[1] - q[1]), ABS ((gint) p[2] - q[2])));
      if (delta) all++; maximum = MAX (maximum, delta);
      if (delta > 2)
        {
          above_two++;
          if (perspective_damage_mask_contains (old_ink, x, y) || perspective_damage_mask_contains (new_ink, x, y)) guide++;
          else if (perspective_damage_mask_contains (border, x, y))
            {
              if (!control_full)
                perspective_damage_border_control (clip, &control_full, &control_clipped);
              if (perspective_damage_control_matches (control_full, control_clipped, x, y, p, q))
                classified_border++;
              else
                unexplained++;
            }
          else unexplained++;
        }
    }
  g_test_message ("perspective global all=%u above2=%u guide=%u preexisting-border=%u unexplained=%u maximum=%u",
                  all, above_two, guide, classified_border, unexplained, maximum);
  g_assert_cmpuint (guide, ==, 0); g_assert_cmpuint (unexplained, ==, 0);
  g_clear_object (&control_full); g_clear_object (&control_clipped);
  cairo_surface_destroy (border);
}
static void perspective_damage_mutations (gconstpointer data)
{
  const gdouble angles[] = { 0, 37, 90 };
  guint a = GPOINTER_TO_UINT (data), scenes = 0, transitions = 0;
  create_image ();
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  g_assert_true (gimp_painter_canvas_ui_get_visible (shell));
  gtk_window_resize (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (shell))), 1040, 1800);
  gimp_test_run_temp_mainloop (800);
  for (guint aniso = 0; aniso < 2; aniso++)
    for (guint flip = 0; flip < 4; flip++)
      {
        GimpPerspectiveGuide *guide = gimp_perspective_guide_new (17);
        gimp_image_set_resolution (image, aniso ? 144 : 72, 72);
        gimp_display_shell_scale_set_dot_for_dot (shell, !aniso);
        gimp_display_shell_scale (shell, GIMP_ZOOM_TO, 1.25, GIMP_ZOOM_FOCUS_IMAGE_CENTER);
        gimp_display_shell_flip (shell, flip & 1, flip & 2);
        gimp_display_shell_rotate_to (shell, angles[a]);
        gimp_display_shell_scroll_set_offset (shell,
          (gint) (128 * shell->scale_x) - shell->disp_width / 2,
          (gint) (128 * shell->scale_y) - shell->disp_height / 2);
        g_assert_true (gimp_display_shell_painter_pan_begin (shell, shell->disp_width / 2, shell->disp_height / 2));
        gimp_display_shell_painter_pan_motion (shell, shell->disp_width / 2 + .375, shell->disp_height / 2 - .625);
        gimp_image_set_perspective_guide (image, guide);
        gtk_widget_queue_draw (shell->canvas);
        GdkPixbuf *previous = perspective_damage_capture ();
        cairo_surface_t *previous_ink = perspective_damage_ink ();
        for (guint step = 0; step < 7; step++)
          {
            if (step < 3)
              g_assert_cmpint (gimp_perspective_guide_add_vanish_points (guide,
                step == 0 ? 55 : step == 1 ? 185 : 100,
                step == 0 ? 65 : step == 1 ? 165 : 205), ==, step);
            else if (step == 3)
              g_assert_true (gimp_perspective_guide_set_vanish_points (guide, 1, 155, 85));
            else if (step == 4)
              g_assert_true (gimp_perspective_guide_remove_vanish_points (guide, 0));
            else if (step == 5)
              {
                GimpPerspectiveGuide *replacement = gimp_perspective_guide_new (18);
                g_assert_cmpint (gimp_perspective_guide_add_vanish_points (replacement, 165, 190), ==, 0);
                gimp_image_set_perspective_guide (image, replacement);
                g_object_unref (replacement);
              }
            else gimp_image_set_perspective_guide (image, NULL);
            /* Capture incremental damage FIRST. A later full redraw is only an
             * independent reference and cannot repair the captured result. */
            cairo_surface_t *current_ink = perspective_damage_ink ();
            PerspectiveDamageClip clip = { cairo_region_create (), 0 };
            gulong clip_handler = g_signal_connect (shell->canvas, "draw",
              G_CALLBACK (perspective_damage_record_clip), &clip);
            GdkPixbuf *incremental = perspective_damage_capture ();
            g_signal_handler_disconnect (shell->canvas, clip_handler);
            guint changed = perspective_damage_difference (previous, incremental, previous_ink, current_ink);
            gtk_widget_queue_draw (shell->canvas);
            GdkPixbuf *reference = perspective_damage_capture ();
            guint residual = perspective_damage_difference (incremental, reference, previous_ink, current_ink);
            perspective_damage_global_audit (incremental, reference, previous_ink, current_ink, &clip);
            cairo_region_destroy (clip.region);
            g_test_message ("perspective damage angle=%g aniso=%u flip=%u step=%u changed=%u residual=%u",
                            angles[a], aniso, flip, step, changed, residual);
            if (residual && g_getenv ("GIMP_TEST_PERSPECTIVE_DAMAGE_DIR")) {
              const gchar *names[] = { "previous.png", "incremental.png", "reference.png" };
              GdkPixbuf *frames[] = { previous, incremental, reference };
              for (guint n = 0; n < 3; n++) {
                gchar *path = g_build_filename (g_getenv ("GIMP_TEST_PERSPECTIVE_DAMAGE_DIR"), names[n], NULL);
                GError *error = NULL; g_assert_true (gdk_pixbuf_save (frames[n], path, "png", &error, NULL));
                g_assert_no_error (error); g_free (path);
              }
            }
            g_assert_cmpuint (changed, >, 0);
            g_assert_cmpuint (residual, ==, 0);
            g_object_unref (previous); g_object_unref (incremental); previous = reference;
            cairo_surface_destroy (previous_ink); previous_ink = current_ink; transitions++;
          }
        g_object_unref (previous); cairo_surface_destroy (previous_ink); g_object_unref (guide); scenes++;
      }
  g_assert_cmpuint (scenes, ==, 8); g_assert_cmpuint (transitions, ==, 56);
  close_image ();
}

/* Every rasterized ink pixel must lie inside the region that the shell can
 * invalidate, including ellipse extrema rather than just point centers. */
void gimp_test_perspective_old_path (GimpDisplayShell *, GimpPerspectiveGuide *, GimpCanvasItem *, cairo_t *);
static void perspective_damage_ink_bounds_mode (gboolean compare_old_path)
{
  const gdouble angles[] = { 0, 37, 90 };
  const gdouble zooms[] = { 1, .25, 1.25, 4 };
  guint scenes = 0;
  create_image ();
  for (guint z = 0; z < G_N_ELEMENTS (zooms); z++)
    for (guint aniso = 0; aniso < 2; aniso++)
      for (guint a = 0; a < 3; a++)
        for (guint flip = 0; flip < 4; flip++)
          {
            GimpPerspectiveGuide *guide = gimp_perspective_guide_new (23);
            gimp_image_set_resolution (image, aniso ? 144 : 72, 72);
            gimp_display_shell_scale_set_dot_for_dot (shell, !aniso);
            gimp_display_shell_scale (shell, GIMP_ZOOM_TO, zooms[z], GIMP_ZOOM_FOCUS_IMAGE_CENTER);
            gimp_display_shell_flip (shell, flip & 1, flip & 2);
            gimp_display_shell_rotate_to (shell, angles[a]);
            gimp_display_shell_scroll_set_offset (shell,
              (gint) (128 * shell->scale_x) - shell->disp_width / 2,
              (gint) (128 * shell->scale_y) - shell->disp_height / 2);
            g_assert_true (gimp_display_shell_painter_pan_begin (shell, shell->disp_width / 2, shell->disp_height / 2));
            gimp_display_shell_painter_pan_motion (shell, shell->disp_width / 2 + .375, shell->disp_height / 2 - .625);
            gimp_image_set_perspective_guide (image, guide);
            for (guint count = 1; count <= 3; count++)
              {
                g_assert_cmpint (gimp_perspective_guide_add_vanish_points (guide,
                  count == 1 ? 110 : count == 2 ? 145 : 125,
                  count == 1 ? 115 : count == 2 ? 145 : 155), ==, count - 1);
                cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                                                        shell->disp_width, shell->disp_height);
                cairo_t *cr = cairo_create (surface);
                if (shell->rotate_transform) cairo_transform (cr, shell->rotate_transform);
                cairo_region_t *local = gimp_canvas_item_get_extents (shell->perspective_guide);
                cairo_region_t *device_region = cairo_region_create ();
                g_assert_nonnull (local);
                for (gint i = 0; i < cairo_region_num_rectangles (local); i++)
                  {
                    cairo_rectangle_int_t r, device;
                    gdouble minx = G_MAXDOUBLE, miny = G_MAXDOUBLE, maxx = -G_MAXDOUBLE, maxy = -G_MAXDOUBLE;
                    cairo_region_get_rectangle (local, i, &r);
                    for (guint corner = 0; corner < 4; corner++)
                      {
                        gdouble x = r.x + (corner & 1 ? r.width : 0), y = r.y + (corner & 2 ? r.height : 0);
                        cairo_user_to_device (cr, &x, &y);
                        minx = MIN (minx, x); miny = MIN (miny, y); maxx = MAX (maxx, x); maxy = MAX (maxy, y);
                      }
                    device.x = floor (minx - 1); device.y = floor (miny - 1);
                    device.width = ceil (maxx + 1) - device.x; device.height = ceil (maxy + 1) - device.y;
                    cairo_region_union_rectangle (device_region, &device);
                  }
                gimp_canvas_item_draw (shell->perspective_guide, cr);
                g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
                cairo_destroy (cr); cairo_surface_flush (surface);
                cairo_surface_t *expected = NULL;
                if (compare_old_path) {
                  expected = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, shell->disp_width, shell->disp_height);
                  cr = cairo_create (expected);
                  if (shell->rotate_transform) cairo_transform (cr, shell->rotate_transform);
                  gimp_test_perspective_old_path (shell, guide, shell->perspective_guide, cr);
                  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
                  cairo_destroy (cr); cairo_surface_flush (expected);
                }
                guint ink = 0, mismatched = 0;
                for (gint y = 0; y < shell->disp_height; y++)
                  for (gint x = 0; x < shell->disp_width; x++)
                    {
                      guint32 actual_pixel = ((guint32 *) (cairo_image_surface_get_data (surface) + y * cairo_image_surface_get_stride (surface)))[x];
                      if (actual_pixel >> 24) { ink++; g_assert_true (cairo_region_contains_point (device_region, x, y)); }
                      if (expected) {
                        guint32 old_pixel = ((guint32 *) (cairo_image_surface_get_data (expected) + y * cairo_image_surface_get_stride (expected)))[x];
                        gboolean differs = FALSE;
                        for (guint shift = 0; shift < 32; shift += 8)
                          if (ABS ((gint) ((actual_pixel >> shift) & 255) - (gint) ((old_pixel >> shift) & 255)) > 2) differs = TRUE;
                        if (differs) mismatched++;
                      }
                    }
                g_assert_cmpuint (ink, >, 0);
                g_test_message ("perspective ink zoom=%g aniso=%u angle=%g flip=%u points=%u pixels=%u",
                                zooms[z], aniso, angles[a], flip, count, ink);
                if (expected) {
                  g_test_message ("perspective old path mismatched=%u", mismatched);
                  g_assert_cmpuint (mismatched, ==, 0); cairo_surface_destroy (expected);
                }
                cairo_region_destroy (local); cairo_region_destroy (device_region); cairo_surface_destroy (surface); scenes++;
              }
            gimp_image_set_perspective_guide (image, NULL); g_object_unref (guide);
          }
  g_assert_cmpuint (scenes, ==, 288);
  close_image ();
}

static void perspective_damage_ink_bounds (void) { perspective_damage_ink_bounds_mode (FALSE); }
static void perspective_old_path_pixels (void) { perspective_damage_ink_bounds_mode (TRUE); }
