/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual-host controls for audit families 10--12. Include after the
 * native-dock fixture in test-painter-right-docks.inc. No private Canvas state, generic
 * attach helper, fabricated host, or direct shell tool-event call is used. */
#include <math.h>
#include "tools/tools-types.h"
#include "tools/tool_manager.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpwindowstrategy.h"
#include "core/gimp-gui.h"

typedef struct
{
  GtkWidget *widgets[4];          /* Weak: window, canvas, right root, options. */
  GtkAllocation previous[4];
  gdouble positions[4];
  guint samples;
  guint stable;
  gint aspect;                   /* -1 either, 0 landscape, 1 portrait. */
} RightHostGeometry;

typedef struct
{
  gdouble x, y, width, height;
} RightHostBounds;

static GimpOverlayChild *
right_host_child (GtkWidget *canvas,
                  GtkWidget *root)
{
  GimpOverlayChild *child;

  g_assert_true (gtk_widget_get_parent (root) == canvas);
  child = gimp_overlay_child_find (GIMP_OVERLAY_BOX (canvas), root);
  g_assert_nonnull (child);
  g_assert_true (child->widget == root);
  return child;
}

static void
right_host_frame (GdkFrameClock *clock,
                  gpointer       data)
{
  RightHostGeometry *g = data;
  gboolean same = g->samples > 0;

  (void) clock;
  for (guint i = 0; i < G_N_ELEMENTS (g->widgets); i++)
    {
      GtkAllocation a;
      GtkAllocation previous = g->previous[i];

      if (!g->widgets[i])
        { g->stable = 0; return; }
      gtk_widget_get_allocation (g->widgets[i], &a);
      same &= right_docks_drawable (g->widgets[i]) &&
              a.x == previous.x && a.y == previous.y &&
              a.width == previous.width && a.height == previous.height;
      g->previous[i] = a;
      if (i >= 2)
        {
          GimpOverlayChild *child = right_host_child (g->widgets[1], g->widgets[i]);
          guint p = 2 * (i - 2);

          /* A queued position request is not settled merely because the
           * previously applied matrix remained unchanged across paints. */
          same &= child->x == child->matrix.x0 &&
                  child->y == child->matrix.y0 &&
                  child->matrix.x0 == g->positions[p] &&
                  child->matrix.y0 == g->positions[p + 1];
          g->positions[p] = child->matrix.x0;
          g->positions[p + 1] = child->matrix.y0;
        }
    }
  if (g->aspect >= 0)
    same &= g->aspect ? g->previous[1].width < g->previous[1].height :
                        g->previous[1].width > g->previous[1].height;
  g->samples++;
  g->stable = same ? g->stable + 1 : 0;
}

/* This is a timing gate only. It does not resnapshot the frozen fixture's
 * expected home geometry, and does not hold strong widget references. */
static void
right_host_settle (RightDocksFixture *f,
                   guint              selected,
                   gint               aspect)
{
  RightHostGeometry g = { 0 };
  GdkFrameClock *clock;
  gulong handler;
  gint64 deadline;

  g.widgets[0] = GTK_WIDGET (f->image_window);
  g.widgets[1] = f->shells[selected]->canvas;
  g.widgets[2] = right_docks_overlay_root (f->columns, g.widgets[1]);
  g.widgets[3] = right_docks_overlay_root (right_docks_toolbar (f, selected), g.widgets[1]);
  g.aspect = aspect;
  for (guint i = 0; i < G_N_ELEMENTS (g.widgets); i++)
    g_object_add_weak_pointer (G_OBJECT (g.widgets[i]), (gpointer *) &g.widgets[i]);
  clock = gtk_widget_get_frame_clock (g.widgets[1]);
  g_assert_nonnull (clock);
  g_object_ref (clock);
  handler = g_signal_connect (clock, "after-paint", G_CALLBACK (right_host_frame), &g);
  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (g.stable < 3 && g_get_monotonic_time () < deadline)
    {
      gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_PAINT);
      gimp_test_run_temp_mainloop (10);
    }
  g_signal_handler_disconnect (clock, handler);
  g_object_unref (clock);
  for (guint i = 0; i < G_N_ELEMENTS (g.widgets); i++)
    {
      g_assert_nonnull (g.widgets[i]);
      g_object_remove_weak_pointer (G_OBJECT (g.widgets[i]), (gpointer *) &g.widgets[i]);
    }
  g_test_message ("Actual-host geometry: canvas %dx%d, %u stable of %u painted frames",
                  g.previous[1].width, g.previous[1].height, g.stable, g.samples);
  if (g.stable < 3)
    for (guint i = 2; i < G_N_ELEMENTS (g.widgets); i++)
      {
        GimpOverlayChild *child = right_host_child (g.widgets[1], g.widgets[i]);

        g_test_message ("Unsettled host %s: requested (%.17g, %.17g), applied (%.17g, %.17g)",
                        gtk_widget_get_name (g.widgets[i]), child->x, child->y,
                        child->matrix.x0, child->matrix.y0);
      }
  g_assert_cmpuint (g.stable, >=, 3);
}

static RightHostBounds
right_host_bounds (GtkWidget *canvas,
                   GtkWidget *root)
{
  GimpOverlayChild *child = right_host_child (canvas, root);
  RightHostBounds bounds;

  /* The native right/options hosts have an unrotated logical overlay matrix.
   * Their actual matrix, not image scale or device-pixel ratio, is measured. */
  g_assert_cmpfloat (child->matrix.xx, ==, 1.0);
  g_assert_cmpfloat (child->matrix.yy, ==, 1.0);
  g_assert_cmpfloat (child->matrix.xy, ==, 0.0);
  g_assert_cmpfloat (child->matrix.yx, ==, 0.0);
  bounds.x = child->matrix.x0;
  bounds.y = child->matrix.y0;
  bounds.width = gtk_widget_get_allocated_width (root);
  bounds.height = gtk_widget_get_allocated_height (root);
  g_assert_cmpfloat (child->x, ==, bounds.x);
  g_assert_cmpfloat (child->y, ==, bounds.y);
  g_assert_cmpfloat (bounds.width, >, 0);
  g_assert_cmpfloat (bounds.height, >, 0);
  return bounds;
}

static gdouble
right_host_distance_squared (RightHostBounds b,
                             gdouble         x,
                             gdouble         y)
{
  gdouble dx = MAX (MAX (b.x - x, 0), x - b.x - b.width);
  gdouble dy = MAX (MAX (b.y - y, 0), y - b.y - b.height);

  return dx * dx + dy * dy;
}

/* Search actual logical canvas coordinates, with a 20-pixel margin around
 * the 200-pixel retirement threshold. No fixed host vector index is used. */
static void
right_host_point (GtkWidget       *canvas,
                  RightHostBounds  target,
                  RightHostBounds  other,
                  gboolean         near_target,
                  gboolean         far_other,
                  gdouble         *x,
                  gdouble         *y)
{
  gint width = gtk_widget_get_allocated_width (canvas);
  gint height = gtk_widget_get_allocated_height (canvas);

  for (gint cy = 8; cy < height - 8; cy += 8)
    for (gint cx = 8; cx < width - 8; cx += 8)
      {
        gdouble d = right_host_distance_squared (target, cx, cy);
        if ((near_target ? d < 180.0 * 180.0 : d > 220.0 * 220.0) &&
            (!far_other || right_host_distance_squared (other, cx, cy) > 220.0 * 220.0))
          {
            *x = cx; *y = cy;
            g_test_message ("Logical point %.0f,%.0f: target distance %.2f, other %.2f",
                            *x, *y, sqrt (d),
                            sqrt (right_host_distance_squared (other, cx, cy)));
            return;
          }
      }
  g_error ("Actual canvas %dx%d has no requested near/far witness", width, height);
}

static void
right_host_assert_state (GtkWidget *canvas,
                         GtkWidget *root,
                         gdouble    opacity,
                         gboolean   pass_through)
{
  GimpOverlayChild *child = right_host_child (canvas, root);
  RightHostBounds b = right_host_bounds (canvas, root);
  gdouble x = MAX (0, b.x) + (MIN (gtk_widget_get_allocated_width (canvas), b.x + b.width) - MAX (0, b.x)) / 2;
  gdouble y = MAX (0, b.y) + (MIN (gtk_widget_get_allocated_height (canvas), b.y + b.height) - MAX (0, b.y)) / 2;

  g_assert_cmpfloat (x, >=, b.x);
  g_assert_cmpfloat (x, <, b.x + b.width);
  g_assert_cmpfloat (y, >=, b.y);
  g_assert_cmpfloat (y, <, b.y + b.height);
  g_assert_cmpfloat (child->opacity, ==, opacity);
  g_assert_cmpint (child->input_pass_through, ==, pass_through);
  g_assert_cmpint (gimp_overlay_box_get_child_input_pass_through (GIMP_OVERLAY_BOX (canvas), root), ==, pass_through);
  g_assert_cmpint (gimp_overlay_child_pick (GIMP_OVERLAY_BOX (canvas), child, x, y), ==, !pass_through);
}

/* Full GTK dispatch: the Canvas generic "event" connection is reached before
 * the ordinary type-specific native shell handler. Event-owned window and
 * device references are released by gdk_event_free; the widget lease spans
 * only the synchronous gtk_widget_event call. This does not simulate a
 * physical tablet, acquire a native painting grab, or send a tool stroke. */
static void
right_host_event (GtkWidget    *widget,
                  GdkEventType  type,
                  gdouble       x,
                  gdouble       y,
                  GdkNotifyType detail)
{
  GdkEvent *event = gdk_event_new (type);
  GdkWindow *event_window = gtk_widget_get_window (widget);
  GdkDevice *device;
  gint root_x = 0, root_y = 0;

  g_assert_nonnull (event_window);
  g_assert_true (gtk_widget_get_realized (widget));
  device = gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (widget)));
  g_assert_nonnull (device);
  event->any.window = g_object_ref (event_window);
  event->any.send_event = TRUE;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);
  gdk_window_get_origin (event_window, &root_x, &root_y);
  switch (type)
    {
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event->crossing.time = GDK_CURRENT_TIME;
      event->crossing.x = x; event->crossing.y = y;
      event->crossing.x_root = root_x + x; event->crossing.y_root = root_y + y;
      event->crossing.mode = GDK_CROSSING_NORMAL;
      event->crossing.detail = detail;
      event->crossing.focus = gtk_widget_has_focus (widget);
      break;
    case GDK_MOTION_NOTIFY:
      event->motion.time = GDK_CURRENT_TIME;
      event->motion.x = x; event->motion.y = y;
      event->motion.x_root = root_x + x; event->motion.y_root = root_y + y;
      event->motion.state = GDK_BUTTON1_MASK;
      event->motion.is_hint = FALSE;
      break;
    case GDK_BUTTON_RELEASE:
      event->button.time = GDK_CURRENT_TIME;
      event->button.x = x; event->button.y = y;
      event->button.x_root = root_x + x; event->button.y_root = root_y + y;
      event->button.button = 1;
      event->button.state = GDK_BUTTON1_MASK;
      break;
    case GDK_PROXIMITY_OUT:
      event->proximity.time = GDK_CURRENT_TIME;
      break;
    case GDK_GRAB_BROKEN:
      event->grab_broken.keyboard = FALSE;
      event->grab_broken.implicit = TRUE;
      event->grab_broken.grab_window = NULL;
      break;
    case GDK_FOCUS_CHANGE:
      /* Match real GTK state; a synthetic focus-out with has-focus TRUE
       * correctly triggers a native shell warning and is not a valid test. */
      g_assert_false (gtk_widget_has_focus (widget));
      event->focus_change.in = FALSE;
      break;
    default:
      g_assert_not_reached ();
    }
  g_object_ref (widget);
  gtk_widget_event (widget, event);
  g_object_unref (widget);
  gdk_event_free (event);
}

static GtkWidget *
right_host_find_type (GtkWidget *widget,
                      GType      type)
{
  GtkWidget *found = NULL;
  GList *children;

  if (g_type_is_a (G_OBJECT_TYPE (widget), type))
    return widget;
  if (!GTK_IS_CONTAINER (widget))
    return NULL;
  children = gtk_container_get_children (GTK_CONTAINER (widget));
  for (GList *iter = children; iter && !found; iter = iter->next)
    found = right_host_find_type (iter->data, type);
  g_list_free (children);
  return found;
}

/* Exercise existing native tabs and the real Layer tree's cursor after input
 * recovery; no private tree-view fields or fabricated controls are used. */
static void
right_host_native_controls (RightDocksFixture *f,
                            guint              selected)
{
  GtkNotebook *book = GTK_NOTEBOOK (f->books[0]);
  GtkWidget *tree;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *wanted, *actual = NULL;

  gtk_notebook_set_current_page (book, 1);
  drain ();
  g_assert_true (gtk_notebook_get_nth_page (book, gtk_notebook_get_current_page (book)) == f->dockables[1]);
  right_docks_wait_drawable (f->dockables[1]);
  gtk_notebook_set_current_page (book, 0);
  drain ();
  tree = right_host_find_type (f->dockables[0], GTK_TYPE_TREE_VIEW);
  g_assert_nonnull (tree);
  right_docks_wait_drawable (tree);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree));
  g_assert_nonnull (model);
  g_assert_true (gtk_tree_model_get_iter_first (model, &iter));
  wanted = gtk_tree_model_get_path (model, &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree), wanted, NULL, FALSE);
  gtk_tree_view_get_cursor (GTK_TREE_VIEW (tree), &actual, NULL);
  g_assert_nonnull (actual);
  g_assert_cmpint (gtk_tree_path_compare (wanted, actual), ==, 0);
  gtk_tree_path_free (wanted);
  gtk_tree_path_free (actual);
  right_docks_assert_canvas (f, selected);
  right_docks_assert_options (f, selected, 0, TRUE);
}

static void
right_docks_panel_hover (void)
{
  RightDocksFixture f;

  right_docks_begin (&f);
  right_docks_enable (&f, 0);
  for (guint cycle = 0; cycle < 3; cycle++)
    {
      guint selected = cycle % 2;
      GtkWidget *canvas, *root, *former;

      right_docks_select (&f, selected);
      right_host_settle (&f, selected, 0);
      canvas = f.shells[selected]->canvas;
      root = right_docks_overlay_root (f.columns, canvas);
      right_host_assert_state (canvas, root, .85, FALSE);
      right_host_event (root, GDK_ENTER_NOTIFY, 8, 8, GDK_NOTIFY_ANCESTOR);
      right_host_assert_state (canvas, root, 1.0, FALSE);
      right_host_event (root, GDK_LEAVE_NOTIFY, 8, 8, GDK_NOTIFY_ANCESTOR);
      right_host_assert_state (canvas, root, .85, FALSE);
      /* Inferior crossings are real wrapper event variants too. This records
       * the existing enter/leave contract, not a new subtree-hover policy. */
      right_host_event (root, GDK_ENTER_NOTIFY, 8, 8, GDK_NOTIFY_INFERIOR);
      right_host_assert_state (canvas, root, 1.0, FALSE);
      right_host_event (root, GDK_LEAVE_NOTIFY, 8, 8, GDK_NOTIFY_INFERIOR);
      right_host_assert_state (canvas, root, .85, FALSE);
      right_docks_assert_canvas (&f, selected);
      former = root;
      g_object_add_weak_pointer (G_OBJECT (former), (gpointer *) &former);
      right_docks_select (&f, 1 - selected);
      drain ();
      /* No test ref keeps a destroyed former wrapper/its callbacks alive. */
      g_assert_null (former);
    }
  right_docks_end (&f);
}

static void
right_docks_panel_retreat_input (void)
{
  RightDocksFixture f;
  GtkWidget *canvas, *right, *options;
  RightHostBounds r, o;
  gdouble x, y;

  right_docks_begin (&f);
  right_docks_enable (&f, 0);
  right_host_settle (&f, 0, 0);
  canvas = f.shells[0]->canvas;
  right = right_docks_overlay_root (f.columns, canvas);
  options = right_docks_overlay_root (right_docks_toolbar (&f, 0), canvas);
  g_assert_true (right != options);
  r = right_host_bounds (canvas, right);
  o = right_host_bounds (canvas, options);
  right_host_assert_state (canvas, right, .85, FALSE);
  right_host_assert_state (canvas, options, .85, FALSE);
  right_host_point (canvas, r, o, FALSE, TRUE, &x, &y);
  gimp_painter_canvas_ui_pointer (f.shells[0], x, y, FALSE);
  right_host_assert_state (canvas, right, .85, FALSE);
  right_host_assert_state (canvas, options, .85, FALSE);
  right_host_event (canvas, GDK_BUTTON_RELEASE, x, y, GDK_NOTIFY_ANCESTOR);
  right_host_point (canvas, r, o, TRUE, TRUE, &x, &y);
  gimp_painter_canvas_ui_pointer (f.shells[0], x, y, FALSE);
  right_host_assert_state (canvas, right, .1, TRUE);
  right_host_assert_state (canvas, options, .85, FALSE);
  right_host_event (canvas, GDK_BUTTON_RELEASE, x, y, GDK_NOTIFY_ANCESTOR);
  right_host_assert_state (canvas, right, .85, FALSE);
  right_host_point (canvas, o, r, TRUE, TRUE, &x, &y);
  gimp_painter_canvas_ui_pointer (f.shells[0], x, y, FALSE);
  right_host_assert_state (canvas, options, .1, TRUE);
  right_host_assert_state (canvas, right, .85, FALSE);
  right_host_event (canvas, GDK_BUTTON_RELEASE, x, y, GDK_NOTIFY_ANCESTOR);
  for (guint i = 0; i < 2; i++)
    {
      GtkWidget *root = i ? options : right;
      GimpOverlayBox *box = GIMP_OVERLAY_BOX (canvas);

      gimp_overlay_box_set_child_opacity (box, root, .1);
      right_host_assert_state (canvas, root, .1, FALSE);
      gimp_overlay_box_set_child_input_pass_through (box, root, TRUE);
      right_host_assert_state (canvas, root, .1, TRUE);
      gimp_overlay_box_set_child_opacity (box, root, .85);
      right_host_assert_state (canvas, root, .85, TRUE);
      gimp_overlay_box_set_child_input_pass_through (box, root, FALSE);
      right_host_assert_state (canvas, root, .85, FALSE);
    }
  right_host_native_controls (&f, 0);
  right_docks_end (&f);
}

typedef struct
{
  GdkEventType recovery;
  guint motions;
  guint recoveries;
} RightHostEventWitness;

static gboolean
right_host_observe_event (GtkWidget *widget,
                          GdkEvent  *event,
                          gpointer   data)
{
  RightHostEventWitness *witness = data;

  (void) widget;
  if (event->type == GDK_MOTION_NOTIFY)
    witness->motions++;
  if (event->type == witness->recovery)
    witness->recoveries++;
  return FALSE;
}

static void
right_host_recovery (GdkEventType recovery)
{
  RightDocksFixture f;

  right_docks_begin (&f);
  right_docks_enable (&f, 0);
  for (guint cycle = 0; cycle < 3; cycle++)
    {
      guint selected = cycle % 2;
      GtkWidget *canvas, *right, *options;
      RightHostBounds r, o;
      RightHostEventWitness witness = { recovery, 0, 0 };
      gdouble x, y;
      gulong handler;

      right_docks_select (&f, selected);
      right_host_settle (&f, selected, 0);
      canvas = f.shells[selected]->canvas;
      /* Clear actual focus before retirement; otherwise GTK's natural
       * focus-out could itself recover the panels before the target event. */
      gtk_window_set_focus (GTK_WINDOW (f.image_window), NULL);
      drain ();
      g_assert_false (gtk_widget_has_focus (canvas));
      right = right_docks_overlay_root (f.columns, canvas);
      options = right_docks_overlay_root (right_docks_toolbar (&f, selected), canvas);
      r = right_host_bounds (canvas, right);
      o = right_host_bounds (canvas, options);
      right_host_assert_state (canvas, right, .85, FALSE);
      right_host_assert_state (canvas, options, .85, FALSE);
      g_assert_null (f.shells[selected]->grab_seat);
      handler = g_signal_connect (canvas, "event", G_CALLBACK (right_host_observe_event), &witness);
      right_host_point (canvas, r, o, TRUE, TRUE, &x, &y);
      right_host_event (canvas, GDK_MOTION_NOTIFY, x, y, GDK_NOTIFY_ANCESTOR);
      right_host_assert_state (canvas, right, .1, TRUE);
      right_host_assert_state (canvas, options, .85, FALSE);
      right_host_point (canvas, o, r, TRUE, TRUE, &x, &y);
      right_host_event (canvas, GDK_MOTION_NOTIFY, x, y, GDK_NOTIFY_ANCESTOR);
      right_host_assert_state (canvas, right, .1, TRUE);
      right_host_assert_state (canvas, options, .1, TRUE);
      g_assert_cmpuint (witness.motions, ==, 2);
      g_assert_cmpuint (witness.recoveries, ==, 0);
      right_host_event (canvas, recovery, x, y, GDK_NOTIFY_ANCESTOR);
      g_assert_cmpuint (witness.recoveries, ==, 1);
      g_signal_handler_disconnect (canvas, handler);
      right_host_assert_state (canvas, right, .85, FALSE);
      right_host_assert_state (canvas, options, .85, FALSE);
      g_assert_false (gtk_widget_has_grab (canvas));
      g_assert_null (f.shells[selected]->grab_seat);
      right_host_native_controls (&f, selected);
    }
  right_docks_end (&f);
}

static void right_docks_panel_release_recovery (void)
{ right_host_recovery (GDK_BUTTON_RELEASE); }
static void right_docks_panel_proximity_recovery (void)
{ right_host_recovery (GDK_PROXIMITY_OUT); }
static void right_docks_panel_grab_recovery (void)
{ right_host_recovery (GDK_GRAB_BROKEN); }
static void right_docks_panel_focus_recovery (void)
{ right_host_recovery (GDK_FOCUS_CHANGE); }

/* Usability checks below distinguish an overflowing but genuinely scrollable
 * native control from gtk_widget_get_visible() on clipped content. They do
 * not certify typography, useful proportions, or visual parity. */
static GtkWidget *
right_host_find_property (GtkWidget  *widget,
                          const gchar *property)
{
  GtkWidget *found = NULL;
  GList *children;

  /* All callers seek actionable boolean controls. Expanding frames and
   * their GtkCheckButton labels carry the same native property tag; continue
   * through the frame instead of returning that non-actionable container. */
  if (GTK_IS_TOGGLE_BUTTON (widget) &&
      g_strcmp0 (g_object_get_data (G_OBJECT (widget), "gimp-widget-property-name"), property) == 0)
    return widget;
  if (GTK_IS_FRAME (widget) && gtk_frame_get_label_widget (GTK_FRAME (widget)))
    found = right_host_find_property (gtk_frame_get_label_widget (GTK_FRAME (widget)), property);
  if (found || !GTK_IS_CONTAINER (widget))
    return found;
  children = gtk_container_get_children (GTK_CONTAINER (widget));
  for (GList *iter = children; iter && !found; iter = iter->next)
    found = right_host_find_property (iter->data, property);
  g_list_free (children);
  return found;
}

/* Diagnostics only: inspect the existing native controls without changing
 * model values, widget sensitivity, parenting, focus or the acceptance gate. */
static void
right_host_trace_ancestry (const gchar *phase,
                           GtkWidget   *widget)
{
  for (GtkWidget *ancestor = widget; ancestor; ancestor = gtk_widget_get_parent (ancestor))
    {
      const gchar *property = g_object_get_data (G_OBJECT (ancestor), "gimp-widget-property-name");
      GtkWidget *label = GTK_IS_FRAME (ancestor) ? gtk_frame_get_label_widget (GTK_FRAME (ancestor)) : NULL;

      g_test_message ("HOST-DIAG %s ancestor=%p type=%s name=%s parent=%p "
                      "sensitive-local=%d sensitive-effective=%d visible-local=%d visible-effective=%d "
                      "mapped=%d realized=%d drawable=%d state=0x%x property=%s frame-label=%p target-is-label=%d",
                      phase, (void *) ancestor, G_OBJECT_TYPE_NAME (ancestor), gtk_widget_get_name (ancestor),
                      (void *) gtk_widget_get_parent (ancestor), gtk_widget_get_sensitive (ancestor),
                      gtk_widget_is_sensitive (ancestor), gtk_widget_get_visible (ancestor),
                      gtk_widget_is_visible (ancestor), gtk_widget_get_mapped (ancestor),
                      gtk_widget_get_realized (ancestor), gtk_widget_is_drawable (ancestor),
                      gtk_widget_get_state_flags (ancestor), property ? property : "(none)",
                      (void *) label, label == widget);
    }
}

static void
right_host_trace_native_state (RightDocksFixture *f,
                               guint              selected,
                               const gchar       *phase)
{
  const gchar *properties[] = { "use-texture", "use-smoothing", "expand-use" };
  GimpContext *context = gimp_get_user_context (gimp);
  GimpToolInfo *context_tool = gimp_context_get_tool (context);
  GimpTool *active_tool = tool_manager_get_active (gimp);
  GimpToolInfo *active_info = NULL;
  GimpImage *image = gimp_display_get_image (f->shells[selected]->display);
  GList *drawables = image ? gimp_image_get_selected_drawables (image) : NULL;

  if (active_tool)
    g_object_get (active_tool, "tool-info", &active_info, NULL);
  g_test_message ("HOST-DIAG %s selected=%u shell-display=%p context=%p context-display=%p "
                  "shell-image=%p context-image=%p expected-tool=%p/%s context-tool=%p/%s "
                  "active-tool=%p/%s active-info=%p/%s expected-options=%p active-options=%p gui=%p compact=%d",
                  phase, selected, (void *) f->shells[selected]->display, (void *) context,
                  (void *) gimp_context_get_display (context), (void *) image,
                  (void *) gimp_context_get_image (context), (void *) f->tools[0],
                  gimp_object_get_name (f->tools[0]), (void *) context_tool,
                  context_tool ? gimp_object_get_name (context_tool) : "(none)",
                  (void *) active_tool, active_tool ? G_OBJECT_TYPE_NAME (active_tool) : "(none)",
                  (void *) active_info, active_info ? gimp_object_get_name (active_info) : "(none)",
                  (void *) f->tools[0]->tool_options,
                  active_info ? (void *) active_info->tool_options : NULL, (void *) f->guis[0],
                  gimp_painter_compact_options_get (f->guis[0]));
  g_test_message ("HOST-DIAG %s layers=%d selected-layers=%u selected-channels=%u selected-drawables=%u",
                  phase, image ? gimp_image_get_n_layers (image) : -1,
                  image ? g_list_length (gimp_image_get_selected_layers (image)) : 0,
                  image ? g_list_length (gimp_image_get_selected_channels (image)) : 0,
                  g_list_length (drawables));
  for (GList *iter = drawables; iter; iter = iter->next)
    {
      GimpItem *item = GIMP_ITEM (iter->data);

      g_test_message ("HOST-DIAG %s drawable=%p type=%s name=%s visible=%d lock-content=%d lock-position=%d",
                      phase, (void *) item, G_OBJECT_TYPE_NAME (item), gimp_object_get_name (item),
                      gimp_item_get_visible (item), gimp_item_get_lock_content (item),
                      gimp_item_get_lock_position (item));
    }
  for (guint i = 0; i < G_N_ELEMENTS (properties); i++)
    {
      GtkWidget *target = right_host_find_property (f->guis[0], properties[i]);
      gboolean model = FALSE;

      g_object_get (f->tools[0]->tool_options, properties[i], &model, NULL);
      g_test_message ("HOST-DIAG %s property=%s model=%d target=%p type=%s active=%d bound-config=%p expected-config=%p",
                      phase, properties[i], model, (void *) target,
                      target ? G_OBJECT_TYPE_NAME (target) : "(none)",
                      GTK_IS_TOGGLE_BUTTON (target) ? gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target)) : -1,
                      target ? g_object_get_data (G_OBJECT (target), "gimp-widget-property-config") : NULL,
                      (void *) f->tools[0]->tool_options);
      if (target)
        right_host_trace_ancestry (phase, target);
    }
  g_list_free (drawables);
  if (active_info)
    g_object_unref (active_info);
}

static GtkScrolledWindow *
right_host_scroller (GtkWidget *root,
                     GtkWidget *content)
{
  GtkWidget *found = NULL;

  g_assert_true (gtk_widget_is_ancestor (content, root));
  for (GtkWidget *parent = gtk_widget_get_parent (content); parent != root;
       parent = gtk_widget_get_parent (parent))
    {
      g_assert_nonnull (parent);
      if (GTK_IS_SCROLLED_WINDOW (parent))
        found = parent;
    }
  g_assert_nonnull (found);
  g_assert_true (gtk_widget_get_parent (found) == root);
  return GTK_SCROLLED_WINDOW (found);
}

static void
right_host_intersect (RightHostBounds *a,
                      RightHostBounds  b)
{
  gdouble right = MIN (a->x + a->width, b.x + b.width);
  gdouble bottom = MIN (a->y + a->height, b.y + b.height);

  a->x = MAX (a->x, b.x);
  a->y = MAX (a->y, b.y);
  a->width = MAX (0, right - a->x);
  a->height = MAX (0, bottom - a->y);
}

static RightHostBounds
right_host_widget_bounds (GtkWidget *canvas,
                          GtkWidget *root,
                          GtkWidget *widget)
{
  RightHostBounds bounds = right_host_bounds (canvas, root);
  gint x, y;

  g_assert_true (gtk_widget_translate_coordinates (widget, root, 0, 0, &x, &y));
  bounds.x += x;
  bounds.y += y;
  bounds.width = gtk_widget_get_allocated_width (widget);
  bounds.height = gtk_widget_get_allocated_height (widget);
  return bounds;
}

static RightHostBounds
right_host_visible_bounds (GtkWidget *canvas,
                           GtkWidget *root,
                           GtkWidget *widget)
{
  RightHostBounds bounds = right_host_widget_bounds (canvas, root, widget);
  RightHostBounds canvas_bounds = { 0, 0, gtk_widget_get_allocated_width (canvas),
                                         gtk_widget_get_allocated_height (canvas) };

  /* Apply native viewport clipping as well as the actual root/canvas bounds.
   * A canonical GUI can have a large allocation outside this intersection. */
  for (GtkWidget *parent = gtk_widget_get_parent (widget); parent && parent != root;
       parent = gtk_widget_get_parent (parent))
    if (GTK_IS_VIEWPORT (parent) || GTK_IS_SCROLLED_WINDOW (parent))
      right_host_intersect (&bounds, right_host_widget_bounds (canvas, root, parent));
  right_host_intersect (&bounds, right_host_bounds (canvas, root));
  right_host_intersect (&bounds, canvas_bounds);
  return bounds;
}

/* Failure-only observations: do not move panels, change scrolling, or replace
 * the center-point acceptance gate. The grid distinguishes a covered center
 * from a control with no sampled usable input region. */
static void
right_host_trace_pick_failure (GtkWidget       *canvas,
                               GtkWidget       *root,
                               GtkWidget       *widget,
                               RightHostBounds  sampled,
                               GdkWindow       *picked)
{
  GimpOverlayChild *expected = right_host_child (canvas, root);
  RightHostBounds fresh = right_host_visible_bounds (canvas, root, widget);
  GList *children = gtk_container_get_children (GTK_CONTAINER (canvas));
  const gchar *property = g_object_get_data (G_OBJECT (widget), "gimp-widget-property-name");
  guint index = 0, hits = 0, patches = 0;
  gdouble first_x = -1, first_y = -1;
  gdouble patch_width = MIN (12, gtk_widget_get_allocated_width (widget));
  gdouble patch_height = MIN (12, gtk_widget_get_allocated_height (widget));

  g_test_message ("PICK-DIAG target=%p/%s/%s label=%s property=%s root=%p/%s "
                  "expected-window=%p picked-window=%p allocated=%dx%d "
                  "sampled=(%.2f,%.2f %.2fx%.2f) fresh=(%.2f,%.2f %.2fx%.2f)",
                  (void *) widget, G_OBJECT_TYPE_NAME (widget), gtk_widget_get_name (widget),
                  GTK_IS_LABEL (widget) ? gtk_label_get_text (GTK_LABEL (widget)) : "(none)",
                  property ? property : "(none)",
                  (void *) root, gtk_widget_get_name (root), (void *) expected->window, (void *) picked,
                  gtk_widget_get_allocated_width (widget), gtk_widget_get_allocated_height (widget),
                  sampled.x, sampled.y, sampled.width, sampled.height,
                  fresh.x, fresh.y, fresh.width, fresh.height);
  for (GList *iter = children; iter; iter = iter->next, index++)
    {
      GtkWidget *overlay = iter->data;
      GimpOverlayChild *child = gimp_overlay_child_find (GIMP_OVERLAY_BOX (canvas), overlay);
      GtkWidget *content = GTK_IS_BIN (overlay) ? gtk_bin_get_child (GTK_BIN (overlay)) : NULL;

      if (!child)
        continue;
      g_test_message ("PICK-DIAG overlay[%u]=%p/%s/%s content=%s/%s window=%p "
                      "visible=%d mapped=%d pass-through=%d opacity=%.2f "
                      "request=(%.2f,%.2f) matrix=(%.2f,%.2f) allocation=%dx%d "
                      "picks-sampled-center=%d expected=%d actual=%d",
                      index, (void *) overlay, G_OBJECT_TYPE_NAME (overlay), gtk_widget_get_name (overlay),
                      content ? G_OBJECT_TYPE_NAME (content) : "(none)",
                      content ? gtk_widget_get_name (content) : "(none)", (void *) child->window,
                      gtk_widget_get_visible (overlay), gtk_widget_get_mapped (overlay),
                      child->input_pass_through, child->opacity, child->x, child->y,
                      child->matrix.x0, child->matrix.y0, gtk_widget_get_allocated_width (overlay),
                      gtk_widget_get_allocated_height (overlay),
                      gimp_overlay_child_pick (GIMP_OVERLAY_BOX (canvas), child,
                                               sampled.x + sampled.width / 2,
                                               sampled.y + sampled.height / 2),
                      child == expected, child->window == picked);
    }
  g_list_free (children);
  for (guint row = 0; row < 9; row++)
    for (guint col = 0; col < 9; col++)
      {
        gdouble x = fresh.x + fresh.width * (col + .5) / 9;
        gdouble y = fresh.y + fresh.height * (row + .5) / 9;
        GdkWindow *hit = NULL;
        gboolean patch = fresh.width >= patch_width && fresh.height >= patch_height;
        gdouble left = CLAMP (x - patch_width / 2, fresh.x,
                               MAX (fresh.x, fresh.x + fresh.width - patch_width));
        gdouble top = CLAMP (y - patch_height / 2, fresh.y,
                              MAX (fresh.y, fresh.y + fresh.height - patch_height));

        g_signal_emit_by_name (gtk_widget_get_window (canvas), "pick-embedded-child", x, y, &hit);
        if (hit == expected->window)
          {
            hits++;
            if (first_x < 0) { first_x = x; first_y = y; }
          }
        for (guint py = 0; patch && py < 3; py++)
          for (guint px = 0; patch && px < 3; px++)
            {
              hit = NULL;
              g_signal_emit_by_name (gtk_widget_get_window (canvas), "pick-embedded-child",
                                     left + .5 + (patch_width - 1) * px / 2,
                                     top + .5 + (patch_height - 1) * py / 2, &hit);
              patch &= hit == expected->window;
            }
        patches += patch;
      }
  g_test_message ("PICK-DIAG bounded-grid expected-host=%u/81 first=(%.2f,%.2f) "
                  "usable-%.0fx%.0f-patches=%u/81",
                  hits, first_x, first_y, patch_width, patch_height, patches);
  right_host_trace_ancestry ("pick-failure-target", widget);
  if (g_getenv ("GIMP_TEST_HOST_FAILURE_SCREENSHOT"))
    {
      const gchar *path = g_getenv ("GIMP_TEST_HOST_FAILURE_SCREENSHOT");
      GdkWindow *screen_root = gdk_screen_get_root_window (gtk_widget_get_screen (canvas));
      GdkPixbuf *screenshot = gdk_pixbuf_get_from_window (screen_root, 0, 0,
                                                        gdk_window_get_width (screen_root),
                                                        gdk_window_get_height (screen_root));
      GError *error = NULL;

      if (screenshot && gdk_pixbuf_save (screenshot, path, "png", &error, NULL))
        g_test_message ("PICK-DIAG screenshot: %s", path);
      else
        g_test_message ("PICK-DIAG screenshot failed: %s",
                        error ? error->message : "root window capture unavailable");
      g_clear_error (&error);
      g_clear_object (&screenshot);
    }
}

static void
right_host_assert_reachable (GtkWidget *canvas,
                             GtkWidget *root,
                             GtkWidget *widget)
{
  RightHostBounds visible = right_host_visible_bounds (canvas, root, widget);
  GimpOverlayChild *child = right_host_child (canvas, root);
  GdkWindow *picked = NULL;
  gdouble x, y;

  right_docks_wait_drawable (widget);
  if (!gtk_widget_is_sensitive (widget))
    right_host_trace_ancestry ("failed-reachability-target", widget);
  g_assert_true (gtk_widget_is_sensitive (widget));
  g_assert_cmpfloat (visible.width, >=, MIN (12, gtk_widget_get_allocated_width (widget)));
  g_assert_cmpfloat (visible.height, >=, MIN (12, gtk_widget_get_allocated_height (widget)));
  x = visible.x + visible.width / 2;
  y = visible.y + visible.height / 2;
  g_assert_false (child->input_pass_through);
  g_assert_true (gimp_overlay_child_pick (GIMP_OVERLAY_BOX (canvas), child, x, y));
  /* Ask GdkWindow's native embedded-child picker, which uses real overlay
   * stacking/input semantics, rather than treating opacity as clickability. */
  g_signal_emit_by_name (gtk_widget_get_window (canvas), "pick-embedded-child", x, y, &picked);
  if (picked != child->window)
    right_host_trace_pick_failure (canvas, root, widget, visible, picked);
  g_assert_true (picked == child->window);
}

static void
right_host_reveal (RightDocksFixture *f,
                   guint              selected,
                   GtkWidget         *root,
                   GtkWidget         *content,
                   GtkWidget         *target)
{
  GtkScrolledWindow *scroll = right_host_scroller (root, content);
  GtkAdjustment *h = gtk_scrolled_window_get_hadjustment (scroll);
  GtkAdjustment *v = gtk_scrolled_window_get_vadjustment (scroll);
  gint x, y;

  g_assert_true (target == content || gtk_widget_is_ancestor (target, content));
  g_assert_true (gtk_widget_translate_coordinates (target, content, 0, 0, &x, &y));
  gtk_adjustment_clamp_page (h, x, x + gtk_widget_get_allocated_width (target));
  gtk_adjustment_clamp_page (v, y, y + gtk_widget_get_allocated_height (target));
  right_host_settle (f, selected, -1);
  right_host_assert_reachable (f->shells[selected]->canvas, root, target);
}

static void
right_host_assert_separate (GtkWidget *canvas,
                            GtkWidget *right,
                            GtkWidget *options)
{
  RightHostBounds r = right_host_bounds (canvas, right);
  RightHostBounds o = right_host_bounds (canvas, options);
  RightHostBounds overlap = r;
  RightHostBounds c = { 0, 0, gtk_widget_get_allocated_width (canvas),
                              gtk_widget_get_allocated_height (canvas) };

  g_assert_true (right != options);
  right_host_intersect (&r, c);
  right_host_intersect (&o, c);
  g_assert_cmpfloat (r.width, >, 0);
  g_assert_cmpfloat (r.height, >, 0);
  g_assert_cmpfloat (o.width, >, 0);
  g_assert_cmpfloat (o.height, >, 0);
  right_host_intersect (&overlap, o);
  g_test_message ("Right/options allocation intersection %.0fx%.0f logical pixels",
                  overlap.width, overlap.height);
  /* Require a usable region of each even if native minima cause partial
   * overlap. Actual control-point native picking below is the stronger gate. */
  g_assert_cmpfloat (overlap.width * overlap.height, <, r.width * r.height);
  g_assert_cmpfloat (overlap.width * overlap.height, <, o.width * o.height);
}

static void
right_host_dock_reachability (RightDocksFixture *f,
                             guint              selected)
{
  GtkWidget *canvas = f->shells[selected]->canvas;
  GtkWidget *root = right_docks_overlay_root (f->columns, canvas);

  for (guint i = 0; i < G_N_ELEMENTS (f->dockables); i++)
    {
      GtkNotebook *book = GTK_NOTEBOOK (f->books[i / 2]);
      GtkWidget *page = f->dockables[i];
      GtkWidget *tab;

      gtk_notebook_set_current_page (book, i % 2);
      right_host_settle (f, selected, -1);
      g_assert_true (gtk_notebook_get_nth_page (book, gtk_notebook_get_current_page (book)) == page);
      tab = gtk_notebook_get_tab_label (book, page);
      g_assert_nonnull (tab);
      right_host_reveal (f, selected, root, f->columns, tab);
      right_host_reveal (f, selected, root, f->columns, gtk_bin_get_child (GTK_BIN (page)));
    }
  right_docks_assert_contents (f);
}

static void
right_host_compact_end_reachability (RightDocksFixture *f,
                                    guint              selected,
                                    gboolean           require_overflow)
{
  const gchar *end_properties[] = { "use-texture", "use-smoothing", "expand-use" };
  GtkWidget *canvas = f->shells[selected]->canvas;
  GtkWidget *toolbar = right_docks_toolbar (f, selected);
  GtkWidget *root = right_docks_overlay_root (toolbar, canvas);
  GtkScrolledWindow *scroll = right_host_scroller (root, toolbar);
  GtkAdjustment *h = gtk_scrolled_window_get_hadjustment (scroll);
  GtkAdjustment *v = gtk_scrolled_window_get_vadjustment (scroll);
  gboolean clipped = FALSE;
  gdouble h_range = gtk_adjustment_get_upper (h) - gtk_adjustment_get_lower (h) - gtk_adjustment_get_page_size (h);
  gdouble v_range = gtk_adjustment_get_upper (v) - gtk_adjustment_get_lower (v) - gtk_adjustment_get_page_size (v);
  gdouble max_movement = 0;

  right_docks_assert_options (f, selected, 0, TRUE);
  gtk_adjustment_set_value (h, gtk_adjustment_get_lower (h));
  gtk_adjustment_set_value (v, gtk_adjustment_get_lower (v));
  right_host_settle (f, selected, -1);
  g_test_message ("Independent toolbar scroll ranges: horizontal %.0f, vertical %.0f; page %.0fx%.0f",
                  h_range, v_range, gtk_adjustment_get_page_size (h), gtk_adjustment_get_page_size (v));
  for (guint i = 0; i < G_N_ELEMENTS (end_properties); i++)
    {
      GtkWidget *target = right_host_find_property (toolbar, end_properties[i]);
      GtkWidget *native = right_host_find_property (f->guis[0], end_properties[i]);
      RightHostBounds before;
      gdouble old_h, old_v;
      gboolean active, model_active;

      g_assert_true (GTK_IS_TOGGLE_BUTTON (target));
      g_assert_true (GTK_IS_TOGGLE_BUTTON (native));
      g_assert_true (target != native);
      g_assert_true (g_object_get_data (G_OBJECT (target), "gimp-widget-property-config") ==
                     f->tools[0]->tool_options);
      gtk_adjustment_set_value (h, gtk_adjustment_get_lower (h));
      gtk_adjustment_set_value (v, gtk_adjustment_get_lower (v));
      right_host_settle (f, selected, -1);
      before = right_host_visible_bounds (canvas, root, target);
      clipped |= before.width < MIN (12, gtk_widget_get_allocated_width (target)) ||
                 before.height < MIN (12, gtk_widget_get_allocated_height (target));
      old_h = gtk_adjustment_get_value (h); old_v = gtk_adjustment_get_value (v);
      right_host_reveal (f, selected, root, toolbar, target);
      max_movement = MAX (max_movement, MAX (fabs (gtk_adjustment_get_value (h) - old_h),
                                            fabs (gtk_adjustment_get_value (v) - old_v)));
      /* The independent toolbar control and the untouched native dock control
       * must both follow the same options model, including the clipped end. */
      active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target));
      g_object_get (f->tools[0]->tool_options, end_properties[i], &model_active, NULL);
      g_assert_cmpint (model_active, ==, active);
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (native)), ==, active);
      gtk_button_clicked (GTK_BUTTON (target));
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target)), ==, !active);
      g_object_get (f->tools[0]->tool_options, end_properties[i], &model_active, NULL);
      g_assert_cmpint (model_active, ==, !active);
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (native)), ==, !active);
      gtk_button_clicked (GTK_BUTTON (target));
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target)), ==, active);
      g_object_get (f->tools[0]->tool_options, end_properties[i], &model_active, NULL);
      g_assert_cmpint (model_active, ==, active);
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (native)), ==, active);
    }
  if (require_overflow)
    {
      g_assert_cmpfloat (MAX (h_range, v_range), >, 0);
      g_assert_true (clipped);
      g_assert_cmpfloat (max_movement, >, 0);
    }
  /* The actual final scrollbar positions must also expose a bounded last
   * control. clamp_page above may stop before the full native range end. */
  gtk_adjustment_set_value (h, MAX (gtk_adjustment_get_lower (h), gtk_adjustment_get_upper (h) - gtk_adjustment_get_page_size (h)));
  gtk_adjustment_set_value (v, MAX (gtk_adjustment_get_lower (v), gtk_adjustment_get_upper (v) - gtk_adjustment_get_page_size (v)));
  right_host_settle (f, selected, -1);
  right_host_assert_reachable (canvas, root, right_host_find_property (toolbar, "expand-use"));
}

/* Native options retain their real editor, dockbook and scroller. Opening the
 * dialog does not move their children into the independent toolbar. */
static GtkWidget *
right_host_open_native_options (RightDocksFixture *f,
                                guint              selected)
{
  GtkWidget *toolbar = right_docks_toolbar (f, selected);
  GtkWidget *dockable = gimp_window_strategy_show_dockable_dialog (
    GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp,
    gimp_dialog_factory_get_singleton (),
    gimp_widget_get_monitor (GTK_WIDGET (f->image_window)), "gimp-tool-options");
  GtkWidget *editor;

  g_assert_true (GIMP_IS_DOCKABLE (dockable));
  right_host_settle (f, selected, -1);
  right_docks_wait_drawable (f->guis[0]);
  g_assert_true (gtk_widget_is_ancestor (f->guis[0], dockable));
  editor = gtk_widget_get_ancestor (f->guis[0], GIMP_TYPE_TOOL_OPTIONS_EDITOR);
  g_assert_nonnull (editor);
  g_assert_true (gimp_tool_options_editor_get_tool_options (GIMP_TOOL_OPTIONS_EDITOR (editor)) ==
                 f->tools[0]->tool_options);
  g_assert_true (right_docks_toolbar (f, selected) == toolbar);
  right_docks_assert_options (f, selected, 0, FALSE);
  return dockable;
}

/* Native windows use their ordinary allocation; an editor docked in the
 * on-canvas columns must additionally honor the actual overlay transform. */
static RightHostBounds
right_host_native_visible_bounds (RightDocksFixture *f,
                                   guint              selected,
                                   GtkWidget         *target)
{
  GtkWidget *canvas = f->shells[selected]->canvas;
  GtkWidget *toplevel = gtk_widget_get_toplevel (target);
  RightHostBounds visible;
  gint x, y;

  if (gtk_widget_is_ancestor (target, canvas))
    return right_host_visible_bounds (canvas, right_docks_overlay_root (target, canvas), target);
  g_assert_true (GTK_IS_WINDOW (toplevel));
  g_assert_true (gtk_widget_translate_coordinates (target, toplevel, 0, 0, &x, &y));
  visible = (RightHostBounds) { x, y, gtk_widget_get_allocated_width (target),
                                     gtk_widget_get_allocated_height (target) };
  for (GtkWidget *parent = gtk_widget_get_parent (target); parent;
       parent = gtk_widget_get_parent (parent))
    if (GTK_IS_VIEWPORT (parent) || GTK_IS_SCROLLED_WINDOW (parent) || parent == toplevel)
      {
        RightHostBounds clip;

        g_assert_true (gtk_widget_translate_coordinates (parent, toplevel, 0, 0, &x, &y));
        clip = (RightHostBounds) { x, y, gtk_widget_get_allocated_width (parent),
                                        gtk_widget_get_allocated_height (parent) };
        right_host_intersect (&visible, clip);
      }
  return visible;
}

static void
right_host_native_reveal (RightDocksFixture *f,
                          guint              selected,
                          GtkWidget         *target)
{
  GtkWidget *canvas = f->shells[selected]->canvas;
  GtkWidget *scroll = gtk_widget_get_ancestor (target, GTK_TYPE_SCROLLED_WINDOW);
  GtkWidget *viewport;
  GtkWidget *content;
  GtkAdjustment *h, *v;
  RightHostBounds visible;
  gint x, y;

  g_assert_nonnull (scroll);
  viewport = gtk_bin_get_child (GTK_BIN (scroll));
  g_assert_true (GTK_IS_VIEWPORT (viewport));
  content = gtk_bin_get_child (GTK_BIN (viewport));
  g_assert_nonnull (content);
  g_assert_true (gtk_widget_translate_coordinates (target, content, 0, 0, &x, &y));
  h = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  v = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroll));
  gtk_adjustment_clamp_page (h, x, x + gtk_widget_get_allocated_width (target));
  gtk_adjustment_clamp_page (v, y, y + gtk_widget_get_allocated_height (target));
  right_host_settle (f, selected, -1);
  right_docks_wait_drawable (target);
  if (gtk_widget_is_ancestor (target, canvas))
    {
      /* Reveal the whole native editor in the right-columns host too. Its
       * internal scroller and the outer overlay scroller are independent. */
      GtkWidget *root = right_docks_overlay_root (target, canvas);
      right_host_reveal (f, selected, root, f->columns, scroll);
      right_host_assert_reachable (canvas, root, target);
    }
  visible = right_host_native_visible_bounds (f, selected, target);
  g_assert_true (gtk_widget_is_sensitive (target));
  g_assert_cmpfloat (visible.width, >=, MIN (12, gtk_widget_get_allocated_width (target)));
  g_assert_cmpfloat (visible.height, >=, MIN (12, gtk_widget_get_allocated_height (target)));
}

static void
right_host_native_end_reachability (RightDocksFixture *f,
                                    guint              selected)
{
  const gchar *properties[] = { "use-texture", "use-smoothing", "expand-use" };
  GtkWidget *scroll = gtk_widget_get_ancestor (f->guis[0], GTK_TYPE_SCROLLED_WINDOW);
  GtkAdjustment *h, *v;
  gboolean clipped = FALSE;
  gdouble movement = 0;

  g_assert_nonnull (scroll);
  h = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (scroll));
  v = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroll));
  g_test_message ("Full native editor scroll ranges: horizontal %.0f, vertical %.0f",
                  gtk_adjustment_get_upper (h) - gtk_adjustment_get_page_size (h) - gtk_adjustment_get_lower (h),
                  gtk_adjustment_get_upper (v) - gtk_adjustment_get_page_size (v) - gtk_adjustment_get_lower (v));
  for (guint i = 0; i < G_N_ELEMENTS (properties); i++)
    {
      GtkWidget *target = right_host_find_property (f->guis[0], properties[i]);
      GtkWidget *peer = right_host_find_property (right_docks_toolbar (f, selected), properties[i]);
      RightHostBounds before;
      gboolean saved, model;
      gdouble old_h, old_v;

      g_assert_true (GTK_IS_TOGGLE_BUTTON (target));
      g_assert_true (GTK_IS_TOGGLE_BUTTON (peer));
      g_assert_true (target != peer);
      gtk_adjustment_set_value (h, gtk_adjustment_get_lower (h));
      gtk_adjustment_set_value (v, gtk_adjustment_get_lower (v));
      right_host_settle (f, selected, -1);
      before = right_host_native_visible_bounds (f, selected, target);
      clipped |= before.width < MIN (12, gtk_widget_get_allocated_width (target)) ||
                 before.height < MIN (12, gtk_widget_get_allocated_height (target));
      old_h = gtk_adjustment_get_value (h); old_v = gtk_adjustment_get_value (v);
      right_host_native_reveal (f, selected, target);
      movement = MAX (movement, MAX (fabs (gtk_adjustment_get_value (h) - old_h),
                                     fabs (gtk_adjustment_get_value (v) - old_v)));
      saved = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (target));
      gtk_button_clicked (GTK_BUTTON (target));
      g_object_get (f->tools[0]->tool_options, properties[i], &model, NULL);
      g_assert_cmpint (model, ==, !saved);
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peer)), ==, !saved);
      gtk_button_clicked (GTK_BUTTON (target));
      g_object_get (f->tools[0]->tool_options, properties[i], &model, NULL);
      g_assert_cmpint (model, ==, saved);
      g_assert_cmpint (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (peer)), ==, saved);
    }
  if (clipped)
    {
      g_assert_cmpfloat (MAX (gtk_adjustment_get_upper (h) - gtk_adjustment_get_page_size (h) - gtk_adjustment_get_lower (h),
                              gtk_adjustment_get_upper (v) - gtk_adjustment_get_page_size (v) - gtk_adjustment_get_lower (v)), >, 0);
      g_assert_cmpfloat (movement, >, 0);
    }
  right_host_native_reveal (f, selected, right_host_find_property (f->guis[0], "expand-use"));
}

/* The source uses the image-window aspect for dock orientation and the canvas
 * aspect for panel placement. A square window belongs to the horizontal-dock
 * branch even when its canvas has a different aspect. */
static void
right_host_assert_source_layout (RightDocksFixture *f,
                                 guint              selected)
{
  GtkWidget *canvas = f->shells[selected]->canvas;
  GtkWidget *right = right_docks_overlay_root (f->columns, canvas);
  GtkWidget *toolbar = right_docks_toolbar (f, selected);
  GtkWidget *options = right_docks_overlay_root (toolbar, canvas);
  GtkWidget *colors = find (canvas, "painter-color-editor");
  GtkWidget *swatches = find (canvas, "painter-fg-bg");
  RightHostBounds r = right_host_bounds (canvas, right);
  RightHostBounds o = right_host_bounds (canvas, options);
  gint w = gtk_widget_get_allocated_width (canvas);
  gint h = gtk_widget_get_allocated_height (canvas);
  gboolean portrait = w < h;
  gboolean horizontal = gtk_widget_get_allocated_width (GTK_WIDGET (f->image_window)) <=
                        gtk_widget_get_allocated_height (GTK_WIDGET (f->image_window));
  gint color_width, color_height, fg_width;
  gint request_width, request_height;
  gint expected_width = MAX (1, w * 98 / 100);
  gint expected_x = (w - expected_width) / 2;

  g_assert_nonnull (colors);
  g_assert_nonnull (swatches);
  colors = right_docks_overlay_root (colors, canvas);
  swatches = right_docks_overlay_root (swatches, canvas);
  color_width = MAX (256, gtk_widget_get_allocated_width (colors));
  color_height = MAX (256, gtk_widget_get_allocated_height (colors));
  fg_width = MAX (84, gtk_widget_get_allocated_width (swatches));
  for (GList *iter = gimp_dock_columns_get_docks (GIMP_DOCK_COLUMNS (f->columns)); iter; iter = iter->next)
    g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (iter->data)), ==,
                     horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
  g_assert_cmpint (gtk_orientable_get_orientation (GTK_ORIENTABLE (toolbar)), ==,
                   GTK_ORIENTATION_HORIZONTAL);
  g_assert_cmpfloat (r.x, ==, portrait ? 0 : w - 88 - color_width - 4);
  g_assert_cmpfloat (r.y, ==, portrait ? 0 : color_height + 4);
  gtk_widget_get_size_request (right, &request_width, &request_height);
  g_assert_cmpint (request_width, ==, portrait ? MAX (1, w - color_width - fg_width - 4) : color_width);
  g_assert_cmpint (request_height, ==, portrait ? color_height : MAX (1, h - (gint) r.y));
  if (!portrait && r.x < expected_x + expected_width)
    {
      expected_width = MAX (1, (gint) r.x - expected_x - 4);
      expected_x = ((gint) r.x - expected_width) / 2;
    }
  gtk_widget_get_size_request (options, &request_width, &request_height);
  g_assert_cmpint (request_width, ==, expected_width);
  g_assert_cmpint (request_height, ==, -1);
  g_assert_cmpfloat (o.x, ==, expected_x);
  g_assert_cmpfloat (o.y, ==, h - MAX (48, (gint) o.height) + 4);
}

/* Native GUI sensitivity and child ownership survive toolbar destruction and
 * recreation. Disabling one view does not disable the independent toolbar. */
static void
right_host_disabled_ancestor_roundtrip (RightDocksFixture *f,
                                        guint              selected,
                                        GtkWidget         *disabled_control)
{
  const gchar *properties[] = { "use-texture", "use-smoothing", "expand-use" };
  GtkWidget *gui = f->guis[0];
  GtkWidget *parent = gtk_widget_get_parent (gui);
  GtkWidget *old_toolbar = right_docks_toolbar (f, selected);
  GtkWidget *button;

  g_assert_true (gtk_widget_get_sensitive (gui));
  g_assert_false (gtk_widget_get_sensitive (disabled_control));
  gtk_widget_set_sensitive (gui, FALSE);
  g_object_add_weak_pointer (G_OBJECT (old_toolbar), (gpointer *) &old_toolbar);
  button = find (f->shells[selected]->canvas, "gimp-rect-select-tool");
  g_assert_true (GTK_IS_BUTTON (button));
  gtk_button_clicked (GTK_BUTTON (button));
  right_host_settle (f, selected, -1);
  right_docks_assert_options (f, selected, 1, TRUE);
  g_assert_null (old_toolbar);
  button = find (f->shells[selected]->canvas, "gimp-paintbrush-tool");
  g_assert_true (GTK_IS_BUTTON (button));
  gtk_button_clicked (GTK_BUTTON (button));
  right_host_settle (f, selected, -1);
  right_host_open_native_options (f, selected);
  g_assert_true (gui == f->guis[0]);
  g_assert_true (gtk_widget_get_parent (gui) == parent);
  g_assert_false (gtk_widget_get_sensitive (gui));
  g_assert_false (gtk_widget_is_sensitive (gui));
  for (guint i = 0; i < G_N_ELEMENTS (properties); i++)
    {
      GtkWidget *control = right_host_find_property (gui, properties[i]);
      GtkWidget *peer = right_host_find_property (right_docks_toolbar (f, selected), properties[i]);

      g_assert_true (GTK_IS_TOGGLE_BUTTON (control));
      g_assert_true (GTK_IS_TOGGLE_BUTTON (peer));
      g_assert_true (control != peer);
      g_assert_true (gtk_widget_get_sensitive (control));
      g_assert_false (gtk_widget_is_sensitive (control));
      g_assert_true (gtk_widget_is_sensitive (peer));
    }
  g_assert_true (right_host_find_property (gui, "brush-lock-to-view") == disabled_control);
  g_assert_false (gtk_widget_get_sensitive (disabled_control));
  g_assert_false (gtk_widget_is_sensitive (disabled_control));
  gtk_widget_set_sensitive (gui, TRUE);
  right_host_settle (f, selected, -1);
  for (guint i = 0; i < G_N_ELEMENTS (properties); i++)
    g_assert_true (gtk_widget_is_sensitive (right_host_find_property (gui, properties[i])));
  g_assert_false (gtk_widget_get_sensitive (disabled_control));
  g_assert_false (gtk_widget_is_sensitive (disabled_control));
  right_host_native_reveal (f, selected, right_host_find_property (gui, "expand-use"));
}

static void
right_host_layout_case (gint width,
                        gint height,
                        gint canvas_aspect)
{
  RightDocksFixture f;
  gint original_width, original_height;
  GtkWidget *disabled_control, *original_end_control, *original_end_parent;
  GtkWidget *native_parent;
  gboolean saved_sensitive;

  right_docks_begin (&f);
  right_host_trace_native_state (&f, 0, "native-before-toolbar");
  native_parent = gtk_widget_get_parent (f.guis[0]);
  disabled_control = right_host_find_property (f.guis[0], "brush-lock-to-view");
  g_assert_true (GTK_IS_TOGGLE_BUTTON (disabled_control));
  saved_sensitive = gtk_widget_get_sensitive (disabled_control);
  gtk_widget_set_sensitive (disabled_control, FALSE);
  original_end_control = right_host_find_property (f.guis[0], "expand-use");
  g_assert_true (GTK_IS_TOGGLE_BUTTON (original_end_control));
  original_end_parent = gtk_widget_get_parent (original_end_control);
  g_assert_true (GTK_IS_FRAME (original_end_parent));
  g_assert_true (gtk_frame_get_label_widget (GTK_FRAME (original_end_parent)) == original_end_control);
  gtk_window_get_size (GTK_WINDOW (f.image_window), &original_width, &original_height);
  right_docks_enable (&f, 0);
  gtk_window_resize (GTK_WINDOW (f.image_window), width, height);
  for (guint selected = 0; selected < 2; selected++)
    {
      GtkWidget *canvas, *right, *options, *end_control;
      gint actual_width, actual_height;

      right_docks_select (&f, selected);
      right_host_settle (&f, selected, canvas_aspect);
      gtk_window_get_size (GTK_WINDOW (f.image_window), &actual_width, &actual_height);
      g_test_message ("Requested image window %dx%d; actual %dx%d, allocation %dx%d, GTK scale %d",
                      width, height, actual_width, actual_height,
                      gtk_widget_get_allocated_width (GTK_WIDGET (f.image_window)),
                      gtk_widget_get_allocated_height (GTK_WIDGET (f.image_window)),
                      gtk_widget_get_scale_factor (GTK_WIDGET (f.image_window)));
      g_assert_cmpint (actual_width, ==, width);
      g_assert_cmpint (actual_height, ==, height);
      g_assert_cmpint (gtk_widget_get_allocated_width (GTK_WIDGET (f.image_window)), ==, width);
      g_assert_cmpint (gtk_widget_get_allocated_height (GTK_WIDGET (f.image_window)), ==, height);
      canvas = f.shells[selected]->canvas;
      right = right_docks_overlay_root (f.columns, canvas);
      options = right_docks_overlay_root (right_docks_toolbar (&f, selected), canvas);
      right_host_assert_source_layout (&f, selected);
      right_host_assert_separate (canvas, right, options);
      right_host_dock_reachability (&f, selected);
      right_host_compact_end_reachability (&f, selected, canvas_aspect == 0);
      right_host_open_native_options (&f, selected);
      right_host_trace_native_state (&f, selected, "native-coexists-with-toolbar");
      end_control = right_host_find_property (f.guis[0], "expand-use");
      g_assert_true (GTK_IS_TOGGLE_BUTTON (end_control));
      g_assert_true (end_control == original_end_control);
      g_assert_true (gtk_widget_get_parent (end_control) == original_end_parent);
      g_assert_true (gtk_frame_get_label_widget (GTK_FRAME (original_end_parent)) == end_control);
      g_assert_true (gtk_widget_get_parent (f.guis[0]) == native_parent);
      g_assert_true (right_host_find_property (f.guis[0], "brush-lock-to-view") == disabled_control);
      g_assert_false (gtk_widget_get_sensitive (disabled_control));
      g_assert_false (gtk_widget_is_sensitive (disabled_control));
      right_host_native_end_reachability (&f, selected);
      right_host_dock_reachability (&f, selected);
      if (selected == 0)
        right_host_disabled_ancestor_roundtrip (&f, selected, disabled_control);
      g_assert_true (right_host_find_property (f.guis[0], "expand-use") == original_end_control);
      g_assert_true (gtk_widget_get_parent (original_end_control) == original_end_parent);
      right_host_assert_source_layout (&f, selected);
      right_docks_assert_options (&f, selected, 0, TRUE);
      right_docks_assert_canvas (&f, selected);
    }
  right_docks_select (&f, 0);
  gtk_window_resize (GTK_WINDOW (f.image_window), original_width, original_height);
  right_host_settle (&f, 0, 0);
  right_docks_assert_canvas (&f, 0);
  right_docks_assert_options (&f, 0, 0, TRUE);
  /* Toolbar brush details are a fresh popup view of the same model. The
   * disabled native control remains in its original dock GUI throughout. */
  {
    GtkWidget *menu = find (right_docks_toolbar (&f, 0), "painter-paint-brush");
    GtkPopover *popup;
    GtkWidget *peer;

    g_assert_true (GTK_IS_MENU_BUTTON (menu));
    popup = gtk_menu_button_get_popover (GTK_MENU_BUTTON (menu));
    g_assert_true (GTK_IS_POPOVER (popup));
    g_assert_true (gtk_popover_get_relative_to (popup) == menu);
    peer = right_host_find_property (GTK_WIDGET (popup), "brush-lock-to-view");
    g_assert_true (GTK_IS_TOGGLE_BUTTON (peer));
    g_assert_true (peer != disabled_control);
    g_assert_true (g_object_get_data (G_OBJECT (peer), "gimp-widget-property-config") == f.tools[0]->tool_options);
    g_assert_true (gtk_widget_is_sensitive (peer));
    g_assert_true (right_host_find_property (f.guis[0], "brush-lock-to-view") == disabled_control);
  }
  g_assert_false (gtk_widget_get_sensitive (disabled_control));
  g_assert_false (gtk_widget_is_sensitive (disabled_control));
  gtk_widget_set_sensitive (disabled_control, saved_sensitive);
  right_docks_end (&f);
}

static void right_docks_controls_reachable_1240 (void)
{ right_host_layout_case (1240, 960, 0); }
static void right_docks_controls_reachable_1280 (void)
{
  right_host_layout_case (1280, 960, 0);
  right_host_layout_case (960, 960, -1);
}
static void right_docks_controls_reachable_portrait (void)
{ right_host_layout_case (900, 1200, 1); }
