/* SPDX-License-Identifier: GPL-3.0-or-later
 * C++14 ownership and integrated operation controls for the original entries.
 * The companion C translation unit owns application startup and C-only tests.
 */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdata.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "core/gimplayerpreset.h"
#include "core/gimpclonelayer.h"
#include "widgets/gimplayertileview.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "gimp-app-test-utils.h"
}
#include "painter/binding-store.hpp"
#include "widgets/gimplayertileview-handle.hpp"
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>

using GimpPainter::BindingStore;
using GimpPainter::LayerTileViewRef;
using GimpPainter::TypeTraits;

static_assert (std::is_same<decltype (&gimp_layer_tile_view_new),
                           GimpLayerTileView *(*) (void)>::value,
               "The original constructor has no context argument");
static_assert (std::is_same<GimpLayerTileView, GimpPainterLayerTiles>::value,
               "The original route must name the existing native widget");
static_assert (std::is_same<decltype (LayerTileViewRef::create ().get ()),
                           GimpLayerTileView *>::value,
               "The handle exposes the original C instance type");
static_assert (std::is_copy_constructible<LayerTileViewRef>::value &&
               std::is_copy_assignable<LayerTileViewRef>::value &&
               std::is_move_constructible<LayerTileViewRef>::value &&
               std::is_move_assignable<LayerTileViewRef>::value,
               "An owning handle supports copy and move ownership");

extern "C" void gimp_test_layer_tile_entry_cpp_register (Gimp *gimp);

#ifdef GIMP_TEST_LAYER_TILE_WRAP_ENTRIES
extern "C" GimpLayerTileView *__wrap_gimp_layer_tile_view_new (void);
extern "C" GType __wrap_gimp_layer_tile_view_get_type (void);
static guint factory_calls;
static GimpLayerTileView *last_factory_result;
extern "C" GimpLayerTileView *__real_gimp_layer_tile_view_new (void);
extern "C" GType __real_gimp_layer_tile_view_get_type (void);
extern "C" GimpLayerTileView *__wrap_gimp_layer_tile_view_new (void)
{
  ++factory_calls;
  last_factory_result = __real_gimp_layer_tile_view_new ();
  return last_factory_result;
}
extern "C" GType __wrap_gimp_layer_tile_view_get_type (void)
{
  /* The public provider is G_GNUC_CONST. Optimizers may coalesce or eliminate
   * calls, so invocation counts are not evidence for this route. The wrapper
   * still proves that the link resolves the original provider directly. */
  return __real_gimp_layer_tile_view_get_type ();
}
#endif

namespace {
Gimp *application;

void storage_finalized (gpointer data)
{
  ++*static_cast<guint *> (data);
}

void watch_storage_finalization (GObject *object, guint *count)
{
  /* GTK clears GObject weak pointers at destroy/dispose. Qdata destruction
   * observes the later final storage release, including when a handle keeps
   * the destroyed widget alive. The counter outlives every owning ref. */
  const GQuark key = g_quark_from_static_string ("layer-tile-entry-storage-finalized");
  g_assert_null (g_object_get_qdata (object, key));
  g_object_set_qdata_full (object, key, count, storage_finalized);
}

void drain ()
{
  for (guint i = 0; i < 300 && g_main_context_pending (nullptr); ++i)
    g_main_context_iteration (nullptr, FALSE);
}

GtkWidget *find (GtkWidget *widget, const gchar *name)
{
  if (!widget) return nullptr;
  if (g_strcmp0 (gtk_widget_get_name (widget), name) == 0) return widget;
  if (!GTK_IS_CONTAINER (widget)) return nullptr;
  GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
  GtkWidget *result = nullptr;
  for (GList *i = children; i && !result; i = i->next)
    result = find (GTK_WIDGET (i->data), name);
  g_list_free (children);
  return result;
}

GtkWidget *find_button_label (GtkWidget *widget, const gchar *label)
{
  if (!widget) return nullptr;
  if (GTK_IS_BUTTON (widget) &&
      g_strcmp0 (gtk_button_get_label (GTK_BUTTON (widget)), label) == 0)
    return widget;
  if (!GTK_IS_CONTAINER (widget)) return nullptr;
  GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
  GtkWidget *result = nullptr;
  for (GList *i = children; i && !result; i = i->next)
    result = find_button_label (GTK_WIDGET (i->data), label);
  g_list_free (children);
  return result;
}

GimpImage *new_image ()
{
  return gimp_image_new (application, 64, 64, GIMP_RGB,
                         GIMP_PRECISION_U8_NON_LINEAR);
}

GimpLayer *add_layer (GimpImage *image, const gchar *name)
{
  GimpLayer *layer = gimp_layer_new (image, 64, 64,
    gimp_image_get_layer_format (image, TRUE), name, 1.,
    GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_nonnull (layer);
  gimp_image_add_layer (image, layer, nullptr, 0, FALSE);
  return layer;
}

void select_only (GimpImage *image, GimpLayer *layer)
{
  GList *selection = g_list_append (nullptr, layer);
  gimp_image_set_selected_layers (image, selection);
  g_list_free (selection);
  drain ();
}

void assert_properties (GimpLayerTileView *view,
                        GimpContext *context, GimpImage *image)
{
  GimpContext *actual_context = nullptr;
  GimpImage *actual_image = nullptr;
  g_object_get (view, "context", &actual_context, "image", &actual_image, nullptr);
  g_assert_true (actual_context == context);
  g_assert_true (actual_image == image);
  g_clear_object (&actual_context);
  g_clear_object (&actual_image);
  g_assert_true (gimp_image_editor_get_image (GIMP_IMAGE_EDITOR (view)) == image);
}

/* Deliberately uses the original C factory, then lets GTK sink its floating
 * result. retain() must add ownership without stealing the container's ref. */
struct Panel {
  GtkWidget *window;
  std::unique_ptr<LayerTileViewRef> view;
  Panel (GimpContext *context, GimpImage *image, bool image_first = false)
  {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_object_ref_sink (window);
    gtk_window_set_default_size (GTK_WINDOW (window), 230, 500);
    auto *raw = gimp_layer_tile_view_new ();
    g_assert_nonnull (raw);
    g_assert_true (g_object_is_floating (raw));
    if (image_first)
      g_object_set (raw, "image", image, "context", context, nullptr);
    else
      g_object_set (raw, "context", context, "image", image, nullptr);
    gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (raw));
    view.reset (new LayerTileViewRef (LayerTileViewRef::retain (raw)));
    gtk_widget_show_all (window);
    drain ();
    assert_properties (raw, context, image);
  }
  ~Panel ()
  {
    gtk_widget_destroy (window);
    g_object_unref (window);
    view.reset ();
    drain ();
  }
  GimpLayerTileView *get () const { return view->get (); }
};

struct Preset {
  static const gchar *name () { return "Entry contract clone preset"; }
  GimpContainer *container;
  GimpLayerPreset *preset;
  Preset ()
  {
    container = gimp_data_factory_get_container (application->layer_preset_factory);
    g_assert_null (gimp_container_get_child_by_name (container, name ()));
    preset = GIMP_LAYER_PRESET (gimp_layer_preset_new (
      gimp_get_user_context (application), name ()));
    GError *error = nullptr;
    JsonNode *root = json_from_string (
      "{\"version\":1,\"source-layer\":{\"type\":\"any\"},"
      "\"replacement-layer\":[{\"target\":\"new\",\"type\":\"clone\","
      "\"name\":\"Entry contract clone\",\"source\":\"source\","
      "\"boundary\":\"source\"},{\"target\":\"source\"}]}", &error);
    g_assert_no_error (error);
    g_assert_nonnull (root);
    g_assert_true (gimp_layer_preset_set_json (preset, root, &error));
    g_assert_no_error (error);
    json_node_unref (root);
    /* Native factory resources need an identifier even for in-memory fixtures. */
    gimp_data_make_internal (GIMP_DATA (preset), "gimp-layer-entry-clone-preset");
    g_assert_true (gimp_container_add (container, GIMP_OBJECT (preset)));
  }
  ~Preset ()
  {
    g_assert_true (gimp_container_remove (container, GIMP_OBJECT (preset)));
    g_object_unref (preset);
  }
};

void click_clone_and_undo (GimpLayerTileView *view, GimpImage *image,
                          GimpLayer *source, bool preset)
{
  select_only (image, source);
  const gint before = gimp_image_get_n_layers (image);
  GtkWidget *popup = gimp_painter_layer_tiles_popup (
    GIMP_PAINTER_LAYER_TILES (view), preset);
  g_assert_true (GTK_IS_POPOVER (popup));
  GtkWidget *button = preset ? find_button_label (popup, Preset::name ()) :
                              find (popup, "layers-new-clone");
  g_assert_true (GTK_IS_BUTTON (button));
  g_assert_true (gtk_widget_get_sensitive (button));
  gtk_button_clicked (GTK_BUTTON (button));
  drain ();
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, before + 1);
  GimpLayer *clone = nullptr;
  for (GList *i = gimp_image_get_layer_iter (image); i; i = i->next)
    if (GIMP_IS_CLONE_LAYER (i->data) &&
        gimp_clone_layer_get_source (GIMP_CLONE_LAYER (i->data)) == source)
      {
        g_assert_null (clone);
        clone = GIMP_LAYER (i->data);
      }
  g_assert_nonnull (clone);
  g_assert_true (gimp_item_is_attached (GIMP_ITEM (source)));
  g_assert_true (gimp_image_undo (image));
  drain ();
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, before);
  g_assert_true (gimp_item_is_attached (GIMP_ITEM (source)));
  g_assert_true (g_list_find (gimp_image_get_layer_iter (image), source) != nullptr);
}

void original_provider_and_store ()
{
#ifdef GIMP_TEST_LAYER_TILE_WRAP_ENTRIES
  const guint before = factory_calls;
#endif
  auto view = LayerTileViewRef::create ();
#ifdef GIMP_TEST_LAYER_TILE_WRAP_ENTRIES
  g_assert_cmpuint (factory_calls, ==, before + 1);
  g_assert_true (view.get () == last_factory_result);
#else
  g_test_message ("Factory interception is unavailable on this linker; ownership and native identity still run");
#endif
  g_assert_false (g_object_is_floating (view.get ()));
  g_assert_cmpuint (G_OBJECT_TYPE (view.get ()), ==, gimp_layer_tile_view_get_type ());
  g_assert_cmpuint (G_OBJECT_TYPE (view.get ()), ==, gimp_painter_layer_tiles_get_type ());
  g_assert_cmpuint (TypeTraits<GimpLayerTileView>::type (), ==, G_OBJECT_TYPE (view.get ()));
  g_assert_cmpuint (TypeTraits<GimpPainterLayerTiles>::type (), ==, G_OBJECT_TYPE (view.get ()));
  g_assert_cmpuint (g_type_parent (G_OBJECT_TYPE (view.get ())), ==, GIMP_TYPE_IMAGE_EDITOR);
  auto *store = BindingStore::find (G_OBJECT (view.get ()));
  g_assert_nonnull (store);
  g_assert_true (store->state () == BindingStore::State::active);
  assert_properties (view.get (), nullptr, nullptr);
  auto retained = LayerTileViewRef::retain (view.get ());
  GimpContext *context = gimp_get_user_context (application);
  g_object_set (view.get (), "context", context, nullptr);
  g_assert_true (BindingStore::find (G_OBJECT (retained.get ())) == store);
  assert_properties (retained.get (), context, nullptr);
  GtkWidget *renamed = gimp_painter_layer_tiles_new (context, nullptr);
  g_object_ref_sink (renamed);
  g_assert_cmpuint (G_OBJECT_TYPE (renamed), ==, G_OBJECT_TYPE (view.get ()));
  g_assert_nonnull (BindingStore::find (G_OBJECT (renamed)));
  gtk_widget_destroy (renamed);
  g_object_unref (renamed);
  gtk_widget_destroy (GTK_WIDGET (view.get ()));
  g_assert_true (BindingStore::find (G_OBJECT (retained.get ())) == store);
  g_assert_true (store->state () == BindingStore::State::closed);
}

void ownership_orders (bool retain_parented, bool parent_first)
{
  GtkWidget *parent = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_ref_sink (parent);
  std::unique_ptr<LayerTileViewRef> handle;
  GimpLayerTileView *raw;
  if (retain_parented)
    {
      raw = gimp_layer_tile_view_new ();
      g_assert_true (g_object_is_floating (raw));
      gtk_container_add (GTK_CONTAINER (parent), GTK_WIDGET (raw));
      g_assert_false (g_object_is_floating (raw));
      handle.reset (new LayerTileViewRef (LayerTileViewRef::retain (raw)));
    }
  else
    {
      handle.reset (new LayerTileViewRef (LayerTileViewRef::create ()));
      raw = handle->get ();
      g_assert_false (g_object_is_floating (raw));
      gtk_container_add (GTK_CONTAINER (parent), GTK_WIDGET (raw));
    }
  guint finalized = 0;
  watch_storage_finalization (G_OBJECT (raw), &finalized);
  gpointer weak = raw;
  g_object_add_weak_pointer (G_OBJECT (raw), &weak);
  auto *store = BindingStore::find (G_OBJECT (raw));
  g_assert_nonnull (store);
  if (parent_first)
    {
      gtk_widget_destroy (parent);
      g_object_unref (parent);
      g_assert_null (weak); // GTK disposal, not final storage release.
      g_assert_cmpuint (finalized, ==, 0);
      g_assert_true (handle->get () == raw);
      g_assert_true (BindingStore::find (G_OBJECT (raw)) == store);
      g_assert_true (store->state () == BindingStore::State::closed);
      /* A handle preserves storage, never operational life after destroy. */
      g_assert_null (gimp_painter_layer_tiles_popup (
        GIMP_PAINTER_LAYER_TILES (handle->get ()), FALSE));
      assert_properties (raw, nullptr, nullptr);
      handle.reset ();
    }
  else
    {
      handle.reset ();
      g_assert_nonnull (weak);
      g_assert_cmpuint (finalized, ==, 0);
      g_assert_true (gtk_widget_get_parent (GTK_WIDGET (raw)) == parent);
      g_assert_true (store->state () == BindingStore::State::active);
      gtk_widget_destroy (parent);
      g_object_unref (parent);
    }
  drain ();
  g_assert_null (weak);
  g_assert_cmpuint (finalized, ==, 1);
}

void copy_move_ownership ()
{
  guint finalized = 0;
  gpointer weak = nullptr;
  std::unique_ptr<LayerTileViewRef> last;
  {
    auto original = LayerTileViewRef::create ();
    auto copy = original;
    watch_storage_finalization (G_OBJECT (original.get ()), &finalized);
    weak = original.get ();
    g_object_add_weak_pointer (G_OBJECT (original.get ()), &weak);
    auto *store = BindingStore::find (G_OBJECT (original.get ()));
    auto moved = std::move (copy);
    g_assert_null (copy.get ());
    g_assert_true (moved.get () == original.get ());
    copy = original;
    last.reset (new LayerTileViewRef (std::move (copy)));
    g_assert_null (copy.get ());
    copy = moved;
    moved = std::move (copy);
    g_assert_null (copy.get ());
    g_assert_true (moved.get () == original.get ());
    g_assert_true (BindingStore::find (G_OBJECT (last->get ())) == store);
  }
  g_assert_nonnull (weak);
  g_assert_cmpuint (finalized, ==, 0);
  gtk_widget_destroy (GTK_WIDGET (last->get ()));
  g_assert_null (weak);
  g_assert_cmpuint (finalized, ==, 0);
  g_assert_true (BindingStore::require (G_OBJECT (last->get ())).state () ==
                 BindingStore::State::closed);
  last.reset ();
  g_assert_null (weak);
  g_assert_cmpuint (finalized, ==, 1);
}

void retain_rejects_invalid ()
{
  GObject *plain = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  for (auto *candidate : {static_cast<GimpLayerTileView *> (nullptr),
                         reinterpret_cast<GimpLayerTileView *> (plain)})
    {
      bool rejected = false;
      try { auto handle = LayerTileViewRef::retain (candidate); (void) handle; }
      catch (const GimpPainter::Error& error)
        { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
      g_assert_true (rejected);
    }
  g_assert_null (BindingStore::find (plain));
  g_object_unref (plain);
}

void original_selection_and_visibility ()
{
  GimpImage *image = new_image ();
  GimpLayer *a = add_layer (image, "Entry A");
  GimpLayer *b = add_layer (image, "Entry B");
  {
    Panel panel (gimp_get_user_context (application), image);
    GtkWidget *canvas = find (GTK_WIDGET (panel.get ()), "painter-layer-canvas");
    g_assert_true (GTK_IS_DRAWING_AREA (canvas));
    auto click = [canvas] (double x, double y) {
      for (auto type : { GDK_BUTTON_PRESS, GDK_BUTTON_RELEASE }) {
        GdkEvent *event = gdk_event_new (type);
        event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (canvas)));
        event->button.send_event = TRUE;
        event->button.time = GDK_CURRENT_TIME;
        event->button.button = 1;
        event->button.x = x;
        event->button.y = y;
        gdk_event_set_device (event, gdk_seat_get_pointer (
          gdk_display_get_default_seat (gtk_widget_get_display (canvas))));
        g_assert_true (gtk_widget_event (canvas, event));
        gdk_event_free (event);
      }
    };
    click (8, 8);  // The painted visibility hit region in the first tile.
    drain ();
    g_assert_false (gimp_item_get_visible (GIMP_ITEM (b)));
    g_assert_true (gimp_item_get_visible (GIMP_ITEM (a)));
    g_assert_true (gimp_image_undo (image));
    drain ();
    g_assert_true (gimp_item_get_visible (GIMP_ITEM (b)));
    GList *selection = g_list_append (g_list_append (nullptr, a), b);
    gimp_image_set_selected_layers (image, selection);
    g_list_free (selection);
    drain ();
    selection = gimp_image_get_selected_layers (image);
    g_assert_cmpuint (g_list_length (selection), ==, 2);
    g_assert_nonnull (g_list_find (selection, a));
    g_assert_nonnull (g_list_find (selection, b));
    // A genuine short press resolves the multi-selection to the hit layer.
    click (28, 32);
    drain ();
    selection = gimp_image_get_selected_layers (image);
    g_assert_cmpuint (g_list_length (selection), ==, 1);
    g_assert_true (selection->data == b);
  }
  g_object_unref (image);
}

void original_popup_clone_undo ()
{
  GimpImage *image = new_image ();
  GimpLayer *source = add_layer (image, "Entry clone source");
  {
    Panel panel (gimp_get_user_context (application), image);
    click_clone_and_undo (panel.get (), image, source, false);
  }
  g_object_unref (image);
}

void original_popup_preset_undo ()
{
  Preset preset;
  GimpImage *image = new_image ();
  GimpLayer *source = add_layer (image, "Entry preset source");
  {
    Panel panel (gimp_get_user_context (application), image, true);
    click_clone_and_undo (panel.get (), image, source, true);
  }
  g_object_unref (image);
}

void supplied_context_isolation ()
{
  Preset preset;
  GimpImage *a = new_image (), *b = new_image (), *foreign = new_image ();
  GimpLayer *la = add_layer (a, "Independent A");
  GimpLayer *lb = add_layer (b, "Independent B");
  GimpContext *source = gimp_context_new (application, "entry-source-context",
                                         gimp_get_user_context (application));
  gimp_context_set_image (source, foreign);
  {
    Panel pa (source, a), pb (source, b, true);
    g_assert_true (gimp_context_get_image (source) == foreign);
    GimpContext *ca = action_data_get_context (pa.get ());
    GimpContext *cb = action_data_get_context (pb.get ());
    g_assert_nonnull (ca);
    g_assert_nonnull (cb);
    g_assert_true (ca != source && cb != source && ca != cb);
    g_assert_true (gimp_context_get_image (ca) == a);
    g_assert_true (gimp_context_get_image (cb) == b);
    click_clone_and_undo (pa.get (), a, la, true);
    g_assert_cmpint (gimp_image_get_n_layers (b), ==, 1);
    g_assert_true (gimp_context_get_image (source) == foreign);
    gimp_context_set_image (source, a);
    drain ();
    assert_properties (pa.get (), source, a);
    assert_properties (pb.get (), source, b);
    click_clone_and_undo (pb.get (), b, lb, false);
    g_assert_cmpint (gimp_image_get_n_layers (a), ==, 1);
    g_assert_true (gimp_context_get_image (source) == a);
  }
  gimp_context_set_image (source, nullptr);
  g_object_unref (source);
  g_object_unref (a);
  g_object_unref (b);
  g_object_unref (foreign);
}

GimpDisplayShell *new_shell ()
{
#ifdef GIMP_TEST_LAYER_TILE_WRAP_ENTRIES
  const guint before = factory_calls;
#endif
  gimp_test_utils_create_image (application, 128, 128);
  drain ();
  GimpDisplay *display = gimp_context_get_display (gimp_get_user_context (application));
  g_assert_nonnull (display);
  GimpDisplayShell *shell = gimp_display_get_shell (display);
  g_assert_nonnull (shell);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  drain ();
#ifdef GIMP_TEST_LAYER_TILE_WRAP_ENTRIES
  g_assert_cmpuint (factory_calls, >, before);
  g_assert_true (GTK_WIDGET (last_factory_result) ==
                 find (shell->canvas, "painter-layer-tiles"));
#endif
  return shell;
}

void close_shell (GimpDisplayShell *shell)
{
  GimpImage *image = gimp_display_get_image (shell->display);
  gimp_display_delete (shell->display);
  g_object_unref (image);
  drain ();
}

void both_canvas_action_isolation ()
{
  Preset preset;
  GimpDisplayShell *a = new_shell (), *b = new_shell ();
  GimpImage *ia = gimp_display_get_image (a->display);
  GimpImage *ib = gimp_display_get_image (b->display);
  GimpLayer *la = GIMP_LAYER (gimp_image_get_selected_layers (ia)->data);
  GimpLayer *lb = GIMP_LAYER (gimp_image_get_selected_layers (ib)->data);
  GtkWidget *ta = find (a->canvas, "painter-layer-tiles");
  GtkWidget *tb = find (b->canvas, "painter-layer-tiles");
  g_assert_true (GIMP_IS_LAYER_TILE_VIEW (ta));
  g_assert_true (GIMP_IS_LAYER_TILE_VIEW (tb));
  GimpContext *global = gimp_get_user_context (application);
  assert_properties (GIMP_LAYER_TILE_VIEW (ta), global, ia);
  assert_properties (GIMP_LAYER_TILE_VIEW (tb), global, ib);
  g_assert_true (action_data_get_context (ta) != action_data_get_context (tb));
  g_assert_true (action_data_get_context (ta) != global);
  g_assert_true (action_data_get_context (tb) != global);
  g_assert_true (action_data_get_image (ta) == ia);
  g_assert_true (action_data_get_image (tb) == ib);
  g_assert_true (action_data_get_display (ta) == a->display);
  g_assert_true (action_data_get_display (tb) == b->display);
  /* Each action starts while the other canvas is globally active. Popup
   * dismissal legitimately restores focus to its own canvas, and native shell
   * focus updates the global display; that is not a configuration leak. */
  for (bool use_preset : { false, true }) {
    gimp_context_set_display (global, b->display);
    gimp_context_set_image (global, ib);
    drain ();
    g_assert_true (gimp_context_get_image (global) == ib);
    click_clone_and_undo (GIMP_LAYER_TILE_VIEW (ta), ia, la, use_preset);
    g_assert_cmpint (gimp_image_get_n_layers (ib), ==, 1);
    assert_properties (GIMP_LAYER_TILE_VIEW (ta), global, ia);
    assert_properties (GIMP_LAYER_TILE_VIEW (tb), global, ib);
  }
  for (bool use_preset : { false, true }) {
    gimp_context_set_display (global, a->display);
    gimp_context_set_image (global, ia);
    drain ();
    g_assert_true (gimp_context_get_image (global) == ia);
    click_clone_and_undo (GIMP_LAYER_TILE_VIEW (tb), ib, lb, use_preset);
    g_assert_cmpint (gimp_image_get_n_layers (ia), ==, 1);
    assert_properties (GIMP_LAYER_TILE_VIEW (ta), global, ia);
    assert_properties (GIMP_LAYER_TILE_VIEW (tb), global, ib);
  }
  /* Close the real first canvas while retaining its tile storage. The second
   * canvas remains operational and its own image/context must survive. */
  auto retained = std::unique_ptr<LayerTileViewRef> (
    new LayerTileViewRef (LayerTileViewRef::retain (GIMP_LAYER_TILE_VIEW (ta))));
  guint finalized = 0;
  watch_storage_finalization (G_OBJECT (ta), &finalized);
  gpointer weak = ta;
  g_object_add_weak_pointer (G_OBJECT (ta), &weak);
  close_shell (a);
  g_assert_null (weak);
  g_assert_cmpuint (finalized, ==, 0);
  g_assert_true (BindingStore::require (G_OBJECT (retained->get ())).state () ==
                 BindingStore::State::closed);
  assert_properties (retained->get (), nullptr, nullptr);
  retained.reset ();
  drain ();
  g_assert_null (weak);
  g_assert_cmpuint (finalized, ==, 1);
  click_clone_and_undo (GIMP_LAYER_TILE_VIEW (tb), ib, lb, true);
  close_shell (b);
}

struct TestCase { void (*run) (); };
void run_case (gconstpointer data)
{
  /* A filtered invocation must not depend on an earlier window receiving focus. */
  gimp_set_focused_once (application);
  static_cast<const TestCase *> (data)->run ();
}
} // namespace

extern "C" void
gimp_test_layer_tile_entry_cpp_register (Gimp *gimp)
{
  application = gimp;
#define ADD(path, function) do { \
  static const TestCase test_case = { function }; \
  g_test_add_data_func ("/layer-tile-entry-contracts/cpp/" path, &test_case, run_case); \
} while (0)
  ADD ("original-provider-and-store", original_provider_and_store);
  ADD ("create-parent-first", [] { ownership_orders (false, true); });
  ADD ("create-handle-first", [] { ownership_orders (false, false); });
  ADD ("retain-parent-first", [] { ownership_orders (true, true); });
  ADD ("retain-handle-first", [] { ownership_orders (true, false); });
  ADD ("copy-move-ownership", copy_move_ownership);
  ADD ("retain-rejects-invalid", retain_rejects_invalid);
  ADD ("original-selection-and-visibility", original_selection_and_visibility);
  ADD ("original-popup-clone-undo", original_popup_clone_undo);
  ADD ("original-popup-preset-undo", original_popup_preset_undo);
  ADD ("supplied-context-isolation", supplied_context_isolation);
  ADD ("both-canvas-action-isolation", both_canvas_action_isolation);
#undef ADD
}
