/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Additive audit-7/8 controls; include AFTER test-painter-right-docks.inc.
 * Every dock is the existing fixture's real window-owned native dock. No
 * attach/detach helper, private production struct, or fake widget is used. */
#include "widgets/gimpsessionmanaged.h"
#include "widgets/gimpsessioninfo-aux.h"
#include "widgets/gimpsessioninfo-book.h"
#include "widgets/gimpsessioninfo-dock.h"

typedef struct
{
  gint     span;
  gint     window_width, window_height;
  gint     hidden_left_span;
  gint     width;
  gint     divider;
  gboolean position_set;
  gchar   *semantics;
} RightDocksWidthSnapshot;

/* Read the native session-managed interface, and check that its native
 * serializer writes the same value. This does not save a user's profile. */
static gint
right_width_aux_value (RightDocksFixture *f,
                       const gchar       *name)
{
  GList *aux = gimp_session_managed_get_aux_info (GIMP_SESSION_MANAGED (f->image_window));
  GString *serialized = g_string_new (NULL);
  GimpConfigWriter *writer = gimp_config_writer_new_from_string (serialized);
  GError *error = NULL;
  gint value = 0;
  guint found = 0;
  gchar *needle;

  g_assert_nonnull (aux);
  for (GList *iter = aux; iter; iter = iter->next)
    {
      GimpSessionInfoAux *entry = iter->data;

      if (g_strcmp0 (entry->name, name) == 0)
        {
          gchar *end = NULL;
          gint64 parsed = g_ascii_strtoll (entry->value, &end, 10);

          g_assert_true (end != entry->value);
          g_assert_cmpstr (end, ==, "");
          g_assert_true (parsed >= G_MININT && parsed <= G_MAXINT);
          value = (gint) parsed;
          found++;
        }
    }
  g_assert_cmpuint (found, ==, 1);
  gimp_session_info_aux_serialize (writer, aux);
  g_assert_true (gimp_config_writer_finish (writer, NULL, &error));
  g_assert_no_error (error);
  needle = g_strdup_printf ("(%s \"%d\")", name, value);
  g_assert_nonnull (strstr (serialized->str, needle));
  g_free (needle);
  g_string_free (serialized, TRUE);
  g_list_free_full (aux, (GDestroyNotify) gimp_session_info_aux_free);
  return value;
}

static void
right_width_append_aux (GList      **aux,
                        const gchar *name,
                        gint         value)
{
  gchar *text = g_strdup_printf ("%d", value);

  *aux = g_list_append (*aux, gimp_session_info_aux_new (name, text));
  g_free (text);
}

static void
right_width_set_aux (RightDocksFixture *f,
                     const gchar       *right_name,
                     gint               right_value,
                     gint               left_value)
{
  GList *aux = NULL;

  if (left_value != G_MININT)
    right_width_append_aux (&aux, "left-docks-width", left_value);
  right_width_append_aux (&aux, right_name, right_value);
  /* set_aux_info also restores maximization; preserve its current value. */
  aux = g_list_append (aux, gimp_session_info_aux_new ("maximized",
                      gimp_image_window_is_maximized (f->image_window) ? "yes" : "no"));
  gimp_session_managed_set_aux_info (GIMP_SESSION_MANAGED (f->image_window), aux);
  g_list_free_full (aux, (GDestroyNotify) gimp_session_info_aux_free);
}

/* Serialize real native session records in their native order. Only dock and
 * book DIVIDER geometry is omitted from the disposable public session DTOs:
 * it can legitimately change with allocation. Dock type/side, book sequence,
 * selected page, tab sequence/identifier and tab session properties remain
 * native-serialized and must match byte for byte. No production state changes. */
static gchar *
right_width_session_semantics (RightDocksFixture *f)
{
  GString *text = g_string_new (NULL);
  GimpConfigWriter *writer = gimp_config_writer_new_from_string (text);
  GError *error = NULL;
  GList *docks = gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (f->columns));

  g_assert_nonnull (docks);
  for (GList *iter = docks; iter; iter = iter->next)
    {
      GimpSessionInfoDock *info = gimp_session_info_dock_from_widget (GIMP_DOCK (iter->data));

      g_assert_nonnull (info);
      g_assert_cmpint (info->side, ==, GIMP_ALIGN_RIGHT);
      info->position = 0;
      for (GList *book = info->books; book; book = book->next)
        ((GimpSessionInfoBook *) book->data)->position = 0;
      gimp_session_info_dock_serialize (writer, info);
      gimp_session_info_dock_free (info);
    }
  g_assert_true (gimp_config_writer_finish (writer, NULL, &error));
  g_assert_no_error (error);
  return g_string_free (text, FALSE);
}

static void
right_width_assert_state (RightDocksFixture            *f,
                          const RightDocksWidthSnapshot *saved,
                          gint                          expected_width)
{
  gchar *actual;

  right_docks_assert_parent (f);
  g_assert_cmpint (right_width_aux_value (f, "right-docks-width"), ==, expected_width);
  actual = right_width_session_semantics (f);
  g_assert_cmpstr (actual, ==, saved->semantics);
  g_free (actual);
}

/* Reuse the fixture's real after-paint geometry observer, without its
 * setup-only wrapper. These samples are ONLY a settling gate: they never
 * replace saved logical width/divider. A requested span change must occur
 * before the gate can pass. All calls deliberately select shell A. */
static void
right_width_settle (RightDocksFixture *f,
                    gint               different_from_span)
{
  RightDocksInitialGeometry geometry = { 0 };
  GdkFrameClock *clock;
  gulong handler;
  gint64 deadline;

  g_assert_true (gimp_image_window_get_active_shell (f->image_window) == f->shells[0]);
  right_docks_wait_drawable (GTK_WIDGET (f->image_window));
  right_docks_wait_drawable (f->home);
  right_docks_wait_drawable (f->columns);
  right_docks_wait_drawable (f->shells[0]->canvas);
  geometry.fixture = f;
  clock = gtk_widget_get_frame_clock (f->shells[0]->canvas);
  g_assert_nonnull (clock);
  g_object_ref (clock);
  handler = g_signal_connect (clock, "after-paint",
                              G_CALLBACK (right_docks_initial_frame_painted), &geometry);
  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while ((geometry.stable_frames < 3 ||
          (different_from_span >= 0 && geometry.allocations[1].width == different_from_span)) &&
         g_get_monotonic_time () < deadline)
    {
      gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_PAINT);
      gimp_test_run_temp_mainloop (10);
    }
  g_signal_handler_disconnect (clock, handler);
  g_object_unref (clock);
  g_assert_cmpuint (geometry.stable_frames, >=, 3);
  if (different_from_span >= 0)
    g_assert_cmpint (geometry.allocations[1].width, !=, different_from_span);
  g_test_message ("Width control settled after %u painted frames: home %d, divider %d",
                  geometry.samples, geometry.allocations[1].width, geometry.divider);
}

static void
right_width_begin (RightDocksFixture      *f,
                    RightDocksWidthSnapshot *saved)
{
  gint minimum;

  right_docks_begin (f);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (f->books[0]), 1);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (f->books[1]), 0);
  right_docks_settle_initial_geometry (f);
  /* Give logical width extra room before any transfer, making accidental
   * equality with the temporary overlay allocation an explicit test failure. */
  g_object_get (f->home, "min-position", &minimum, NULL);
  g_assert_cmpint (f->divider - minimum, >, 73);
  gtk_paned_set_position (GTK_PANED (f->home), f->divider - 73);
  right_docks_settle_initial_geometry (f);
  f->divider = gtk_paned_get_position (GTK_PANED (f->home));
  saved->divider = f->divider;
  saved->span = gtk_widget_get_allocated_width (f->home);
  saved->window_width = gtk_widget_get_allocated_width (GTK_WIDGET (f->image_window));
  saved->window_height = gtk_widget_get_allocated_height (GTK_WIDGET (f->image_window));
  GtkWidget *left = GTK_WIDGET (gimp_image_window_get_left_docks (f->image_window));
  g_assert_true (gtk_widget_get_visible (left));
  saved->hidden_left_span = gtk_widget_get_allocated_width (gtk_widget_get_parent (left)) - saved->span;
  g_assert_cmpint (saved->hidden_left_span, >, 0);
  saved->width = right_width_aux_value (f, "right-docks-width");
  g_assert_cmpint (saved->width, >, 0);
  g_assert_cmpint (saved->width, ==, saved->span - saved->divider);
  g_object_get (f->home, "position-set", &saved->position_set, NULL);
  saved->semantics = right_width_session_semantics (f);
  right_docks_assert_home (f);
}

static void
right_width_enable_and_switch (RightDocksFixture             *f,
                               const RightDocksWidthSnapshot *saved,
                               gint                          expected_width)
{
  right_docks_enable (f, 0);
  right_docks_assert_canvas (f, 0);
  right_width_settle (f, -1);
  g_assert_cmpint (gtk_widget_get_allocated_width (f->columns), !=, expected_width);
  right_width_assert_state (f, saved, expected_width);
  right_docks_select (f, 1);
  right_docks_assert_canvas (f, 1);
  right_width_assert_state (f, saved, expected_width);
  right_docks_select (f, 0);
  right_docks_assert_canvas (f, 0);
  right_width_settle (f, -1);
  right_width_assert_state (f, saved, expected_width);
}

/* Public GtkPaned limits are the only permitted clamp, never a tolerance
 * inferred from the observed divider. Unchanged-size/explicit aux tests
 * require an unclamped exact result; only the resize control permits minima. */
static void
right_width_assert_restored (RightDocksFixture             *f,
                             const RightDocksWidthSnapshot *saved,
                             gint                          expected_width,
                             gboolean                      expected_position_set,
                             gboolean                      allow_native_clamp)
{
  gboolean resize, shrink, position_set;
  gint span, minimum, maximum, desired, expected;

  g_assert_true (gtk_widget_get_parent (f->columns) == f->home);
  g_assert_true (gtk_paned_get_child1 (GTK_PANED (f->home)) == f->home_child1);
  g_assert_true (gtk_paned_get_child2 (GTK_PANED (f->home)) == f->columns);
  gtk_container_child_get (GTK_CONTAINER (f->home), f->columns,
                           "resize", &resize, "shrink", &shrink, NULL);
  g_assert_cmpint (resize, ==, f->resize);
  g_assert_cmpint (shrink, ==, f->shrink);
  g_object_get (f->home, "position-set", &position_set,
                "min-position", &minimum, "max-position", &maximum, NULL);
  g_assert_cmpint (position_set, ==, expected_position_set);
  span = gtk_widget_get_allocated_width (f->home);
  desired = span - expected_width;
  if (! allow_native_clamp)
    {
      g_assert_cmpint (desired, >=, minimum);
      g_assert_cmpint (desired, <=, maximum);
    }
  expected = CLAMP (desired, minimum, maximum);
  g_test_message ("Restore: home %d minus logical width %d = %d; native limits [%d, %d]; expected %d",
                  span, expected_width, desired, minimum, maximum, expected);
  g_assert_cmpint (gtk_paned_get_position (GTK_PANED (f->home)), ==, expected);
  right_width_assert_state (f, saved, span - expected);
}

static void
right_docks_session_width_unchanged_restore (void)
{
  RightDocksFixture f;
  RightDocksWidthSnapshot saved;

  right_width_begin (&f, &saved);
  right_width_enable_and_switch (&f, &saved, saved.width);
  /* Painter hides the left native column and its paned handle. The window
   * stays fixed; the right home grows by exactly that released allocation. */
  g_assert_cmpint (gtk_widget_get_allocated_width (GTK_WIDGET (f.image_window)), ==, saved.window_width);
  g_assert_cmpint (gtk_widget_get_allocated_height (GTK_WIDGET (f.image_window)), ==, saved.window_height);
  g_assert_false (gtk_widget_get_visible (GTK_WIDGET (gimp_image_window_get_left_docks (f.image_window))));
  g_assert_cmpint (gtk_widget_get_allocated_width (f.home), ==, saved.span + saved.hidden_left_span);
  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  right_width_settle (&f, -1);
  g_assert_cmpint (gtk_widget_get_allocated_width (f.home), ==, saved.span);
  right_docks_assert_home (&f);
  g_assert_cmpint (gtk_paned_get_position (GTK_PANED (f.home)), ==, saved.divider);
  right_width_assert_restored (&f, &saved, saved.width, saved.position_set, FALSE);
  g_free (saved.semantics);
  right_docks_end (&f);
}

static void
right_docks_session_width_resize_restore (void)
{
  RightDocksFixture f;
  RightDocksWidthSnapshot saved;
  gint window_width, window_height, minimum_width;
  gint borrowed_span;

  right_width_begin (&f, &saved);
  right_width_enable_and_switch (&f, &saved, saved.width);
  borrowed_span = gtk_widget_get_allocated_width (f.home);
  gtk_window_get_size (GTK_WINDOW (f.image_window), &window_width, &window_height);
  gtk_widget_get_preferred_width (GTK_WIDGET (f.image_window), &minimum_width, NULL);
  g_assert_cmpint (window_width - minimum_width, >, 113);
  gtk_window_resize (GTK_WINDOW (f.image_window), window_width - 113, window_height);
  right_width_settle (&f, borrowed_span);
  g_assert_cmpint (gtk_widget_get_allocated_width (f.home), !=, saved.span);
  right_docks_assert_canvas (&f, 0);
  right_width_assert_state (&f, &saved, saved.width);
  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  right_width_settle (&f, saved.span);
  right_width_assert_restored (&f, &saved, saved.width, saved.position_set, TRUE);
  g_free (saved.semantics);
  right_docks_end (&f);
}

static void
right_docks_session_aux_positive_width (void)
{
  RightDocksFixture f;
  RightDocksWidthSnapshot saved;
  gint chosen;

  right_width_begin (&f, &saved);
  right_width_enable_and_switch (&f, &saved, saved.width);
  chosen = saved.width + 29;
  right_width_set_aux (&f, "right-docks-width", chosen, G_MININT);
  right_width_assert_state (&f, &saved, chosen);
  right_docks_select (&f, 1);
  right_docks_assert_canvas (&f, 1);
  right_width_assert_state (&f, &saved, chosen);
  right_docks_select (&f, 0);
  right_docks_assert_canvas (&f, 0);
  right_width_assert_state (&f, &saved, chosen);
  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  right_width_settle (&f, -1);
  right_width_assert_restored (&f, &saved, chosen, TRUE, FALSE);
  g_free (saved.semantics);
  right_docks_end (&f);
}

static void
right_docks_session_aux_legacy_position (void)
{
  RightDocksFixture f;
  RightDocksWidthSnapshot saved;
  gint span_at_set, absolute_divider, expected_width;

  right_width_begin (&f, &saved);
  right_width_enable_and_switch (&f, &saved, saved.width);
  span_at_set = gtk_widget_get_allocated_width (f.home);
  absolute_divider = saved.divider - 41;
  g_assert_cmpint (absolute_divider, >, 0);
  expected_width = span_at_set - absolute_divider;
  g_assert_cmpint (expected_width, >, 0);
  g_assert_cmpint (expected_width, !=, absolute_divider);
  /* Positive legacy position is an absolute divider. The native parser
   * negates it internally; passing a pre-negated value would test width. */
  right_width_set_aux (&f, "right-docks-position", absolute_divider, G_MININT);
  right_width_assert_state (&f, &saved, expected_width);
  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  right_width_settle (&f, -1);
  g_assert_cmpint (gtk_widget_get_allocated_width (f.home), ==, span_at_set - saved.hidden_left_span);
  right_width_assert_restored (&f, &saved, expected_width, TRUE, FALSE);
  g_assert_cmpint (gtk_paned_get_position (GTK_PANED (f.home)), ==, absolute_divider - saved.hidden_left_span);
  g_free (saved.semantics);
  right_docks_end (&f);
}

typedef struct
{
  guint    allocations;
  guint    allocations_after_destroy;
  guint    positions_after_destroy;
  gboolean destroyed;
} RightDocksWidthPending;

static void
right_width_observe_allocate (GtkWidget              *widget,
                              GtkAllocation          *allocation,
                              RightDocksWidthPending *pending)
{
  (void) widget;
  (void) allocation;
  pending->allocations++;
  if (pending->destroyed)
    pending->allocations_after_destroy++;
}

static void
right_width_observe_destroy (GtkWidget              *widget,
                             RightDocksWidthPending *pending)
{
  (void) widget;
  pending->destroyed = TRUE;
}

static void
right_width_observe_position (GObject                *object,
                              GParamSpec             *pspec,
                              RightDocksWidthPending *pending)
{
  (void) object;
  (void) pspec;
  if (pending->destroyed)
    pending->positions_after_destroy++;
}

/* This entire helper deliberately performs NO main-context iteration.
 * Public allocation signals prove the old request is still unallocated.
 * The mode-off action restores to the real Paned and installs its native
 * pending restore; mode-on reborrows before that allocation can occur. */
static gint
right_width_pending_reborrow (RightDocksFixture             *f,
                              const RightDocksWidthSnapshot *saved,
                              RightDocksWidthPending        *pending,
                              gint                           newer_width)
{
  GtkWidget *left_columns = GTK_WIDGET (gimp_image_window_get_left_docks (f->image_window));
  GtkWidget *left_paned = gtk_widget_get_parent (left_columns);
  gint previous_left, chosen_left, maximum_left;
  gint previous_span = gtk_widget_get_allocated_width (f->home);

  g_assert_true (GTK_IS_PANED (left_paned));
  g_assert_true (gtk_paned_get_child2 (GTK_PANED (left_paned)) == f->home);
  previous_left = right_width_aux_value (f, "left-docks-width");
  g_assert_cmpint (previous_left, ==, gtk_paned_get_position (GTK_PANED (left_paned)));
  g_object_get (left_paned, "max-position", &maximum_left, NULL);
  chosen_left = previous_left + 37;
  g_assert_cmpint (chosen_left, <, maximum_left);
  g_signal_connect (f->home, "size-allocate", G_CALLBACK (right_width_observe_allocate), pending);
  g_signal_connect_after (f->home, "destroy", G_CALLBACK (right_width_observe_destroy), pending);
  g_signal_connect (f->home, "notify::position", G_CALLBACK (right_width_observe_position), pending);
  right_width_set_aux (f, "right-docks-width", saved->width + 19, chosen_left);
  g_assert_cmpint (right_width_aux_value (f, "left-docks-width"), ==, chosen_left);
  right_width_assert_state (f, saved, saved->width + 19);
  g_assert_cmpuint (pending->allocations, ==, 0);
  right_docks_action (f, "view", "view-painter-canvas-ui", FALSE);
  g_assert_true (gtk_widget_get_parent (f->columns) == f->home);
  g_assert_cmpuint (pending->allocations, ==, 0);
  right_width_assert_state (f, saved, saved->width + 19);
  right_docks_action (f, "view", "view-painter-canvas-ui", TRUE);
  g_assert_true (gtk_widget_is_ancestor (f->columns, f->shells[0]->canvas));
  g_assert_cmpuint (pending->allocations, ==, 0);
  right_width_set_aux (f, "right-docks-width", newer_width, G_MININT);
  right_width_assert_state (f, saved, newer_width);
  g_assert_cmpuint (pending->allocations, ==, 0);
  return previous_span;
}

static void
right_docks_session_aux_deferred_reborrow (void)
{
  RightDocksFixture f;
  RightDocksWidthSnapshot saved;
  RightDocksWidthPending pending = { 0 };
  gint previous_span, newer_width;

  right_width_begin (&f, &saved);
  right_width_enable_and_switch (&f, &saved, saved.width);
  newer_width = saved.width + 43;
  previous_span = right_width_pending_reborrow (&f, &saved, &pending, newer_width);
  /* Reborrow hides the left column again before allocation. Its new saved
   * width must not steal canvas space while Painter is enabled. */
  right_width_settle (&f, -1);
  g_assert_false (gtk_widget_get_visible (GTK_WIDGET (gimp_image_window_get_left_docks (f.image_window))));
  g_assert_cmpint (gtk_widget_get_allocated_width (f.home), ==, previous_span);
  g_assert_cmpuint (pending.allocations, >, 0);
  right_docks_assert_canvas (&f, 0);
  right_width_assert_state (&f, &saved, newer_width);
  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  right_width_settle (&f, -1);
  right_width_assert_restored (&f, &saved, newer_width, TRUE, FALSE);
  g_signal_handlers_disconnect_by_data (f.home, &pending);
  g_free (saved.semantics);
  right_docks_end (&f);
}

static void
right_docks_session_aux_deferred_close (void)
{
  RightDocksFixture f;
  RightDocksWidthSnapshot saved;
  RightDocksWidthPending pending = { 0 };
  gint newer_width;

  right_width_begin (&f, &saved);
  right_width_enable_and_switch (&f, &saved, saved.width);
  newer_width = saved.width + 47;
  right_width_pending_reborrow (&f, &saved, &pending, newer_width);
  /* Schedule the newer real home restore, then close all displays before
   * its first allocation. close_shell() intentionally drains, so use the
   * same public display deletion and image release without that drain. */
  right_docks_action (&f, "view", "view-painter-canvas-ui", FALSE);
  g_assert_true (gtk_widget_get_parent (f.columns) == f.home);
  right_width_assert_state (&f, &saved, newer_width);
  g_assert_cmpuint (pending.allocations, ==, 0);
  for (guint i = 0; i < G_N_ELEMENTS (f.shells); i++)
    if (f.shells[i])
      {
        GimpImage *image = gimp_display_get_image (f.shells[i]->display);

        gimp_display_delete (f.shells[i]->display);
        g_object_unref (image);
      }
  g_assert_cmpuint (pending.allocations, ==, 0);
  g_free (saved.semantics);
  /* Existing weak observers must clear: no extra test refs conceal an
   * orphan. Its drain also exposes any queued stale callback as a failure. */
  right_docks_end (&f);
  g_assert_true (pending.destroyed);
  g_assert_cmpuint (pending.allocations_after_destroy, ==, 0);
  g_assert_cmpuint (pending.positions_after_destroy, ==, 0);
}
