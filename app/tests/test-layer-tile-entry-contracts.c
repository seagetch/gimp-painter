/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Actual-C11 tests of the original deferred LayerTileView construction route.
 * The original source names alias the single native GimpImageEditor widget;
 * these tests do not assert the old GtkBox subclass/binary ABI. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpitem.h"
#include "core/gimplayer-new.h"
#include "core/gimpundostack.h"
#include "widgets/gimplayertileview.h"
#include "widgets/gimppainterlayertiles.h"
#include "core/gimpdrawable.h"
#include "widgets/gimpuimanager.h"
#include "display/gimpdisplay.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "painter/gimp-painter-binding.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
#include "gimpcoreapp.h"

void gimp_test_layer_tile_entry_cpp_register (Gimp *application);

static Gimp *gimp;

typedef struct
{
  GtkWidget         *window;
  GimpLayerTileView *view;
} MountedView;

typedef struct
{
  void (*run) (void);
} EntryTestCase;

static void
run_entry_case (gconstpointer data)
{
  /* Each separately selected case must establish the normal first-focus
   * device restoration, just as the canvas fixture does. */
  gimp_set_focused_once (gimp);
  ((const EntryTestCase *) data)->run ();
}

static void
drain (void)
{
  for (guint i = 0; i < 300 && g_main_context_pending (NULL); i++)
    g_main_context_iteration (NULL, FALSE);
}

static GimpImage *
new_image (void)
{
  return gimp_image_new (gimp, 32, 32, GIMP_RGB,
                         GIMP_PRECISION_U8_NON_LINEAR);
}

static GimpLayer *
add_layer (GimpImage   *image,
           const gchar *name)
{
  GimpLayer *layer = gimp_layer_new (image, 32, 32,
                                    gimp_image_get_layer_format (image, TRUE),
                                    name, 1.0, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, layer, NULL, 0, FALSE);
  return layer;
}

static GimpContext *
new_context (const gchar *name,
             GimpImage   *image)
{
  GimpContext *context = gimp_context_new (gimp, name,
                                          gimp_get_user_context (gimp));
  gimp_context_set_display (context, NULL);
  gimp_context_set_image (context, image);
  return context;
}

static GimpLayerTileView *
new_owned_view (void)
{
  GimpLayerTileView *view = gimp_layer_tile_view_new ();
  g_assert_nonnull (view);
  g_assert_true (g_object_is_floating (view));
  g_object_ref_sink (view);
  return view;
}

static void
clear_view (GimpLayerTileView *view)
{
  gtk_widget_destroy (GTK_WIDGET (view));
  g_object_unref (view);
  drain ();
}

static MountedView
mount_view (GimpLayerTileView *view)
{
  MountedView result;
  result.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (result.window);
  gtk_window_set_default_size (GTK_WINDOW (result.window), 240, 480);
  result.view = view;
  gtk_container_add (GTK_CONTAINER (result.window), GTK_WIDGET (view));
  gtk_widget_show_all (result.window);
  drain ();
  return result;
}

static void
clear_mounted (MountedView *mounted)
{
  gtk_widget_destroy (mounted->window);
  g_object_unref (mounted->window);
  g_object_unref (mounted->view);
  mounted->view = NULL;
  mounted->window = NULL;
  drain ();
}

static GtkWidget *
find_widget (GtkWidget   *widget,
             const gchar *name)
{
  GtkWidget *result = NULL;
  if (g_strcmp0 (gtk_widget_get_name (widget), name) == 0)
    return widget;
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (GList *i = children; i && !result; i = i->next)
        result = find_widget (i->data, name);
      g_list_free (children);
    }
  return result;
}

static GtkWidget *
entry_canvas (GimpLayerTileView *view)
{
  GtkWidget *canvas = find_widget (GTK_WIDGET (view), "painter-layer-canvas");
  g_assert_true (GTK_IS_DRAWING_AREA (canvas));
  return canvas;
}

static gboolean
entry_button (GtkWidget *canvas, GdkEventType type, gdouble x, gdouble y)
{
  GdkEvent *event = gdk_event_new (type);
  event->button.window = g_object_ref (gtk_widget_get_window (canvas));
  event->button.send_event = TRUE;
  event->button.time = GDK_CURRENT_TIME;
  event->button.button = 1;
  event->button.x = x;
  event->button.y = y;
  gdk_event_set_device (event, gdk_seat_get_pointer (
    gdk_display_get_default_seat (gtk_widget_get_display (canvas))));
  gboolean handled = gtk_widget_event (canvas, event);
  gdk_event_free (event);
  return handled;
}

static cairo_surface_t *
entry_snapshot (GimpLayerTileView *view)
{
  GtkWidget *canvas = entry_canvas (view);
  gtk_container_check_resize (GTK_CONTAINER (gtk_widget_get_toplevel (canvas)));
  GtkAllocation allocation;
  gtk_widget_get_allocation (canvas, &allocation);
  g_assert_cmpint (allocation.width, >, 0);
  g_assert_cmpint (allocation.height, >, 0);
  cairo_surface_t *surface = cairo_image_surface_create (
    CAIRO_FORMAT_ARGB32, allocation.width, allocation.height);
  cairo_t *cr = cairo_create (surface);
  gtk_widget_draw (canvas, cr);
  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
  cairo_destroy (cr);
  cairo_surface_flush (surface);
  return surface;
}

static guint32
entry_pixel (cairo_surface_t *surface, gint x, gint y)
{
  g_assert_cmpint (x, <, cairo_image_surface_get_width (surface));
  g_assert_cmpint (y, <, cairo_image_surface_get_height (surface));
  return *(guint32 *) (cairo_image_surface_get_data (surface) +
                      y * cairo_image_surface_get_stride (surface) + x * 4);
}

static void
paint_layer (GimpLayer *layer, const gchar *name)
{
  GeglColor *color = gegl_color_new (name);
  gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (layer)), NULL, color);
  g_object_unref (color);
  gimp_drawable_update (GIMP_DRAWABLE (layer), 0, 0, -1, -1);
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (layer));
}

static void
assert_preview_color (GimpLayerTileView *view, gboolean red)
{
  gint64 deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  gboolean matched = FALSE;
  do
    {
      drain ();
      cairo_surface_t *surface = entry_snapshot (view);
      guint32 pixel = entry_pixel (surface, 28, 32);
      matched = red ? (((pixel >> 16) & 255) > (pixel & 255) + 100) :
                      ((pixel & 255) > ((pixel >> 16) & 255) + 100);
      cairo_surface_destroy (surface);
      if (!matched) g_usleep (1000);
    }
  while (!matched && g_get_monotonic_time () < deadline);
  g_assert_true (matched);
}

static void
assert_first_canvas_layer (GimpLayerTileView *view, GimpLayer *layer)
{
  GtkWidget *canvas = entry_canvas (view);
  g_assert_true (entry_button (canvas, GDK_BUTTON_PRESS, 28, 32));
  g_assert_true (entry_button (canvas, GDK_BUTTON_RELEASE, 28, 32));
  GList *selection = gimp_image_get_selected_layers (
    gimp_image_editor_get_image (GIMP_IMAGE_EDITOR (view)));
  g_assert_cmpuint (g_list_length (selection), ==, 1);
  g_assert_true (selection->data == layer);
}

static void
weak_finalized (gpointer data,
                GObject *object)
{
  (void) object;
  ++*(guint *) data;
}

static void
widget_storage_finalized (gpointer data)
{
  ++*(guint *) data;
}

static void
watch_widget_storage (GimpLayerTileView *view,
                      guint             *finalized)
{
  /* GtkWidget destroy runs disposal and may notify weak observers while an
   * explicit strong reference still retains valid, terminal widget storage.
   * This qdata notifier runs only when that final storage is released. */
  g_object_set_data_full (G_OBJECT (view), "layer-tile-entry-storage-finalized",
                          finalized, widget_storage_finalized);
}

static void
assert_properties (GimpLayerTileView *view,
                   GimpContext       *context,
                   GimpImage         *image)
{
  GimpContext *got_context = NULL;
  GimpImage   *got_image = NULL;
  g_object_get (view, "context", &got_context, "image", &got_image, NULL);
  g_assert_true (got_context == context);
  g_assert_true (got_image == image);
  g_assert_true (gimp_image_editor_get_image (GIMP_IMAGE_EDITOR (view)) == image);
  g_clear_object (&got_context);
  g_clear_object (&got_image);
}

static void
assert_action_context (GimpLayerTileView *view,
                       GimpContext       *source,
                       GimpImage         *image)
{
  GimpContext *context = action_data_get_context (view);
  if (source)
    {
      g_assert_nonnull (context);
      g_assert_true (context != source);
      g_assert_true (context == GIMP_IMAGE_EDITOR (view)->context);
      g_assert_true (gimp_context_get_parent (context) == source);
      g_assert_true (gimp_context_get_image (context) == image);
      g_assert_null (gimp_context_get_display (context));
    }
  else
    g_assert_null (context);
  g_assert_true (action_data_get_image (view) == image);
}

static void
c_factory_type_and_property_contract (void)
{
  GType (*type_provider) (void) = gimp_layer_tile_view_get_type;
  GimpLayerTileView *(*factory_provider) (void) = gimp_layer_tile_view_new;
  GimpLayerTileView *view = factory_provider ();
  GObjectClass *klass;
  GParamSpec *context_pspec, *image_pspec;
  guint finalized = 0;
  const GParamFlags required = G_PARAM_READWRITE | G_PARAM_CONSTRUCT;

  g_assert_nonnull (view);
  g_assert_true (type_provider () == GIMP_TYPE_PAINTER_LAYER_TILES);
  g_assert_true (G_OBJECT_TYPE (view) == type_provider ());
  g_assert_true (GIMP_IS_LAYER_TILE_VIEW (view));
  g_assert_true (GIMP_IS_PAINTER_LAYER_TILES (view));
  g_assert_true (g_type_parent (type_provider ()) == GIMP_TYPE_IMAGE_EDITOR);
  g_assert_true (GIMP_IS_LAYER_TILE_VIEW_CLASS (GIMP_LAYER_TILE_VIEW_GET_CLASS (view)));
  g_assert_true (g_object_is_floating (view));
  klass = G_OBJECT_GET_CLASS (view);
  context_pspec = g_object_class_find_property (klass, "context");
  image_pspec = g_object_class_find_property (klass, "image");
  g_assert_nonnull (context_pspec);
  g_assert_nonnull (image_pspec);
  g_assert_true (G_IS_PARAM_SPEC_OBJECT (context_pspec));
  g_assert_true (G_IS_PARAM_SPEC_OBJECT (image_pspec));
  g_assert_true (G_PARAM_SPEC_VALUE_TYPE (context_pspec) == GIMP_TYPE_CONTEXT);
  g_assert_true (G_PARAM_SPEC_VALUE_TYPE (image_pspec) == GIMP_TYPE_IMAGE);
  g_assert_cmpuint (context_pspec->flags & required, ==, required);
  g_assert_cmpuint (image_pspec->flags & required, ==, required);
  g_assert_false ((context_pspec->flags & G_PARAM_CONSTRUCT_ONLY) != 0);
  g_assert_false ((image_pspec->flags & G_PARAM_CONSTRUCT_ONLY) != 0);
  assert_properties (view, NULL, NULL);
  assert_action_context (view, NULL, NULL);
  g_assert_null (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (view), TRUE));
  watch_widget_storage (view, &finalized);
  g_object_ref_sink (view);
  clear_view (view);
  g_assert_cmpuint (finalized, ==, 1);
}

static void
c_construct_property_orders (void)
{
  GimpImage *active = new_image ();
  GimpImage *image = new_image ();
  GimpContext *context = new_context ("construction source", active);
  add_layer (image, "Constructed layer");

  /* Every combination of nullable arguments in both textual orders. GObject
   * may dispatch construct properties in its own order; neither route may
   * import the source context's unrelated active image. */
  for (guint order = 0; order < 2; order++)
    for (guint has_context = 0; has_context < 2; has_context++)
      for (guint has_image = 0; has_image < 2; has_image++)
        {
          GimpContext *expected_context = has_context ? context : NULL;
          GimpImage *expected_image = has_image ? image : NULL;
          GimpLayerTileView *view;
          if (order == 0)
            view = g_object_new (GIMP_TYPE_LAYER_TILE_VIEW,
                                 "context", expected_context,
                                 "image", expected_image, NULL);
          else
            view = g_object_new (GIMP_TYPE_LAYER_TILE_VIEW,
                                 "image", expected_image,
                                 "context", expected_context, NULL);
          g_assert_true (g_object_is_floating (view));
          g_object_ref_sink (view);
          assert_properties (view, expected_context, expected_image);
          assert_action_context (view, expected_context, expected_image);
          g_assert_true (gimp_context_get_image (context) == active);
          clear_view (view);
        }

  /* Omitting a construct argument must have the same nullable semantics. */
  GimpLayerTileView *view = g_object_new (GIMP_TYPE_LAYER_TILE_VIEW,
                                        "context", context, NULL);
  g_object_ref_sink (view);
  assert_properties (view, context, NULL);
  clear_view (view);
  view = g_object_new (GIMP_TYPE_LAYER_TILE_VIEW, "image", image, NULL);
  g_object_ref_sink (view);
  assert_properties (view, NULL, image);
  clear_view (view);
  g_object_unref (context);
  g_object_unref (active);
  g_object_unref (image);
}

static void
c_deferred_nullable_assignment_matrix (void)
{
  GimpImage *active = new_image ();
  GimpImage *images[] = { NULL, new_image (), new_image () };
  GimpContext *contexts[] = { NULL,
    new_context ("matrix source A", active),
    new_context ("matrix source B", active) };
  add_layer (images[1], "Image A");
  add_layer (images[2], "Image B");

  /* Exhaust all 9 initial states and all 9 target states in both operation
   * orders. This includes NULL, identity repeats, replacements and clears. */
  for (guint order = 0; order < 2; order++)
    for (guint old_context = 0; old_context < 3; old_context++)
      for (guint old_image = 0; old_image < 3; old_image++)
        for (guint next_context = 0; next_context < 3; next_context++)
          for (guint next_image = 0; next_image < 3; next_image++)
            {
              GimpLayerTileView *view = new_owned_view ();
              g_object_set (view, "context", contexts[old_context],
                             "image", images[old_image], NULL);
              if (order == 0)
                {
                  g_object_set (view, "context", contexts[next_context], NULL);
                  assert_properties (view, contexts[next_context], images[old_image]);
                  g_object_set (view, "image", images[next_image], NULL);
                }
              else
                {
                  g_object_set (view, "image", images[next_image], NULL);
                  assert_properties (view, contexts[old_context], images[next_image]);
                  g_object_set (view, "context", contexts[next_context], NULL);
                }
              assert_properties (view, contexts[next_context], images[next_image]);
              assert_action_context (view, contexts[next_context], images[next_image]);
              g_assert_true (gimp_context_get_image (contexts[1]) == active);
              g_assert_true (gimp_context_get_image (contexts[2]) == active);
              clear_view (view);
            }
  for (guint i = 1; i < 3; i++)
    {
      g_object_unref (contexts[i]);
      g_object_unref (images[i]);
    }
  g_object_unref (active);
}

static void
c_context_only_and_active_image_independence (void)
{
  GimpImage *active_a = new_image ();
  GimpImage *active_b = new_image ();
  GimpImage *bound = new_image ();
  GimpContext *context = new_context ("independent source", active_a);
  GimpLayerTileView *view = new_owned_view ();
  add_layer (bound, "Authoritative image");
  g_object_set (view, "context", context, NULL);
  assert_properties (view, context, NULL);
  assert_action_context (view, context, NULL);
  gimp_context_set_image (context, active_b);
  assert_properties (view, context, NULL);
  g_object_set (view, "image", bound, NULL);
  g_assert_true (gimp_context_get_image (context) == active_b);
  gimp_context_set_image (context, active_a);
  assert_properties (view, context, bound);
  assert_action_context (view, context, bound);
  g_object_set (view, "context", NULL, NULL);
  assert_properties (view, NULL, bound);
  gimp_context_set_image (context, active_b);
  assert_properties (view, NULL, bound);
  g_object_set (view, "context", context, NULL);
  assert_properties (view, context, bound);
  assert_action_context (view, context, bound);
  g_assert_true (gimp_context_get_image (context) == active_b);
  clear_view (view);
  g_object_unref (context);
  g_object_unref (active_a);
  g_object_unref (active_b);
  g_object_unref (bound);
}

static void
c_inherited_source_properties_stay_isolated (void)
{
  GimpLayerTileView *view = new_owned_view ();
  GimpImage *bound = new_image ();
  GimpImage *replacement = new_image ();
  GimpContext *ancestor, *source, *private_context;
  GimpDisplay *display;
  GimpImage *active;
  gimp_test_utils_create_image (gimp, 32, 32);
  drain ();
  display = gimp_context_get_display (gimp_get_user_context (gimp));
  g_assert_nonnull (display);
  active = gimp_display_get_image (display);
  g_assert_nonnull (active);
  ancestor = new_context ("source ancestor", active);
  gimp_context_set_display (ancestor, display);
  source = new_context ("inherited source", NULL);
  gimp_context_define_properties (source,
                                  GIMP_CONTEXT_PROP_MASK_IMAGE |
                                  GIMP_CONTEXT_PROP_MASK_DISPLAY, FALSE);
  gimp_context_set_parent (source, ancestor);
  g_assert_false (gimp_context_property_defined (source, GIMP_CONTEXT_PROP_IMAGE));
  g_assert_false (gimp_context_property_defined (source, GIMP_CONTEXT_PROP_DISPLAY));
  g_assert_true (gimp_context_get_image (source) == active);
  g_assert_true (gimp_context_get_display (source) == display);

  g_object_set (view, "image", bound, "context", source, NULL);
  assert_properties (view, source, bound);
  assert_action_context (view, source, bound);
  private_context = action_data_get_context (view);
  g_assert_true (gimp_context_property_defined (private_context, GIMP_CONTEXT_PROP_IMAGE));
  g_assert_true (gimp_context_property_defined (private_context, GIMP_CONTEXT_PROP_DISPLAY));
  g_object_set (view, "image", replacement, NULL);
  assert_properties (view, source, replacement);
  g_assert_true (gimp_context_get_image (source) == active);
  g_assert_true (gimp_context_get_image (ancestor) == active);
  g_assert_true (gimp_context_get_display (source) == display);
  g_assert_true (gimp_context_get_display (ancestor) == display);
  g_assert_false (gimp_context_property_defined (source, GIMP_CONTEXT_PROP_IMAGE));
  g_assert_false (gimp_context_property_defined (source, GIMP_CONTEXT_PROP_DISPLAY));
  g_object_set (view, "context", NULL, NULL);
  assert_properties (view, NULL, replacement);
  g_assert_true (gimp_context_get_image (source) == active);
  g_assert_true (gimp_context_get_image (ancestor) == active);
  g_assert_true (gimp_context_get_display (source) == display);
  g_assert_true (gimp_context_get_display (ancestor) == display);
  clear_view (view);
  g_object_unref (source);
  g_object_unref (ancestor);
  gimp_display_delete (display);
  g_object_unref (active);
  drain ();
  g_object_unref (bound);
  g_object_unref (replacement);
}

static void
c_exact_object_getter_reference_lifetimes (void)
{
  GimpLayerTileView *view = new_owned_view ();
  GimpContext *context = new_context ("owned source", NULL);
  GimpImage *image = new_image ();
  GimpContext *first_context = NULL, *second_context = NULL;
  GimpImage *first_image = NULL, *second_image = NULL;
  guint contexts_finalized = 0, images_finalized = 0;
  g_object_weak_ref (G_OBJECT (context), weak_finalized, &contexts_finalized);
  g_object_weak_ref (G_OBJECT (image), weak_finalized, &images_finalized);
  g_object_set (view, "context", context, "image", image, NULL);
  g_object_unref (context);
  g_object_unref (image);
  g_assert_cmpuint (contexts_finalized, ==, 0);
  g_assert_cmpuint (images_finalized, ==, 0);
  g_object_get (view, "context", &first_context, "image", &first_image, NULL);
  g_object_get (view, "context", &second_context, "image", &second_image, NULL);
  g_assert_true (first_context == context);
  g_assert_true (second_context == context);
  g_assert_true (first_image == image);
  g_assert_true (second_image == image);
  g_assert_true (first_context != GIMP_IMAGE_EDITOR (view)->context);

  /* Clearing widget ownership leaves two independent caller-owned references. */
  g_object_set (view, "context", NULL, "image", NULL, NULL);
  assert_properties (view, NULL, NULL);
  clear_view (view);
  g_assert_cmpuint (contexts_finalized, ==, 0);
  g_assert_cmpuint (images_finalized, ==, 0);
  g_object_unref (first_context);
  g_object_unref (first_image);
  g_assert_cmpuint (contexts_finalized, ==, 0);
  g_assert_cmpuint (images_finalized, ==, 0);
  g_object_unref (second_context);
  g_object_unref (second_image);
  drain ();
  g_assert_cmpuint (contexts_finalized, ==, 1);
  g_assert_cmpuint (images_finalized, ==, 1);
}

static void
c_replacement_and_destroy_release_inputs (void)
{
  for (guint destroy_first = 0; destroy_first < 2; destroy_first++)
    {
      GimpLayerTileView *view = new_owned_view ();
      guint contexts_finalized = 0, images_finalized = 0, views_finalized = 0;
      guint views_disposed = 0;
      watch_widget_storage (view, &views_finalized);
      g_object_weak_ref (G_OBJECT (view), weak_finalized, &views_disposed);
      for (guint i = 0; i < 16; i++)
        {
          GimpContext *context = new_context ("replacement source", NULL);
          GimpImage *image = new_image ();
          g_object_weak_ref (G_OBJECT (context), weak_finalized, &contexts_finalized);
          g_object_weak_ref (G_OBJECT (image), weak_finalized, &images_finalized);
          g_object_set (view, "image", image, "context", context, NULL);
          assert_properties (view, context, image);
          g_object_unref (context);
          g_object_unref (image);
          drain ();
          g_assert_cmpuint (contexts_finalized, ==, i);
          g_assert_cmpuint (images_finalized, ==, i);
        }
      if (destroy_first)
        {
          gtk_widget_destroy (GTK_WIDGET (view));
          drain ();
          g_assert_cmpuint (contexts_finalized, ==, 16);
          g_assert_cmpuint (images_finalized, ==, 16);
          g_assert_cmpuint (views_disposed, ==, 1);
          g_assert_cmpuint (views_finalized, ==, 0);
          assert_properties (view, NULL, NULL);
          g_object_unref (view);
        }
      else
        g_object_unref (view);
      drain ();
      g_assert_cmpuint (contexts_finalized, ==, 16);
      g_assert_cmpuint (images_finalized, ==, 16);
      g_assert_cmpuint (views_disposed, ==, 1);
      g_assert_cmpuint (views_finalized, ==, 1);
    }
}

static void
c_same_image_context_refreshes_previews (void)
{
  GimpImage *image = new_image ();
  GimpLayer *layer = add_layer (image, "Preview context");
  GimpContext *a = new_context ("preview source A", NULL);
  GimpContext *b = new_context ("preview source B", NULL);
  GimpLayerTileView *view = new_owned_view ();
  paint_layer (layer, "red");
  g_object_set (view, "context", a, "image", image, NULL);
  MountedView mounted = mount_view (view);
  assert_preview_color (view, TRUE);
  GimpContext *previous_action = GIMP_IMAGE_EDITOR (view)->context;
  gpointer weak_action = previous_action;
  g_object_add_weak_pointer (G_OBJECT (previous_action), &weak_action);
  /* Queue an old-generation update, then replace its context before the idle. */
  paint_layer (layer, "blue");
  g_object_set (view, "context", b, NULL);
  drain ();
  g_assert_null (weak_action);
  assert_properties (view, b, image);
  assert_action_context (view, b, image);
  assert_preview_color (view, FALSE);
  gimp_viewable_invalidate_preview (GIMP_VIEWABLE (layer));
  g_object_set (view, "context", NULL, NULL);
  drain ();
  assert_properties (view, NULL, image);
  /* Context removal releases cached pixels but preserves the row model. */
  cairo_surface_t *blank = entry_snapshot (view);
  guint32 pixel = entry_pixel (blank, 28, 32);
  g_assert_cmpuint ((pixel >> 16) & 255, ==, pixel & 255);
  cairo_surface_destroy (blank);
  paint_layer (layer, "red");
  drain ();
  blank = entry_snapshot (view);
  pixel = entry_pixel (blank, 28, 32);
  g_assert_cmpuint ((pixel >> 16) & 255, ==, pixel & 255);
  cairo_surface_destroy (blank);
  g_object_set (view, "context", a, NULL);
  assert_preview_color (view, TRUE);
  assert_action_context (view, a, image);
  assert_first_canvas_layer (view, layer);
  clear_mounted (&mounted);
  g_object_unref (a);g_object_unref (b);g_object_unref (image);
}

static void
send_button (GtkWidget   *widget,
             GdkEventType type)
{
  GdkEventButton event = { 0 };
  gboolean handled = FALSE;
  event.type = type;
  event.button = 1;
  event.x = 8;
  event.y = 8;
  g_signal_emit_by_name (widget,
                         type == GDK_BUTTON_PRESS ? "button-press-event" :
                                                    "button-release-event",
                         &event, &handled);
}

static void
c_contextless_image_actions_inert (void)
{
  GimpImage *image = new_image ();
  GimpLayer *layer = add_layer (image, "No action context");
  GimpLayerTileView *view = new_owned_view ();
  g_object_set (view, "image", image, NULL);
  MountedView mounted = mount_view (view);
  gint undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image));
  GtkWidget *add = find_widget (GTK_WIDGET (view), "painter-layer-add");
  GtkWidget *visible = entry_canvas (view);
  g_assert_nonnull (add);
  g_assert_nonnull (visible);
  g_assert_null (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (view), FALSE));
  g_assert_null (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (view), TRUE));
  g_assert_true (entry_button (visible, GDK_BUTTON_PRESS, 8, 8));
  send_button (add, GDK_BUTTON_PRESS);
  send_button (add, GDK_BUTTON_RELEASE);
  drain ();
  g_assert_true (gimp_item_get_visible (GIMP_ITEM (layer)));
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 1);
  g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)), ==, undo_depth);
  assert_properties (view, NULL, image);
  assert_action_context (view, NULL, image);
  g_assert_false (gimp_painter_layer_tiles_long_press_pending (GIMP_PAINTER_LAYER_TILES (view)));
  clear_mounted (&mounted);
  g_object_unref (image);
}

/* Reentry callbacks are one-shot; a later notification caused by the newer
 * assignment is deliberately accepted, not recursively rewritten. */
typedef struct
{
  GimpContext *context;
  GimpImage   *image;
  GimpContext *intermediate_context;
  GimpImage   *intermediate_image;
  guint       calls;
  gboolean    destroy;
} Reentry;

static void
apply_reentry (GimpLayerTileView *view,
               Reentry          *reentry)
{
  if (reentry->calls++)
    return;
  if (reentry->destroy)
    gtk_widget_destroy (GTK_WIDGET (view));
  else
    {
      if (reentry->intermediate_context || reentry->intermediate_image)
        g_object_set (view,
                       "context", reentry->intermediate_context,
                       "image", reentry->intermediate_image, NULL);
      g_object_set (view, "context", reentry->context,
                     "image", reentry->image, NULL);
    }
}

static void
reenter_notify (GObject    *object,
                GParamSpec *pspec,
                gpointer    data)
{
  (void) pspec;
  apply_reentry (GIMP_LAYER_TILE_VIEW (object), data);
}

static void
reenter_popup_closed (GtkWidget *widget,
                      gpointer   data)
{
  apply_reentry (GIMP_LAYER_TILE_VIEW (widget), data);
}

static void
assert_closed (GimpLayerTileView *view)
{
  assert_properties (view, NULL, NULL);
  g_assert_null (action_data_get_context (view));
  g_assert_null (action_data_get_image (view));
  g_assert_null (action_data_get_display (view));
  g_assert_null (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (view), TRUE));
  g_assert_false (gimp_painter_layer_tiles_long_press_pending (GIMP_PAINTER_LAYER_TILES (view)));
}

static void
c_property_notifications_newer_assignment (void)
{
  GimpImage *images[] = { new_image (), new_image (), new_image () };
  GimpContext *contexts[] = {
    new_context ("notify source A", NULL),
    new_context ("notify source B", NULL),
    new_context ("notify source C", NULL) };
  const gchar *signals[] = { "notify::context", "notify::image" };
  for (guint signal = 0; signal < G_N_ELEMENTS (signals); signal++)
    for (guint aba = 0; aba < 2; aba++)
      {
        GimpLayerTileView *view = new_owned_view ();
        Reentry reentry = { contexts[aba ? 0 : 2], images[aba ? 0 : 2],
                            aba ? contexts[2] : NULL,
                            aba ? images[2] : NULL, 0, FALSE };
        g_object_set (view, "context", contexts[0], "image", images[0], NULL);
        gulong handler = g_signal_connect (view, signals[signal],
                                          G_CALLBACK (reenter_notify), &reentry);
        if (signal == 0)
          g_object_set (view, "context", contexts[1], NULL);
        else
          g_object_set (view, "image", images[1], NULL);
        g_assert_cmpuint (reentry.calls, >=, 1);
        assert_properties (view, reentry.context, reentry.image);
        assert_action_context (view, reentry.context, reentry.image);
        g_signal_handler_disconnect (view, handler);
        clear_view (view);
      }
  for (guint i = 0; i < G_N_ELEMENTS (images); i++)
    {
      g_object_unref (contexts[i]);
      g_object_unref (images[i]);
    }
}

static void
c_property_notifications_destroy_no_revival (void)
{
  const gchar *signals[] = { "notify::context", "notify::image", "notify::sensitive" };
  GimpContext *context = new_context ("notify close source", NULL);
  GimpImage *image = new_image ();
  for (guint signal = 0; signal < G_N_ELEMENTS (signals); signal++)
    {
      GimpLayerTileView *view = new_owned_view ();
      Reentry reentry = { NULL, NULL, NULL, NULL, 0, TRUE };
      guint finalized = 0;
      watch_widget_storage (view, &finalized);
      gulong handler = g_signal_connect (view, signals[signal],
                                        G_CALLBACK (reenter_notify), &reentry);
      if (signal == 0)
        g_object_set (view, "context", context, NULL);
      else
        g_object_set (view, "image", image, NULL);
      g_assert_cmpuint (reentry.calls, >=, 1);
      assert_closed (view);
      /* GTK destroy has already disconnected instance signal handlers. */
      g_assert_false (g_signal_handler_is_connected (view, handler));
      g_object_set (view, "context", context, "image", image, NULL);
      assert_closed (view);
      clear_view (view);
      g_assert_cmpuint (finalized, ==, 1);
    }
  g_object_unref (context);
  g_object_unref (image);
}

static void
popup_reentry_case (gboolean change_context,
                    gboolean aba,
                    gboolean destroy)
{
  GimpImage *images[] = { new_image (), new_image (), new_image () };
  GimpContext *contexts[] = {
    new_context ("popup source A", NULL),
    new_context ("popup source B", NULL),
    new_context ("popup source C", NULL) };
  for (guint i = 0; i < G_N_ELEMENTS (images); i++)
    add_layer (images[i], "Popup image");
  GimpLayerTileView *view = new_owned_view ();
  Reentry reentry = { contexts[aba ? 0 : 2], images[aba ? 0 : 2],
                      aba ? contexts[1] : NULL,
                      aba ? images[1] : NULL, 0, destroy };
  g_object_set (view, "context", contexts[0], "image", images[0], NULL);
  MountedView mounted = mount_view (view);
  g_assert_nonnull (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (view), FALSE));
  gulong handler = g_signal_connect (view, "popup-closed",
                                    G_CALLBACK (reenter_popup_closed), &reentry);
  if (change_context)
    g_object_set (view, "context", contexts[1], NULL);
  else
    g_object_set (view, "image", images[1], NULL);
  drain ();
  g_assert_cmpuint (reentry.calls, ==, 1);
  if (destroy)
    {
      assert_closed (view);
      g_object_set (view, "context", contexts[2], "image", images[2], NULL);
      assert_closed (view);
    }
  else
    {
      assert_properties (view, reentry.context, reentry.image);
      assert_action_context (view, reentry.context, reentry.image);
      assert_first_canvas_layer (view, gimp_image_get_layer_iter (reentry.image)->data);
    }
  if (destroy) g_assert_false (g_signal_handler_is_connected (view, handler));
  else g_signal_handler_disconnect (view, handler);
  clear_mounted (&mounted);
  for (guint i = 0; i < G_N_ELEMENTS (images); i++)
    {
      g_object_unref (contexts[i]);
      g_object_unref (images[i]);
    }
}

static void
c_popup_close_newer_context_and_image (void)
{
  popup_reentry_case (FALSE, FALSE, FALSE);
  popup_reentry_case (TRUE, FALSE, FALSE);
}

static void
c_popup_close_aba_context_and_image (void)
{
  popup_reentry_case (FALSE, TRUE, FALSE);
  popup_reentry_case (TRUE, TRUE, FALSE);
}

static void
c_popup_close_destroy_no_revival (void)
{
  popup_reentry_case (FALSE, FALSE, TRUE);
  popup_reentry_case (TRUE, FALSE, TRUE);
}

typedef struct
{
  GimpLayerTileView *view;
  GimpImage         *image;
  guint             calls;
} ImageReentry;

static void
same_image_on_popup_closed (GtkWidget *widget,
                           gpointer   data)
{
  ImageReentry *reentry = data;
  (void) widget;
  if (reentry->calls++ == 0)
    g_object_set (reentry->view, "image", reentry->image, NULL);
}

static void
c_popup_close_same_target_image (void)
{
  GimpImage *a = new_image (), *b = new_image ();
  GimpLayer *old_layer = add_layer (a, "Old row");
  GimpLayer *new_layer = add_layer (b, "Same target row");
  GimpContext *context = new_context ("same target source", NULL);
  GimpLayerTileView *view = new_owned_view ();
  ImageReentry reentry = { view, b, 0 };
  g_object_set (view, "context", context, "image", a, NULL);
  MountedView mounted = mount_view (view);
  g_assert_nonnull (gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (view), FALSE));
  gulong handler = g_signal_connect (view, "popup-closed",
                                    G_CALLBACK (same_image_on_popup_closed), &reentry);
  /* The inherited editor has already adopted B when closing the old popup
   * calls back. A second B request must still commit the authoritative slot. */
  g_object_set (view, "image", b, NULL);
  drain ();
  g_assert_cmpuint (reentry.calls, ==, 1);
  assert_properties (view, context, b);
  assert_action_context (view, context, b);
  assert_first_canvas_layer (view, new_layer);
  g_assert_true (entry_button (entry_canvas (view), GDK_BUTTON_PRESS, 8, 8));
  g_assert_false (gimp_item_get_visible (GIMP_ITEM (new_layer)));
  g_assert_true (gimp_item_get_visible (GIMP_ITEM (old_layer)));
  g_signal_handler_disconnect (view, handler);
  clear_mounted (&mounted);
  g_object_unref (context);
  g_object_unref (a);
  g_object_unref (b);
}

static void
clear_image_on_manager_update (GimpUIManager *manager,
                               gpointer       update_data,
                               gpointer       data)
{
  ImageReentry *reentry = data;
  (void) manager;
  g_assert_true (update_data == reentry->view);
  if (reentry->calls++ == 0)
    g_object_set (reentry->view, "image", NULL, NULL);
}

static void
c_context_clear_explicit_image_clear_wins (void)
{
  GimpImage *image = new_image ();
  GimpLayer *layer = add_layer (image, "Explicit clear");
  GimpContext *context = new_context ("clear source", NULL);
  GimpLayerTileView *view = new_owned_view ();
  ImageReentry reentry = { view, NULL, 0 };
  g_object_set (view, "context", context, "image", image, NULL);
  MountedView mounted = mount_view (view);
  GimpUIManager *manager = gimp_editor_get_ui_manager (GIMP_EDITOR (view));
  g_assert_nonnull (manager);
  g_object_ref (manager);
  gulong handler = g_signal_connect (manager, "update",
                                    G_CALLBACK (clear_image_on_manager_update), &reentry);
  /* Suppressing the parent's implicit NULL image is one-shot. A real NULL
   * assignment from an update listener is a newer instruction and must win. */
  g_object_set (view, "context", NULL, NULL);
  g_assert_cmpuint (reentry.calls, >=, 1);
  g_signal_handler_disconnect (manager, handler);
  g_object_unref (manager);
  drain ();
  assert_properties (view, NULL, NULL);
  assert_action_context (view, NULL, NULL);
  /* The empty canvas must neither hit nor mutate the former image's row. */
  g_assert_true (entry_button (entry_canvas (view), GDK_BUTTON_PRESS, 8, 8));
  g_assert_true (gimp_item_get_visible (GIMP_ITEM (layer)));
  clear_mounted (&mounted);
  g_object_unref (context);
  g_object_unref (image);
}

typedef struct
{
  GimpLayerTileView *view;
  GimpContext       *context;
  GimpImage         *image;
  guint             calls;
} RowReentry;

static void
replace_on_context_release (gpointer data,
                            GObject *context)
{
  RowReentry *reentry = data;
  (void) context;
  if (reentry->calls++ == 0)
    {
      if (reentry->context)
        g_object_set (reentry->view, "context", reentry->context, NULL);
      else
        g_object_set (reentry->view, "image", reentry->image, NULL);
    }
}

static void
c_context_refresh_cache_release_reentry (void)
{
  for (guint replace_context = 0; replace_context < 2; replace_context++)
    {
      GimpImage *a = new_image (), *c = new_image ();
      GimpContext *contexts[] = {
        new_context ("row source A", NULL),
        new_context ("row source B", NULL),
        new_context ("row source C", NULL) };
      GimpLayerTileView *view = new_owned_view ();
      RowReentry reentry = { view, replace_context ? contexts[2] : NULL, c, 0 };
      add_layer (a, "A1");
      add_layer (a, "A2");
      add_layer (c, "C1");
      add_layer (c, "C2");
      add_layer (c, "C3");
      g_object_set (view, "context", contexts[0], "image", a, NULL);
      MountedView mounted = mount_view (view);
      /* No row widgets exist. Reenter on release of the old cache's action
       * context, the corresponding native ownership boundary during refresh. */
      g_object_weak_ref (G_OBJECT (GIMP_IMAGE_EDITOR (view)->context),
                         replace_on_context_release, &reentry);
      g_object_set (view, "context", contexts[1], NULL);
      drain ();
      GimpImage *expected_image = replace_context ? a : c;
      GimpContext *expected_context = replace_context ? contexts[2] : contexts[1];
      g_assert_cmpuint (reentry.calls, ==, 1);
      assert_properties (view, expected_context, expected_image);
      assert_action_context (view, expected_context, expected_image);
      gint width, height;
      gtk_widget_get_size_request (entry_canvas (view), &width, &height);
      g_assert_cmpint (height, ==, (replace_context ? 2 : 3) * 64);
      g_assert_cmpint (width, ==, 64);
      GtkWidget *visible = entry_canvas (view);
      GimpLayer *selected = gimp_image_get_selected_layers (expected_image)->data;
      gint undo_depth = gimp_undo_stack_get_depth (gimp_image_get_undo_stack (expected_image));
      g_assert_nonnull (visible);
      g_assert_true (entry_button (visible, GDK_BUTTON_PRESS, 8, 8));
      g_assert_false (gimp_item_get_visible (GIMP_ITEM (selected)));
      g_assert_cmpint (gimp_undo_stack_get_depth (gimp_image_get_undo_stack (expected_image)),
                       ==, undo_depth + 1);
      clear_mounted (&mounted);
      for (guint i = 0; i < G_N_ELEMENTS (contexts); i++)
        g_object_unref (contexts[i]);
      g_object_unref (a);
      g_object_unref (c);
    }
}

typedef struct
{
  GimpLayerTileView *view;
  GimpImage         *target;
  GimpImage         *intermediate;
  GtkWidget         *popup;
  guint             calls;
  guint             popup_destroyed;
} SensitiveReentry;

static void
count_popup_destroy (GtkWidget *widget,
                     gpointer   data)
{
  (void) widget;
  ++*(guint *) data;
}

static void
newer_popup_on_sensitive (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    data)
{
  SensitiveReentry *reentry = data;
  (void) object;
  (void) pspec;
  if (reentry->calls++ != 0)
    return;
  g_object_set (reentry->view, "image", reentry->intermediate, NULL);
  g_object_set (reentry->view, "image", reentry->target, NULL);
  reentry->popup = gimp_painter_layer_tiles_popup (
    GIMP_PAINTER_LAYER_TILES (reentry->view), FALSE);
  g_assert_nonnull (reentry->popup);
  g_object_ref (reentry->popup);
  g_signal_connect (reentry->popup, "destroy", G_CALLBACK (count_popup_destroy),
                    &reentry->popup_destroyed);
}

static void
c_parent_image_reentry_keeps_newer_popup (void)
{
  GimpImage *a = new_image (), *b = new_image ();
  GimpContext *context = new_context ("parent image source", NULL);
  GimpLayerTileView *view = new_owned_view ();
  SensitiveReentry reentry = { view, a, b, NULL, 0, 0 };
  add_layer (a, "Target popup");
  add_layer (b, "Intermediate image");
  g_object_set (view, "context", context, NULL);
  MountedView mounted = mount_view (view);
  gulong handler = g_signal_connect (view, "notify::sensitive",
                                    G_CALLBACK (newer_popup_on_sensitive), &reentry);
  /* The native outer entry leaves notify synchronous. An outer g_object_set
   * would freeze notify and exercise a different, post-setter reentry path. */
  gimp_painter_layer_tiles_set_image (GIMP_PAINTER_LAYER_TILES (view), a);
  g_signal_handler_disconnect (view, handler);
  drain ();
  g_assert_cmpuint (reentry.calls, >=, 1);
  assert_properties (view, context, a);
  assert_action_context (view, context, a);
  g_assert_cmpuint (reentry.popup_destroyed, ==, 0);
  g_assert_true (gtk_widget_get_visible (reentry.popup));
  g_assert_true (gtk_popover_get_relative_to (GTK_POPOVER (reentry.popup)) == GTK_WIDGET (view));
  clear_mounted (&mounted);
  g_assert_cmpuint (reentry.popup_destroyed, ==, 1);
  g_object_unref (reentry.popup);
  g_object_unref (context);
  g_object_unref (a);
  g_object_unref (b);
}

static gpointer
try_close_from_worker (gpointer data)
{
  GError *error = NULL;
  /* Only the binding admission gate is called off-thread. It must reject
   * before any GTK close hook, property, renderer or widget API executes. */
  g_assert_false (gimp_painter_binding_close (data, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_THREAD);
  g_clear_error (&error);
  return NULL;
}

static void
c_native_invalid_and_wrong_thread_controls (void)
{
  GObject *wrong = g_object_new (G_TYPE_OBJECT, NULL);
  GimpContext *context = new_context ("boundary source", NULL);
  GimpImage *image = new_image ();
  GimpLayerTileView *view = new_owned_view ();
  GError *error = NULL;
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  g_assert_false (gimp_painter_binding_close (NULL, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_WRONG_TYPE);
  g_clear_error (&error);
  g_assert_null (g_object_get_qdata (wrong, key));
  g_assert_null (gimp_painter_layer_tiles_popup ((GimpPainterLayerTiles *) wrong, TRUE));
  g_assert_false (gimp_painter_layer_tiles_long_press_pending ((GimpPainterLayerTiles *) wrong));
  gimp_painter_layer_tiles_cancel_interaction ((GimpPainterLayerTiles *) wrong);
  g_assert_null (g_object_get_qdata (wrong, key));
  /* The renamed C++ provider has the default log domain. The original
   * zero-argument constructor has no invalid-argument shape to probe. */
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_CONTEXT*failed*");
  g_assert_null (gimp_painter_layer_tiles_new (NULL, image));
  g_test_assert_expected_messages ();
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_CONTEXT*failed*");
  g_assert_null (gimp_painter_layer_tiles_new ((GimpContext *) wrong, image));
  g_test_assert_expected_messages ();
  g_test_expect_message (NULL, G_LOG_LEVEL_CRITICAL, "*GIMP_IS_IMAGE*failed*");
  g_assert_null (gimp_painter_layer_tiles_new (context, (GimpImage *) wrong));
  g_test_assert_expected_messages ();
  g_object_set (view, "context", context, "image", image, NULL);
  gpointer binding = g_object_get_qdata (G_OBJECT (view), key);
  g_assert_nonnull (binding);
  GThread *thread = g_thread_new ("layer-entry-admission", try_close_from_worker, view);
  g_thread_join (thread);
  assert_properties (view, context, image);
  g_assert_true (g_object_get_qdata (G_OBJECT (view), key) == binding);
  gtk_widget_destroy (GTK_WIDGET (view));
  thread = g_thread_new ("closed-layer-entry-admission", try_close_from_worker, view);
  g_thread_join (thread);
  assert_closed (view);
  clear_view (view);
  g_object_unref (wrong);
  g_object_unref (context);
  g_object_unref (image);
}

static void
c_destroyed_original_entry_is_terminal (void)
{
  GimpContext *context = new_context ("terminal source", NULL);
  GimpImage *image = new_image ();
  GimpLayer *layer = add_layer (image, "Terminal layer");
  GimpLayerTileView *view = new_owned_view ();
  guint finalized = 0;
  watch_widget_storage (view, &finalized);
  g_object_set (view, "context", context, "image", image, NULL);
  MountedView mounted = mount_view (view);
  gtk_widget_destroy (GTK_WIDGET (view));
  drain ();
  assert_closed (view);
  g_object_set (view, "context", context, "image", image, NULL);
  g_object_set (view, "image", image, "context", context, NULL);
  gimp_painter_layer_tiles_set_image (GIMP_PAINTER_LAYER_TILES (view), image);
  gimp_image_editor_set_image (GIMP_IMAGE_EDITOR (view), image);
  gimp_painter_layer_tiles_cancel_interaction (GIMP_PAINTER_LAYER_TILES (view));
  g_assert_false (gimp_painter_layer_tiles_move (GIMP_PAINTER_LAYER_TILES (view),
                                               layer, layer, FALSE, FALSE));
  assert_closed (view);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (view), NULL));
  clear_mounted (&mounted);
  g_assert_cmpuint (finalized, ==, 1);
  /* Any stale parent/model connection would now observe freed storage. */
  gimp_image_flush (image);
  gimp_object_set_name (GIMP_OBJECT (layer), "After owner finalization");
  gimp_context_set_image (context, image);
  drain ();
  g_object_unref (context);
  g_object_unref (image);
}

int
main (int argc, char **argv)
{
  int result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
#define ADD(function) G_STMT_START { \
    static const EntryTestCase entry = { function }; \
    g_test_add_data_func ("/layer-tile-entry-contracts/c/" #function, \
                          &entry, run_entry_case); \
  } G_STMT_END
  ADD (c_factory_type_and_property_contract);
  ADD (c_construct_property_orders);
  ADD (c_deferred_nullable_assignment_matrix);
  ADD (c_context_only_and_active_image_independence);
  ADD (c_inherited_source_properties_stay_isolated);
  ADD (c_exact_object_getter_reference_lifetimes);
  ADD (c_replacement_and_destroy_release_inputs);
  ADD (c_same_image_context_refreshes_previews);
  ADD (c_contextless_image_actions_inert);
  ADD (c_property_notifications_newer_assignment);
  ADD (c_property_notifications_destroy_no_revival);
  ADD (c_popup_close_newer_context_and_image);
  ADD (c_popup_close_aba_context_and_image);
  ADD (c_popup_close_destroy_no_revival);
  ADD (c_popup_close_same_target_image);
  ADD (c_context_clear_explicit_image_clear_wins);
  ADD (c_context_refresh_cache_release_reentry);
  ADD (c_parent_image_reentry_keeps_newer_popup);
  ADD (c_native_invalid_and_wrong_thread_controls);
  ADD (c_destroyed_original_entry_is_terminal);
#undef ADD
  gimp_test_layer_tile_entry_cpp_register (gimp);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
