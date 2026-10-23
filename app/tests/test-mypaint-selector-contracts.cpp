/* SPDX-License-Identifier: GPL-3.0-or-later */
/* C++14 ownership and actual native GUI integration for row 35. History is
 * deliberately the application-owned snapshot design, not old brush identity. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "tools/tools-types.h"
#include "config/gimpcoreconfig.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdrawable.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimpundostack.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "tools/gimppaintermybrushtool.h"
#include "tools/gimptoolcontrol.h"
#include "tools/tool_manager.h"
#include "widgets/gimpcontainereditor.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpcontainericonview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpmypaintbrushoptions-gui.h"
#include "widgets/gimppaintermybrusheditor.h"
#include "widgets/gimppaintercompactoptions.h"
#include "widgets/gimpview.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimpwidgets-utils.h"
#include "painter/gimp-painter-binding.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
}
#include "core/gimpcontext-handle.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "painter/binding-store.hpp"
#include <algorithm>
#include <cstring>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "test-mypaint-selector-history.inc"

using namespace GimpPainter;
extern "C" void gimp_test_mypaint_selector_cpp_register (Gimp *application);
static_assert (std::is_same<decltype (&gimp_mypaint_brush_button_with_popup),
                           GtkWidget *(*) (GObject *)>::value,
               "The original C factory signature must stay source compatible");
static_assert (std::is_same<decltype (std::declval<const ContextRef&> ().create_mypaint_brush_button ()),
                           ObjectRef<GObject>>::value,
               "The named context route returns one sunk owning widget handle");
static_assert (std::is_copy_constructible<ContextRef>::value &&
               std::is_move_constructible<ContextRef>::value,
               "ContextRef retains ordinary value ownership");

#ifdef GIMP_TEST_MYPAINT_SELECTOR_WRAP_FACTORY
static guint selector_factory_calls;
static GtkWidget *selector_factory_result;
extern "C" GtkWidget *__real_gimp_mypaint_brush_button_with_popup (GObject *);
extern "C" GtkWidget *__wrap_gimp_mypaint_brush_button_with_popup (GObject *);
extern "C" GtkWidget *__wrap_gimp_mypaint_brush_button_with_popup (GObject *context)
{
  ++selector_factory_calls;
  selector_factory_result = __real_gimp_mypaint_brush_button_with_popup (context);
  return selector_factory_result;
}
#endif

namespace {
Gimp *application;
constexpr const char *selector_name = "painter-mypaint-brush-selector";
struct ContextUnref { void operator() (GimpContext *context) const { if (context) g_object_unref (context); } };
using ContextOwner = std::unique_ptr<GimpContext, ContextUnref>;

void drain () { gimp_test_run_mainloop_until_idle (); }

void collect (GtkWidget *widget, std::set<GtkWidget *>& result)
{
  if (!widget || !result.insert (widget).second) return;
  if (GTK_IS_MENU_BUTTON (widget))
    collect (GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (widget))), result);
  if (GTK_IS_CONTAINER (widget))
    {
      auto *children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (auto *i = children; i; i = i->next) collect (GTK_WIDGET (i->data), result);
      g_list_free (children);
    }
}

GtkWidget *find (GtkWidget *root, const char *name, GType type = G_TYPE_INVALID)
{
  std::set<GtkWidget *> all; collect (root, all);
  for (auto *widget : all)
    if ((!name || !g_strcmp0 (gtk_widget_get_name (widget), name)) &&
        (!type || G_TYPE_CHECK_INSTANCE_TYPE (widget, type))) return widget;
  return nullptr;
}

GtkWidget *control (GtkWidget *root, const char *name)
{
  auto *result = find (root, name); g_assert_nonnull (result); return result;
}

guint count_type (GtkWidget *root, GType type)
{
  std::set<GtkWidget *> all; collect (root, all); guint count = 0;
  for (auto *widget : all) if (G_TYPE_CHECK_INSTANCE_TYPE (widget, type)) ++count;
  return count;
}

GimpToolInfo *tool_info ()
{
  auto *info = gimp_get_tool_info (application, "gimp-painter-mypaint-tool");
  g_assert_nonnull (info); return info;
}

ObjectRef<GimpPainterMybrushOptions> new_options ()
{
  gimp_set_focused_once (application);
  return ObjectRef<GimpPainterMybrushOptions>::adopt (
    GIMP_PAINTER_MYBRUSH_OPTIONS (g_object_new (
      GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, "gimp", application, nullptr)));
}

void set_draft (GimpPainterMybrushOptions *options, const MyPaint::Resource& resource)
{
  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_options_set_json (options, resource.encode ().c_str (), &error));
  g_assert_no_error (error);
}

void set_name (GimpPainterMybrushOptions *options, const char *name)
{
  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_options_set_draft_name (options, name, &error));
  g_assert_no_error (error);
}

void set_icon (GimpPainterMybrush *brush, guint32 color)
{
  auto icon = ObjectRef<GObject>::adopt (G_OBJECT (gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 16, 16)));
  gdk_pixbuf_fill (GDK_PIXBUF (icon.get ()), color);
  GError *error = nullptr;
  g_assert_true (gimp_painter_mybrush_set_icon (brush, GDK_PIXBUF (icon.get ()), &error));
  g_assert_no_error (error);
}

std::vector<guchar> pixels (GdkPixbuf *pixbuf)
{
  g_assert_nonnull (pixbuf); std::vector<guchar> result;
  for (gint y = 0; y < gdk_pixbuf_get_height (pixbuf); ++y)
    {
      const auto *row = gdk_pixbuf_get_pixels (pixbuf) + y * gdk_pixbuf_get_rowstride (pixbuf);
      result.insert (result.end (), row, row + gdk_pixbuf_get_width (pixbuf) * gdk_pixbuf_get_n_channels (pixbuf));
    }
  return result;
}

void add_fixture_brush (GimpContainer *container, GimpPainterMybrush *brush)
{
  static guint next_identifier = 0;
  String identifier (g_strdup_printf ("selector-contract-fixture-%u", ++next_identifier));
  /* Native factory tag/identifier consumers require an internal, file, or
   * image identity. These temporary resources never create/save real files. */
  gimp_data_make_internal (GIMP_DATA (brush), identifier.get ());
  g_assert_true (gimp_container_add (container, GIMP_OBJECT (brush)));
}

struct Resources {
  GimpContainer *container;
  PainterMybrushRef a = PainterMybrushRef::create ("Selector fixture A");
  PainterMybrushRef b = PainterMybrushRef::create ("Selector fixture B");
  PainterMybrushRef c = PainterMybrushRef::create ("Selector fixture C");
  Resources ()
  {
    gimp_set_focused_once (application);
    container = gimp_data_factory_get_container (application->painter_mybrush_factory);
    for (auto *brush : {a.get (), b.get (), c.get ()})
      {
        MyPaint::Resource resource;
        resource.set_base_value (BRUSH_DABS_PER_SECOND, 40.);
        resource.set_base_value (BRUSH_RADIUS_LOGARITHMIC, brush == b.get () ? 2.5 : 1.5);
        resource.set_base_value (BRUSH_OPAQUE, brush == b.get () ? .15 : .95);
        PainterMybrushRef::retain (brush).replace (resource);
        set_icon (brush, brush == a.get () ? 0xe02030ff : brush == b.get () ? 0x20c040ff : 0x3050e0ff);
        add_fixture_brush (container, brush);
      }
  }
  ~Resources ()
  {
    for (auto *brush : {a.get (), b.get (), c.get ()})
      if (gimp_container_have (container, GIMP_OBJECT (brush)))
        gimp_container_remove (container, GIMP_OBJECT (brush));
    drain ();
  }
};

GtkWidget *popup (GtkWidget *button)
{
  auto *result = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
  g_assert_nonnull (result); return result;
}

void open_popup (GtkWidget *button)
{
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE); drain ();
  g_assert_true (gtk_widget_get_visible (popup (button)));
}

void close_popup (GtkWidget *button)
{
  gtk_popover_popdown (GTK_POPOVER (popup (button))); drain ();
  g_assert_false (gtk_widget_get_visible (popup (button)));
}

GtkTreePath *preset_path (GtkWidget *button, GimpPainterMybrush *brush)
{
  auto *view = GIMP_CONTAINER_ICON_VIEW (control (popup (button), "painter-mypaint-preset-view"));
  GtkTreeIter iter;
  for (gboolean valid = gtk_tree_model_get_iter_first (view->model, &iter); valid;
       valid = gtk_tree_model_iter_next (view->model, &iter))
    {
      GimpViewRenderer *renderer = nullptr;
      gtk_tree_model_get (view->model, &iter, GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer, -1);
      const bool match = renderer->viewable == GIMP_VIEWABLE (brush);
      g_object_unref (renderer);
      if (match) return gtk_tree_model_get_path (view->model, &iter);
    }
  return nullptr;
}

void choose_preset (GtkWidget *button, GimpPainterMybrush *brush)
{
  auto *view = GIMP_CONTAINER_ICON_VIEW (control (popup (button), "painter-mypaint-preset-view"));
  auto *path = preset_path (button, brush); g_assert_nonnull (path);
  /* This is the actual GtkIconView selection-changed/item-activated path,
   * including the native GimpContainerIconView and selector callbacks. */
  gtk_icon_view_select_path (view->view, path);
  gtk_icon_view_item_activated (view->view, path);
  gtk_tree_path_free (path); drain ();
}

GtkTreePath *history_path (GtkWidget *button, guint index)
{
  auto *tree = GTK_TREE_VIEW (control (popup (button), "painter-mypaint-custom-list"));
  auto *model = gtk_tree_view_get_model (tree); GtkTreeIter iter;
  for (gboolean valid = gtk_tree_model_get_iter_first (model, &iter); valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      guint value = G_MAXUINT; gtk_tree_model_get (model, &iter, 0, &value, -1);
      if (value == index) return gtk_tree_model_get_path (model, &iter);
    }
  return nullptr;
}

void choose_history (GtkWidget *button, guint index)
{
  auto *tree = GTK_TREE_VIEW (control (popup (button), "painter-mypaint-custom-list"));
  auto *path = history_path (button, index); g_assert_nonnull (path);
  gtk_tree_view_set_cursor (tree, path, nullptr, FALSE);
  gtk_tree_view_row_activated (tree, path, gtk_tree_view_get_column (tree, 0));
  gtk_tree_path_free (path); drain ();
}

void assert_selected (GtkWidget *button, GimpPainterMybrushOptions *options,
                      GimpPainterMybrush *brush)
{
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (options)) == brush);
  auto *thumb = GIMP_VIEW (control (button, "painter-mypaint-brush-thumbnail"));
  g_assert_true (thumb->renderer->viewable == GIMP_VIEWABLE (brush));
}

struct Selector {
  ObjectRef<GimpPainterMybrushOptions> model;
  GtkWidget *window;
  GtkWidget *button;
  explicit Selector (GimpContext *context = nullptr)
  {
    gimp_set_focused_once (application); GError *error = nullptr;
    if (context) model = ObjectRef<GimpPainterMybrushOptions>::adopt (
      gimp_painter_mybrush_options_ref_for_context (context, &error));
    else model = new_options ();
    g_assert_no_error (error); g_assert_true (bool (model));
    button = gimp_mypaint_brush_button_with_popup (G_OBJECT (context ? context : GIMP_CONTEXT (model.get ())));
    g_assert_nonnull (button); g_object_ref_sink (button);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
    gtk_window_set_default_size (GTK_WINDOW (window), 300, 140);
    gtk_container_add (GTK_CONTAINER (window), button);
    gtk_widget_show_all (window); drain ();
  }
  ~Selector ()
  {
    gtk_widget_destroy (window); g_object_unref (window);
    g_object_unref (button); drain ();
  }
  MyPaint::Resource snapshot () { return PainterOptionsRef::retain (model.get ()).snapshot (); }
};

void handle_route_ownership ()
{
  auto options = new_options (); guint context_gone = 0, button_gone = 0;
  auto notify = +[](gpointer data) { ++*static_cast<guint *> (data); };
  g_object_set_data_full (G_OBJECT (options.get ()), "selector-handle-context-storage", &context_gone, notify);
  {
    auto context = ContextRef::retain (GIMP_CONTEXT (options.get ()));
    options.reset ();
    auto copy = context; auto moved = std::move (copy);
#ifdef GIMP_TEST_MYPAINT_SELECTOR_WRAP_FACTORY
    guint calls = selector_factory_calls;
#endif
    auto button = moved.create_mypaint_brush_button ();
    g_assert_true (bool (button)); g_assert_true (GTK_IS_MENU_BUTTON (button.get ()));
    g_assert_false (g_object_is_floating (button.get ()));
#ifdef GIMP_TEST_MYPAINT_SELECTOR_WRAP_FACTORY
    g_assert_cmpuint (selector_factory_calls, ==, calls + 1);
    g_assert_true (GTK_WIDGET (button.get ()) == selector_factory_result);
#endif
    g_object_set_data_full (button.get (), "selector-handle-widget-storage", &button_gone, notify);
    auto retained = button; auto transferred = std::move (retained);
    GtkWidget *host = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (host);
    gtk_container_add (GTK_CONTAINER (host), GTK_WIDGET (button.get ()));
    button.reset ();
    gtk_widget_destroy (host); g_object_unref (host);
    g_assert_cmpuint (button_gone, ==, 0);
    transferred.reset (); g_assert_cmpuint (button_gone, ==, 1);
    g_assert_cmpuint (context_gone, ==, 0);
    g_assert_true (gimp_painter_binding_close (G_OBJECT (context.get ()), nullptr));
    g_assert_false (bool (context.create_mypaint_brush_button ()));
  }
  g_assert_cmpuint (context_gone, ==, 1);
  for (auto *invalid : {static_cast<GimpContext *> (nullptr),
                        reinterpret_cast<GimpContext *> (application)})
    {
      bool rejected = false;
      try { auto value = ContextRef::retain (invalid); (void) value; }
      catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
      g_assert_true (rejected);
    }
}

void preset_selection_and_canonical_sync ()
{
  Resources brushes;
  auto context = ContextOwner (gimp_context_new (application, "selector ordinary source", nullptr));
  gimp_context_set_painter_mybrush (context.get (), brushes.a.get ());
  Selector selector (context.get ());
  g_assert_true (selector.model.get () == GIMP_PAINTER_MYBRUSH_OPTIONS (tool_info ()->tool_options));
  const guint before = gimp_painter_mybrush_options_history_size (selector.model.get ());
  open_popup (selector.button);
  for (auto *brush : {brushes.b.get (), brushes.a.get (), brushes.a.get ()})
    {
      choose_preset (selector.button, brush);
      assert_selected (selector.button, selector.model.get (), brush);
      g_assert_true (gimp_context_get_painter_mybrush (context.get ()) == brush);
    }
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (selector.model.get ()), ==, before);
  gimp_context_set_painter_mybrush (context.get (), brushes.c.get ()); drain ();
  assert_selected (selector.button, selector.model.get (), brushes.c.get ());
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.b.get ()); drain ();
  g_assert_true (gimp_context_get_painter_mybrush (context.get ()) == brushes.b.get ());
  assert_selected (selector.button, selector.model.get (), brushes.b.get ());
  auto draft = selector.snapshot (); draft.set_base_value (BRUSH_OPAQUE, .327);
  set_draft (selector.model.get (), draft); const auto encoded = selector.snapshot ().encode ();
  const guint count = gimp_painter_mybrush_options_history_size (selector.model.get ());
  for (guint i = 0; i < 5; ++i) { open_popup (selector.button); close_popup (selector.button); }
  g_assert_true (selector.snapshot ().encode () == encoded);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (selector.model.get ()), ==, count);
}

void custom_snapshot_restore_and_immutable_preview ()
{
  Resources brushes; Selector selector;
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.a.get ());
  auto draft = selector.snapshot ();
  draft.set_base_value (BRUSH_OPAQUE, .173);
  draft.set_curve (BRUSH_SMUDGE, INPUT_PRESSURE, {{0., .125}, {.5, .75}, {1., .375}});
  draft.set_text (BRUSH_TEXTURE_NAME, "Selector missing snapshot texture");
  draft.set_text (BRUSH_BRUSHMARK_NAME, "Selector missing snapshot brushmark");
  set_draft (selector.model.get (), draft); set_name (selector.model.get (), "Custom immutable draft");
  const guint index = gimp_painter_mybrush_options_history_size (selector.model.get ());
  open_popup (selector.button); choose_preset (selector.button, brushes.b.get ());
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (selector.model.get ()), ==, index + 1);
  GError *error = nullptr;
  auto preview = ObjectRef<GObject>::adopt (G_OBJECT (gimp_painter_mybrush_options_history_preview (selector.model.get (), index, &error)));
  g_assert_no_error (error); const auto original = pixels (GDK_PIXBUF (preview.get ()));
  set_icon (brushes.a.get (), 0x2040e0ff); drain ();
  auto *tree = GTK_TREE_VIEW (control (popup (selector.button), "painter-mypaint-custom-list"));
  auto *path = history_path (selector.button, index); g_assert_nonnull (path);
  GtkTreeIter iter; auto *model = gtk_tree_view_get_model (tree);
  g_assert_true (gtk_tree_model_get_iter (model, &iter, path)); gtk_tree_path_free (path);
  GdkPixbuf *shown = nullptr; gchar *name = nullptr;
  gtk_tree_model_get (model, &iter, 1, &shown, 2, &name, -1);
  g_assert_cmpstr (name, ==, "Custom immutable draft"); g_free (name);
  auto expected_scaled = ObjectRef<GObject>::adopt (G_OBJECT (gdk_pixbuf_scale_simple (
    GDK_PIXBUF (preview.get ()), gdk_pixbuf_get_width (shown), gdk_pixbuf_get_height (shown), GDK_INTERP_BILINEAR)));
  g_assert_true (pixels (shown) == pixels (GDK_PIXBUF (expected_scaled.get ()))); g_object_unref (shown);
  choose_history (selector.button, index);
  assert_selected (selector.button, selector.model.get (), brushes.a.get ());
  g_assert_true (selector.snapshot ().encode () == draft.encode ());
  String restored_name (gimp_painter_mybrush_options_dup_draft_name (selector.model.get (), &error));
  g_assert_no_error (error); g_assert_cmpstr (restored_name.get (), ==, "Custom immutable draft");
  gboolean settings_dirty = FALSE, name_dirty = FALSE;
  g_object_get (selector.model.get (), "painter-dirty", &settings_dirty, "painter-name-dirty", &name_dirty, nullptr);
  g_assert_true (settings_dirty); g_assert_true (name_dirty);
  auto source_icon = ObjectRef<GObject>::adopt (G_OBJECT (gimp_painter_mybrush_ref_icon (brushes.a.get ())));
  g_assert_true (pixels (GDK_PIXBUF (source_icon.get ())) != original);
  const guint before = gimp_painter_mybrush_options_history_size (selector.model.get ());
  open_popup (selector.button); close_popup (selector.button);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (selector.model.get ()), ==, before);
  /* Native deliberate repeated Custom activation appends the still-dirty
   * draft. It must not be replaced with legacy clean-object semantics. */
  choose_history (selector.button, index);
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (selector.model.get ()), ==, before + 1);
  g_assert_true (selector.snapshot ().encode () == draft.encode ());
}

void duplicate_history_names_refresh_and_teardown ()
{
  Resources brushes; Selector selector;
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.a.get ());
  auto first = selector.snapshot (); first.set_base_value (BRUSH_OPAQUE, .111);
  set_draft (selector.model.get (), first); set_name (selector.model.get (), "Repeated custom name");
  guint first_index = gimp_painter_mybrush_options_history_size (selector.model.get ());
  open_popup (selector.button); choose_preset (selector.button, brushes.b.get ());
  auto second = selector.snapshot (); second.set_base_value (BRUSH_OPAQUE, .777);
  set_draft (selector.model.get (), second); set_name (selector.model.get (), "Repeated custom name");
  guint second_index = gimp_painter_mybrush_options_history_size (selector.model.get ());
  choose_preset (selector.button, brushes.c.get ());
  g_assert_cmpuint (second_index, ==, first_index + 1);
  choose_history (selector.button, first_index);
  g_assert_cmpfloat_with_epsilon (selector.snapshot ().base_value (BRUSH_OPAQUE), .111, 1e-8);
  choose_history (selector.button, second_index);
  g_assert_cmpfloat_with_epsilon (selector.snapshot ().base_value (BRUSH_OPAQUE), .777, 1e-8);
  g_assert_true (gimp_context_get_painter_mybrush (GIMP_CONTEXT (selector.model.get ())) == brushes.b.get ());
  /* Queue the application's deferred history notification and destroy its
   * viewer before pumping. Old row paths/callback owners must not be reused. */
  set_name (selector.model.get (), "Pending notification at destruction");
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.c.get ());
  gtk_widget_destroy (selector.button); drain ();
  g_assert_true (BindingStore::require (G_OBJECT (selector.button)).state () == BindingStore::State::closed);
}

void popup_event (GtkWidget *popover, GdkEventType type, double x = 0., double y = 0.)
{
  if (type == GDK_KEY_PRESS || type == GDK_BUTTON_PRESS)
    {
      const auto deadline = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
      while ((!gtk_widget_get_window (popover) || !gdk_window_is_viewable (gtk_widget_get_window (popover))) &&
             g_get_monotonic_time () < deadline)
        { g_main_context_iteration (nullptr, FALSE); g_usleep (1000); }
      g_assert_nonnull (gtk_widget_get_window (popover));
      g_assert_true (gdk_window_is_viewable (gtk_widget_get_window (popover)));
    }
  auto *event = gdk_event_new (type);
  auto *seat = gdk_display_get_default_seat (gtk_widget_get_display (popover));
  event->any.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (popover)));
  event->any.send_event = TRUE;
  if (type == GDK_KEY_PRESS)
    {
      event->key.time = GDK_CURRENT_TIME; event->key.keyval = GDK_KEY_Escape;
      gdk_event_set_device (event, gdk_seat_get_keyboard (seat));
    }
  else
    {
      event->button.time = GDK_CURRENT_TIME; event->button.button = 1;
      event->button.x = x; event->button.y = y;
      gdk_event_set_device (event, gdk_seat_get_pointer (seat));
    }
  gtk_widget_event (popover, event); gdk_event_free (event);
}

void popup_repeat_pages_and_native_dismissal ()
{
  Resources brushes; Selector first, second;
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (first.model.get ()), brushes.a.get ());
  auto draft = first.snapshot (); draft.set_base_value (BRUSH_OPAQUE, .421); set_draft (first.model.get (), draft);
  const guint before = gimp_painter_mybrush_options_history_size (first.model.get ());
  auto *popover = popup (first.button);
  auto *tabs = GTK_NOTEBOOK (control (popover, "painter-mypaint-brush-tabs"));
  auto *other_tabs = GTK_NOTEBOOK (control (popup (second.button), "painter-mypaint-brush-tabs"));
  g_assert_cmpint (gtk_notebook_get_n_pages (tabs), ==, 2);
  gtk_notebook_set_current_page (tabs, 1); gtk_notebook_set_current_page (other_tabs, 0);
  for (guint cycle = 0; cycle < 4; ++cycle)
    {
      open_popup (first.button); g_assert_true (popup (first.button) == popover);
      g_assert_cmpint (gtk_notebook_get_current_page (tabs), ==, 1);
      g_assert_cmpint (gtk_notebook_get_current_page (other_tabs), ==, 0);
      g_assert_true (gtk_widget_has_grab (popover));
      if (cycle % 2) {
        popup_event (popover, GDK_BUTTON_PRESS, -10., -10.);
        popup_event (popover, GDK_BUTTON_RELEASE, -10., -10.);
      } else popup_event (popover, GDK_KEY_PRESS);
      drain (); g_assert_false (gtk_widget_get_visible (popover));
      g_assert_false (gtk_widget_has_grab (popover));
      g_assert_false (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (first.button)));
    }
  g_assert_true (first.snapshot ().encode () == draft.encode ());
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (first.model.get ()), ==, before);
  open_popup (first.button); choose_preset (first.button, brushes.b.get ()); close_popup (first.button);
  assert_selected (first.button, first.model.get (), brushes.b.get ());
}

struct ReenterSelection {
  GimpPainterMybrush *trigger;
  GimpPainterMybrush *replacement;
  bool fired = false;
};

void reenter_selection (GimpContext *context, GimpPainterMybrush *brush, gpointer data)
{
  auto& request = *static_cast<ReenterSelection *> (data);
  if (!request.fired && brush == request.trigger)
    { request.fired = true; gimp_context_set_painter_mybrush (context, request.replacement); }
}

void reentrant_newer_selection_wins ()
{
  Resources brushes;
  for (bool before_controller : {true, false})
    {
      auto context = ContextOwner (gimp_context_new (application, "selector reentrant source", nullptr));
      gimp_context_set_painter_mybrush (context.get (), brushes.a.get ());
      ReenterSelection request{brushes.b.get (), brushes.c.get (), false};
      gulong connection = 0;
      if (before_controller) connection = g_signal_connect (context.get (), "painter-mybrush-changed", G_CALLBACK (reenter_selection), &request);
      {
        Selector selector (context.get ());
        if (!before_controller) connection = g_signal_connect (context.get (), "painter-mybrush-changed", G_CALLBACK (reenter_selection), &request);
        choose_preset (selector.button, brushes.b.get ());
        g_assert_true (request.fired);
        assert_selected (selector.button, selector.model.get (), brushes.c.get ());
        g_assert_true (gimp_context_get_painter_mybrush (context.get ()) == brushes.c.get ());
        /* Reopening must not reacquire the model from stale source A or B. */
        open_popup (selector.button); close_popup (selector.button);
        assert_selected (selector.button, selector.model.get (), brushes.c.get ());
        g_signal_handler_disconnect (context.get (), connection);
      }
    }
  /* Native GtkIconView::selection-changed can reenter while the outer
   * gimp_container_view_set_1_selected() is unselecting its former item. A
   * nested canonical selection must also win the final visible grid state. */
  {
    Selector selector;
    gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.a.get ()); drain ();
    auto *view = GIMP_CONTAINER_ICON_VIEW (control (popup (selector.button), "painter-mypaint-preset-view"));
    struct NativeReentry { GimpPainterMybrushOptions *options; GimpPainterMybrush *trigger; GimpPainterMybrush *newer; bool fired; };
    NativeReentry request{selector.model.get (), brushes.b.get (), brushes.c.get (), false};
    auto connection = g_signal_connect (view->view, "selection-changed", G_CALLBACK (+[](GtkIconView *native, gpointer data) {
      auto& request = *static_cast<NativeReentry *> (data);
      auto *items = gtk_icon_view_get_selected_items (native);
      const bool empty = items == nullptr;
      g_list_free_full (items, +[](gpointer item) { gtk_tree_path_free (static_cast<GtkTreePath *> (item)); });
      if (!request.fired && empty &&
          gimp_context_get_painter_mybrush (GIMP_CONTEXT (request.options)) == request.trigger)
        {
          request.fired = true;
          gimp_context_set_painter_mybrush (GIMP_CONTEXT (request.options), request.newer);
        }
    }), &request);
    gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.b.get ()); drain ();
    g_assert_true (request.fired);
    assert_selected (selector.button, selector.model.get (), brushes.c.get ());
    g_assert_true (gimp_container_view_get_1_selected (GIMP_CONTAINER_VIEW (view)) == GIMP_VIEWABLE (brushes.c.get ()));
    g_signal_handler_disconnect (view->view, connection);
  }
}

struct CloseSelection {
  GtkWidget *button;
  GimpPainterMybrush *trigger;
  guint action;
  bool fired = false;
};

void close_on_selection (GimpContext *context, GimpPainterMybrush *brush, gpointer data)
{
  auto& request = *static_cast<CloseSelection *> (data);
  if (request.fired || brush != request.trigger) return;
  request.fired = true;
  if (request.action == 1) g_assert_true (gimp_painter_binding_close (G_OBJECT (context), nullptr));
  else if (request.action == 2) g_object_run_dispose (G_OBJECT (context));
  else gtk_widget_destroy (request.button);
}

void reentrant_destroy_and_model_close ()
{
  Resources brushes;
  for (guint action : {0u, 1u, 2u})
    {
      Selector selector;
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.a.get ());
      CloseSelection request{selector.button, brushes.b.get (), action, false};
      auto connection = g_signal_connect (selector.model.get (), "painter-mybrush-changed", G_CALLBACK (close_on_selection), &request);
      auto *view = GIMP_CONTAINER_ICON_VIEW (control (popup (selector.button), "painter-mypaint-preset-view"));
      auto owner = ObjectRef<GObject>::retain (G_OBJECT (view->view));
      auto retained_popup = ObjectRef<GObject>::retain (G_OBJECT (popup (selector.button)));
      open_popup (selector.button);
      auto *path = preset_path (selector.button, brushes.b.get ()); g_assert_nonnull (path);
      gtk_icon_view_select_path (view->view, path); gtk_tree_path_free (path); drain ();
      g_assert_true (request.fired);
      if (g_signal_handler_is_connected (selector.model.get (), connection))
        g_signal_handler_disconnect (selector.model.get (), connection);
      if (action != 0)
        {
          g_assert_null (gimp_mypaint_brush_button_with_popup (G_OBJECT (selector.model.get ())));
          /* The existing owner may remain allocated, but opening cannot use
           * closed options or revive an interactive view. */
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (selector.button), TRUE); drain ();
          g_assert_false (gtk_widget_get_visible (GTK_WIDGET (retained_popup.get ())));
          auto *remaining = gtk_menu_button_get_popover (GTK_MENU_BUTTON (selector.button));
          g_assert_true (!remaining || !gtk_widget_get_visible (GTK_WIDGET (remaining)));
        }
      else
        g_assert_true (BindingStore::require (G_OBJECT (selector.button)).state () == BindingStore::State::closed);
    }
  for (guint action : {0u, 1u, 2u})
    {
      Selector selector;
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.a.get ()); drain ();
      auto *view = GIMP_CONTAINER_ICON_VIEW (control (popup (selector.button), "painter-mypaint-preset-view"));
      auto native_owner = ObjectRef<GObject>::retain (G_OBJECT (view->view));
      auto list_owner = ObjectRef<GObject>::retain (G_OBJECT (view->model));
      auto popup_owner = ObjectRef<GObject>::retain (G_OBJECT (popup (selector.button)));
      struct NativeClose { GtkWidget *button; GimpPainterMybrushOptions *options; GimpPainterMybrush *trigger; guint action; bool fired; };
      NativeClose request{selector.button, selector.model.get (), brushes.b.get (), action, false};
      auto connection = g_signal_connect (native_owner.get (), "selection-changed", G_CALLBACK (+[](GtkIconView *native, gpointer data) {
        auto& request = *static_cast<NativeClose *> (data);
        auto *items = gtk_icon_view_get_selected_items (native); const bool empty = items == nullptr;
        g_list_free_full (items, +[](gpointer item) { gtk_tree_path_free (static_cast<GtkTreePath *> (item)); });
        if (!request.fired && empty &&
            gimp_context_get_painter_mybrush (GIMP_CONTEXT (request.options)) == request.trigger)
          {
            request.fired = true;
            if (request.action == 0) gtk_widget_destroy (request.button);
            else if (request.action == 1) g_assert_true (gimp_painter_binding_close (G_OBJECT (request.options), nullptr));
            else g_object_run_dispose (G_OBJECT (request.options));
          }
      }), &request);
      open_popup (selector.button);
      gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.b.get ()); drain ();
      g_assert_true (request.fired);
      g_assert_false (gtk_widget_get_visible (GTK_WIDGET (popup_owner.get ())));
      g_assert_true (BindingStore::require (G_OBJECT (selector.button)).state () == BindingStore::State::closed);
      if (g_signal_handler_is_connected (native_owner.get (), connection))
        g_signal_handler_disconnect (native_owner.get (), connection);
    }
}

void resource_rename_icon_and_remove ()
{
  Resources brushes; Selector selector;
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), brushes.a.get ()); drain ();
  auto *thumb = GIMP_VIEW (control (selector.button, "painter-mypaint-brush-thumbnail"));
  guint invalidations = 0;
  auto connection = g_signal_connect (thumb->renderer, "update", G_CALLBACK (+[](GimpViewRenderer *, gpointer data) { ++*static_cast<guint *> (data); }), &invalidations);
  const auto before = pixels (gimp_viewable_get_pixbuf (GIMP_VIEWABLE (brushes.a.get ()), GIMP_CONTEXT (selector.model.get ()), 16, 16, nullptr));
  gimp_object_set_name (GIMP_OBJECT (brushes.a.get ()), "Renamed selector resource");
  set_icon (brushes.a.get (), 0xa0b0c0ff); drain ();
  /* The generic test drain quits at DEFAULT_IDLE (200), before the native
   * renderer's VIEWABLE_IDLE (300). Wait for its actual emitted update; never
   * call the renderer update method to manufacture the notification. */
  const auto update_deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (invalidations == 0 && g_get_monotonic_time () < update_deadline)
    { g_main_context_iteration (nullptr, FALSE); g_usleep (1000); }
  assert_selected (selector.button, selector.model.get (), brushes.a.get ());
  gchar *tooltip = gtk_widget_get_tooltip_text (selector.button);
  g_assert_nonnull (tooltip); g_assert_nonnull (std::strstr (tooltip, "Renamed selector resource")); g_free (tooltip);
  const auto after = pixels (gimp_viewable_get_pixbuf (thumb->renderer->viewable, GIMP_CONTEXT (selector.model.get ()), 16, 16, nullptr));
  g_assert_true (after != before); g_assert_cmpuint (invalidations, >, 0);
  g_signal_handler_disconnect (thumb->renderer, connection);
  open_popup (selector.button);
  auto late = PainterMybrushRef::create ("Selector late factory addition");
  set_icon (late.get (), 0x70d020ff);
  add_fixture_brush (brushes.container, late.get ()); drain ();
  auto *late_path = preset_path (selector.button, late.get ()); g_assert_nonnull (late_path);
  gtk_tree_path_free (late_path);
  /* Force a native draw after a newly inserted factory row received a safe
   * renderer context, instead of checking only list-model structure. */
  auto *grid = control (popup (selector.button), "painter-mypaint-preset-view");
  auto *tabs = GTK_NOTEBOOK (control (popup (selector.button), "painter-mypaint-brush-tabs"));
  const auto mapping_deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (gtk_widget_get_visible (grid) && !gtk_widget_get_mapped (grid) &&
         g_get_monotonic_time () < mapping_deadline)
    { g_main_context_iteration (nullptr, FALSE); g_usleep (1000); }
  g_test_message ("Preset draw state: visible=%d child-visible=%d mapped=%d realized=%d page=%d popup-mapped=%d",
    gtk_widget_get_visible (grid), gtk_widget_get_child_visible (grid), gtk_widget_get_mapped (grid),
    gtk_widget_get_realized (grid), gtk_notebook_get_current_page (tabs), gtk_widget_get_mapped (popup (selector.button)));
  g_assert_true (gtk_widget_get_visible (grid));
  g_assert_true (gtk_widget_get_mapped (grid));
  auto *frame_clock = gtk_widget_get_frame_clock (grid); g_assert_nonnull (frame_clock);
  guint frames = 0;
  auto frame_connection = g_signal_connect (frame_clock, "after-paint", G_CALLBACK (+[](GdkFrameClock *, gpointer data) {
    ++*static_cast<guint *> (data);
  }), &frames);
  gtk_widget_queue_draw (grid);
  const auto frame_deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (frames == 0 && g_get_monotonic_time () < frame_deadline)
    { g_main_context_iteration (nullptr, FALSE); g_usleep (1000); }
  g_signal_handler_disconnect (frame_clock, frame_connection);
  g_assert_cmpuint (frames, >, 0); g_assert_true (gtk_widget_get_mapped (grid));
  g_assert_cmpint (gtk_widget_get_allocated_width (grid), >, 1);
  g_assert_cmpint (gtk_widget_get_allocated_height (grid), >, 1);
  auto *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
    std::max (1, gtk_widget_get_allocated_width (grid)), std::max (1, gtk_widget_get_allocated_height (grid)));
  auto *cr = cairo_create (surface); gtk_widget_draw (grid, cr);
  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
  cairo_destroy (cr); cairo_surface_destroy (surface);
  choose_preset (selector.button, late.get ());
  assert_selected (selector.button, selector.model.get (), late.get ());
  gimp_container_remove (brushes.container, GIMP_OBJECT (late.get ())); drain ();
  gimp_container_remove (brushes.container, GIMP_OBJECT (brushes.a.get ())); drain ();
  g_assert_null (preset_path (selector.button, brushes.a.get ()));
  auto *selected = gimp_context_get_painter_mybrush (GIMP_CONTEXT (selector.model.get ()));
  g_assert_true (GIMP_VIEW (control (selector.button, "painter-mypaint-brush-thumbnail"))->renderer->viewable == GIMP_VIEWABLE (selected));
  choose_preset (selector.button, brushes.b.get ());
  assert_selected (selector.button, selector.model.get (), brushes.b.get ());
}

void empty_factory_and_history_view ()
{
  gimp_set_focused_once (application);
  auto *container = gimp_data_factory_get_container (application->painter_mybrush_factory);
  std::vector<ObjectRef<GObject>> resources;
  for (gint index = 0; index < gimp_container_get_n_children (container); ++index)
    resources.push_back (ObjectRef<GObject>::retain (G_OBJECT (gimp_container_get_child_by_index (container, index))));
  struct Selection { ObjectRef<GObject> context, resource; };
  std::vector<Selection> selected;
  for (auto *item = application->context_list; item; item = item->next)
    {
      auto *context = GIMP_CONTEXT (item->data);
      selected.push_back ({ObjectRef<GObject>::retain (G_OBJECT (context)),
        ObjectRef<GObject>::retain (G_OBJECT (gimp_context_get_painter_mybrush (context)))});
    }
  gimp_container_freeze (container);
  for (const auto& resource : resources) gimp_container_remove (container, GIMP_OBJECT (resource.get ()));
  gimp_container_thaw (container);
  {
    Selector empty; open_popup (empty.button);
    auto *grid = GIMP_CONTAINER_ICON_VIEW (control (popup (empty.button), "painter-mypaint-preset-view"));
    auto *history = GTK_TREE_VIEW (control (popup (empty.button), "painter-mypaint-custom-list"));
    g_assert_cmpint (gtk_tree_model_iter_n_children (grid->model, nullptr), ==, 0);
    g_assert_cmpuint (gimp_painter_mybrush_options_history_size (empty.model.get ()), ==, 0);
    g_assert_cmpint (gtk_tree_model_iter_n_children (gtk_tree_view_get_model (history), nullptr), ==, 0);
    close_popup (empty.button);
    gimp_container_freeze (container);
    for (const auto& resource : resources) gimp_container_add (container, GIMP_OBJECT (resource.get ()));
    gimp_container_thaw (container); drain ();
    g_assert_cmpint (gimp_container_get_n_children (container), ==, static_cast<gint> (resources.size ()));
    for (const auto& resource : resources) g_assert_true (gimp_container_have (container, GIMP_OBJECT (resource.get ())));
    g_assert_cmpint (gtk_tree_model_iter_n_children (grid->model, nullptr), ==, static_cast<gint> (resources.size ()));
    gimp_context_set_painter_mybrush (GIMP_CONTEXT (empty.model.get ()), nullptr);
  }
  for (const auto& selection : selected)
    gimp_context_set_painter_mybrush (GIMP_CONTEXT (selection.context.get ()),
      reinterpret_cast<GimpPainterMybrush *> (selection.resource.get ()));
  drain ();
}

void source_less_draft_and_missing_preview ()
{
  /* Fresh process supplies genuinely empty application history. The factory
   * is changed only in memory, with each resource retained and every row and
   * original context selection restored before normal subprocess teardown. */
  if (g_test_subprocess ()) { empty_factory_and_history_view (); return; }
  g_test_trap_subprocess (nullptr, 30 * G_TIME_SPAN_SECOND, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_passed ();
  Selector selector;
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), nullptr);
  MyPaint::Resource draft; draft.set_base_value (BRUSH_OPAQUE, .219);
  set_draft (selector.model.get (), draft); set_name (selector.model.get (), "Source-less native draft");
  guint count = gimp_painter_mybrush_options_history_size (selector.model.get ());
  for (guint i = 0; i < 3; ++i) { open_popup (selector.button); close_popup (selector.button); }
  g_assert_true (selector.snapshot ().encode () == draft.encode ());
  g_assert_cmpuint (gimp_painter_mybrush_options_history_size (selector.model.get ()), ==, count);
  auto source = PainterMybrushRef::create ("No embedded preview fixture");
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), source.get ()); drain ();
  choose_history (selector.button, count);
  g_assert_true (selector.snapshot ().encode () == draft.encode ());
  GError *error = nullptr;
  g_assert_null (gimp_painter_mybrush_options_history_preview (selector.model.get (), count, &error));
  g_assert_no_error (error);
  g_assert_null (gimp_context_get_painter_mybrush (GIMP_CONTEXT (selector.model.get ())));
  auto malformed = draft; malformed.set_preview_png_base64 ("%%%");
  set_draft (selector.model.get (), malformed); set_name (selector.model.get (), "Malformed preview remains selectable");
  const guint malformed_index = gimp_painter_mybrush_options_history_size (selector.model.get ());
  gimp_context_set_painter_mybrush (GIMP_CONTEXT (selector.model.get ()), source.get ()); drain ();
  open_popup (selector.button);
  auto *tree = GTK_TREE_VIEW (control (popup (selector.button), "painter-mypaint-custom-list"));
  auto *path = history_path (selector.button, malformed_index); g_assert_nonnull (path);
  GtkTreeIter iter; auto *model = gtk_tree_view_get_model (tree);
  g_assert_true (gtk_tree_model_get_iter (model, &iter, path)); gtk_tree_path_free (path);
  GdkPixbuf *preview = nullptr; gtk_tree_model_get (model, &iter, 1, &preview, -1); g_assert_null (preview);
  auto *column = gtk_tree_view_get_column (tree, 0);
  gtk_tree_view_column_cell_set_cell_data (column, model, &iter, FALSE, FALSE);
  auto *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column)); g_assert_nonnull (cells);
  gchar *icon = nullptr; g_object_get (cells->data, "icon-name", &icon, nullptr);
  g_assert_cmpstr (icon, ==, "gimp-tool-mypaint-brush"); g_free (icon); g_list_free (cells);
  auto *status = GTK_LABEL (control (popup (selector.button), "painter-mypaint-selector-status"));
  g_assert_cmpstr (gtk_label_get_text (status), !=, "");
  choose_history (selector.button, malformed_index);
  g_assert_true (selector.snapshot ().encode () == malformed.encode ());
  close_popup (selector.button);
}

void registered_tool_gui_and_editor_identity ()
{
  Resources brushes;
  auto *info = tool_info ();
  auto *options = GIMP_PAINTER_MYBRUSH_OPTIONS (info->tool_options);
  auto *gui = gimp_tools_get_tool_options_gui (info->tool_options); g_assert_nonnull (gui);
  auto *button = control (gui, selector_name);
  auto *editor = find (gui, nullptr, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR); g_assert_nonnull (editor);
  g_assert_true (gtk_widget_get_parent (button) == gui);
  g_assert_true (gtk_widget_get_parent (editor) == gui);
  g_assert_cmpuint (count_type (gui, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR), ==, 1);
  auto *opaque = control (editor, "opaque");
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  gpointer editor_store = g_object_get_qdata (G_OBJECT (editor), key);
  gpointer selector_store = g_object_get_qdata (G_OBJECT (button), key);
  g_assert_nonnull (editor_store); g_assert_nonnull (selector_store);
  g_assert_true (editor_store != selector_store);
  choose_preset (button, brushes.b.get ());
  assert_selected (button, options, brushes.b.get ());
  g_assert_cmpfloat_with_epsilon (gtk_spin_button_get_value (GTK_SPIN_BUTTON (opaque)), .15, 1e-6);
  for (auto orientation : {GTK_ORIENTATION_HORIZONTAL, GTK_ORIENTATION_VERTICAL})
    {
      GError *error = nullptr;
      g_assert_true (gimp_painter_compact_options_set (gui, info->tool_options, TRUE, orientation, &error));
      g_assert_no_error (error);
      g_assert_true (control (gui, selector_name) == button);
      g_assert_true (gtk_widget_get_parent (button) == gui);
      auto *editor_button = control (gui, "painter-compact-mypaint");
      g_assert_true (find (popup (editor_button), nullptr, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR) == editor);
      g_assert_cmpuint (count_type (gui, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR), ==, 1);
      g_assert_true (g_object_get_qdata (G_OBJECT (editor), key) == editor_store);
      g_assert_true (g_object_get_qdata (G_OBJECT (button), key) == selector_store);
      g_assert_true (control (gui, "opaque") == opaque);
      gtk_spin_button_set_value (GTK_SPIN_BUTTON (opaque), .582);
      g_assert_cmpfloat_with_epsilon (PainterOptionsRef::retain (options).snapshot ().base_value (BRUSH_OPAQUE), .582, 1e-6);
      g_assert_true (gimp_painter_compact_options_set (gui, info->tool_options, FALSE, orientation, &error));
      g_assert_no_error (error);
      g_assert_true (gtk_widget_get_parent (editor) == gui);
      g_assert_true (gtk_widget_get_parent (button) == gui);
    }
}

/* The same native GimpTool/128px image fixture used by the focused painter
 * tool suite, retained here so selector choices precede real strokes. */
struct Scene {
  GimpDisplay *display;
  GimpImage *image;
  GimpDrawable *drawable;
  GimpTool *tool;
  GimpPainterMybrushOptions *options;
  GimpCoords coords = GIMP_COORDS_DEFAULT_VALUES;
  guint32 time = 100;
  Scene ()
  {
    gimp_set_focused_once (application); gimp_test_utils_create_image (application, 128, 128); drain ();
    display = gimp_context_get_display (gimp_get_user_context (application)); g_assert_nonnull (display);
    image = gimp_display_get_image (display);
    auto *drawables = gimp_image_get_selected_drawables (image); g_assert_nonnull (drawables);
    drawable = GIMP_DRAWABLE (g_object_ref (drawables->data)); g_list_free (drawables);
    gegl_buffer_clear (gimp_drawable_get_buffer (drawable), nullptr);
    gimp_context_set_tool (gimp_get_user_context (application), tool_info ());
    tool = GIMP_TOOL (g_object_ref (tool_manager_get_active (application)));
    g_assert_true (GIMP_IS_PAINTER_MYBRUSH_TOOL (tool));
    options = GIMP_PAINTER_MYBRUSH_OPTIONS (gimp_tool_get_options (tool));
    auto *color = gegl_color_new ("red");
    gimp_context_set_foreground (GIMP_CONTEXT (options), color); g_object_unref (color);
    coords.x = 32.; coords.y = 32.; coords.pressure = 1.; gimp_image_undo_free (image);
  }
  ~Scene ()
  {
    gimp_context_set_tool (gimp_get_user_context (application), gimp_get_tool_info (application, "gimp-paintbrush-tool"));
    g_object_unref (tool); g_object_unref (drawable); g_object_unref (image);
    gimp_display_close (display); drain ();
  }
  GimpDisplayShell *shell () { return gimp_display_get_shell (display); }
  std::vector<guchar> pixels ()
  {
    std::vector<guchar> result (128 * 128 * 4);
    gegl_buffer_get (gimp_drawable_get_buffer (drawable), GEGL_RECTANGLE (0, 0, 128, 128), 1.,
                     babl_format ("R'G'B'A u8"), result.data (), GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
    return result;
  }
  gint depth () { return gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)); }
  void motion (bool down, guint32 elapsed = 20)
  {
    time += elapsed;
    gimp_tool_motion (tool, &coords, time, down ? GDK_BUTTON1_MASK : GdkModifierType (0), display);
  }
  void begin ()
  {
    coords.x = 32.; coords.y = 32.; coords.pressure = 1.; motion (false);
    gimp_tool_button_press (tool, &coords, time, GdkModifierType (0), GIMP_BUTTON_PRESS_NORMAL, display);
  }
  void stroke (bool cancel = false)
  {
    begin ();
    for (guint i = 0; i < 5; ++i) { coords.x += 9.; coords.y += 3.; motion (true, 35); }
    time += 20;
    gimp_tool_button_release (tool, &coords, time, cancel ? GDK_BUTTON3_MASK : GdkModifierType (0), display);
    if (!cancel) gimp_tool_control (tool, GIMP_TOOL_ACTION_COMMIT, display);
  }
};

void actual_canvas_transfer_and_tool_switch ()
{
  gboolean previous = application->config->painter_canvas_ui;
  g_object_set (application->config, "painter-canvas-ui", FALSE, nullptr);
  {
    Scene scene;
    auto *gui = gimp_tools_get_tool_options_gui (GIMP_TOOL_OPTIONS (scene.options));
    auto *button = control (gui, selector_name);
    auto *editor = find (gui, nullptr, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR); g_assert_nonnull (editor);
    auto *opaque = control (editor, "opaque");
    gimp_painter_canvas_ui_set_visible (scene.shell (), TRUE); drain ();
    g_assert_true (gimp_painter_compact_options_get (gui));
    g_assert_true (gtk_widget_get_ancestor (gui, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (scene.shell ()));
    g_assert_true (control (scene.shell ()->canvas, selector_name) == button);
    open_popup (button);
    gtk_button_clicked (GTK_BUTTON (control (scene.shell ()->canvas, "painter-canvas-options"))); drain ();
    g_assert_false (gimp_painter_compact_options_get (gui));
    g_assert_false (gtk_widget_get_visible (popup (button)));
    g_assert_true (find (gui, nullptr, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR) == editor);
    gtk_button_clicked (GTK_BUTTON (control (scene.shell ()->canvas, "painter-canvas-compact"))); drain ();
    g_assert_true (gimp_painter_compact_options_get (gui));
    gimp_test_utils_create_image (application, 96, 96); drain ();
    auto *other = gimp_context_get_display (gimp_get_user_context (application));
    auto *other_image = gimp_display_get_image (other);
    g_assert_true (other != scene.display);
    auto *other_shell = gimp_display_get_shell (other);
    gimp_painter_canvas_ui_set_visible (other_shell, TRUE); drain ();
    g_assert_true (gtk_widget_get_ancestor (gui, GIMP_TYPE_DISPLAY_SHELL) == GTK_WIDGET (other_shell));
    g_assert_true (control (gui, selector_name) == button);
    g_assert_true (control (gui, "opaque") == opaque);
    g_assert_true (find (gui, nullptr, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR) == editor);
    g_assert_cmpuint (count_type (gui, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR), ==, 1);
    open_popup (button);
    gimp_context_set_tool (gimp_get_user_context (application), gimp_get_tool_info (application, "gimp-rect-select-tool")); drain ();
    g_assert_false (gtk_widget_get_visible (popup (button)));
    g_assert_false (gimp_painter_compact_options_get (gui));
    gimp_context_set_tool (gimp_get_user_context (application), tool_info ()); drain ();
    g_assert_true (control (gui, selector_name) == button);
    g_assert_true (find (gui, nullptr, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR) == editor);
    gimp_painter_canvas_ui_set_visible (other_shell, FALSE); drain ();
    gimp_display_delete (other); g_object_unref (other_image); drain ();
  }
  g_object_set (application->config, "painter-canvas-ui", previous, nullptr);
}

void preset_custom_real_stroke_undo_cancel ()
{
  Resources brushes;
  gboolean previous = application->config->painter_canvas_ui;
  {
    Scene scene;
    gimp_painter_canvas_ui_set_visible (scene.shell (), TRUE); drain ();
    auto *gui = gimp_tools_get_tool_options_gui (GIMP_TOOL_OPTIONS (scene.options));
    auto *button = control (gui, selector_name);
    const auto baseline = scene.pixels ();
    std::vector<std::vector<guchar>> painted;
    for (auto *brush : {brushes.a.get (), brushes.b.get ()})
      {
        open_popup (button); choose_preset (button, brush); close_popup (button);
        assert_selected (button, scene.options, brush);
        scene.stroke (); painted.push_back (scene.pixels ());
        g_assert_true (painted.back () != baseline); g_assert_cmpint (scene.depth (), ==, 1);
        g_assert_true (gimp_image_undo (scene.image)); g_assert_true (scene.pixels () == baseline);
        g_assert_cmpint (scene.depth (), ==, 0); gimp_image_undo_free (scene.image);
      }
    g_assert_true (painted[0] != painted[1]);
    auto draft = PainterOptionsRef::retain (scene.options).snapshot ();
    draft.set_base_value (BRUSH_OPAQUE, .53); draft.set_base_value (BRUSH_RADIUS_LOGARITHMIC, 1.9);
    set_draft (scene.options, draft); set_name (scene.options, "Selector stroke custom snapshot");
    const guint index = gimp_painter_mybrush_options_history_size (scene.options);
    choose_preset (button, brushes.a.get ());
    open_popup (button); choose_history (button, index); close_popup (button);
    g_assert_true (PainterOptionsRef::retain (scene.options).snapshot ().encode () == draft.encode ());
    scene.stroke (); const auto custom_pixels = scene.pixels ();
    g_assert_true (custom_pixels != baseline);
    g_assert_true (custom_pixels != painted[0]); g_assert_true (custom_pixels != painted[1]);
    g_assert_cmpint (scene.depth (), ==, 1);
    g_assert_true (gimp_image_undo (scene.image)); g_assert_true (scene.pixels () == baseline);
    g_assert_cmpint (scene.depth (), ==, 0); gimp_image_undo_free (scene.image);
    scene.stroke (true);
    g_assert_true (scene.pixels () == baseline); g_assert_cmpint (scene.depth (), ==, 0);
    g_assert_false (gimp_painter_mybrush_tool_has_pending_stroke (GIMP_PAINTER_MYBRUSH_TOOL (scene.tool)));
  }
  g_object_set (application->config, "painter-canvas-ui", previous, nullptr);
}
} // namespace

extern "C" void
gimp_test_mypaint_selector_cpp_register (Gimp *gimp)
{
  application = gimp;
  /* Disable GTK animations for bounded native dismissal checks. This changes
   * only the process-local test GtkSettings and does not save user settings. */
  g_object_set (gtk_settings_get_default (), "gtk-enable-animations", FALSE, nullptr);
  g_test_add_func ("/mypaint-selector/cpp/01-handle-route-ownership", handle_route_ownership);
  g_test_add_func ("/mypaint-selector/cpp/02-preset-canonical-sync", preset_selection_and_canonical_sync);
  g_test_add_func ("/mypaint-selector/cpp/03-custom-immutable-snapshot", custom_snapshot_restore_and_immutable_preview);
  g_test_add_func ("/mypaint-selector/cpp/04-duplicate-history-refresh-teardown", duplicate_history_names_refresh_and_teardown);
  g_test_add_func ("/mypaint-selector/cpp/05-popup-repeat-native-dismissal", popup_repeat_pages_and_native_dismissal);
  g_test_add_func ("/mypaint-selector/cpp/06-reentrant-newer-selection", reentrant_newer_selection_wins);
  g_test_add_func ("/mypaint-selector/cpp/07-reentrant-destroy-close", reentrant_destroy_and_model_close);
  g_test_add_func ("/mypaint-selector/cpp/08-resource-rename-icon-remove", resource_rename_icon_and_remove);
  g_test_add_func ("/mypaint-selector/cpp/09-source-less-missing-preview", source_less_draft_and_missing_preview);
  g_test_add_func ("/mypaint-selector/cpp/10-registered-editor-identity", registered_tool_gui_and_editor_identity);
  g_test_add_func ("/mypaint-selector/cpp/11-canvas-transfer-tool-switch", actual_canvas_transfer_and_tool_switch);
  g_test_add_func ("/mypaint-selector/cpp/12-preset-custom-stroke-undo-cancel", preset_custom_real_stroke_undo_cancel);
  register_selector_history_tests (application);
}
