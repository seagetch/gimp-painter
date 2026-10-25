/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Common layer hosts only. Filter's staged Apply editor and secondary dialogs
 * retain their own registrations and are deliberately not emulated here. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <gmodule.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <initializer_list>
#include <string>
#include <vector>
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimpundostack.h"
#include "core/gimplayer.h"
#include "core/gimpclonelayer.h"
#include "core/gimpfilterlayer.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpdrawable.h"
#include "config/gimpcoreconfig.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpwindowstrategy.h"
#include "widgets/gimppainterlayertiles.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpitemtreeview.h"
#include "widgets/gimplayertreeview.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimpcellrendererpopup.h"
#include "widgets/gimplayermodebox.h"
#include "widgets/gimplayermodecombobox.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
void gimp_test_common_layer_popup_register (Gimp *application);
void gimp_test_common_native_dock_case (Gimp *,
  void (*) (GtkWidget *, GtkWidget *, gpointer),
  void (*) (GtkWidget *, GtkWidget *, gpointer),
  void (*) (GtkWidget *, gpointer), gpointer);
}
namespace {
Gimp *app;
/* Keep one live native tab so closing a destructive fixture retires its own
 * shell, not the restored single-window container and dock collection. */
GimpDisplay *keeper = nullptr;
GtkWidget *keeper_window = nullptr;
void drain () {
  auto until = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  while (g_main_context_pending (nullptr) && g_get_monotonic_time () < until)
    g_main_context_iteration (nullptr, FALSE);
}
void settle () {
  auto until = g_get_monotonic_time () + 80 * G_TIME_SPAN_MILLISECOND;
  do { g_main_context_iteration (nullptr, FALSE); g_usleep (1000); }
  while (g_get_monotonic_time () < until);
}
GtkWidget *find (GtkWidget *widget, const char *name, GType type = 0) {
  if ((!name || !g_strcmp0 (gtk_widget_get_name (widget), name)) &&
      (!type || G_TYPE_CHECK_INSTANCE_TYPE (widget, type))) return widget;
  if (!GTK_IS_CONTAINER (widget)) return nullptr;
  auto *children = gtk_container_get_children (GTK_CONTAINER (widget));
  GtkWidget *result = nullptr;
  for (auto *i = children; i && !result; i = i->next)
    result = find (GTK_WIDGET (i->data), name, type);
  g_list_free (children); return result;
}
GtkWidget *field (GtkWidget *widget, const char *name) {
  auto *result = find (widget, name); g_assert_nonnull (result); return result;
}
bool require_input () {
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ())) return true;
#endif
  g_test_skip ("Actual common layer input requires the X11/XTest runtime"); return false;
}
/* Opt-in passive diagnostics for a native dockbook drawing failure. Normal
 * acceptance installs no hooks and retains no additional GTK objects. */
struct DockTrace {
  struct Book {
    DockTrace *trace; GtkWidget *widget;
    std::vector<gulong> handlers;
    std::string last_draw;
    guint allocations = 0;
  };
  struct Hook { guint signal; gulong id; };
  FILE *file = nullptr;
  std::vector<std::unique_ptr<Book>> books;
  std::vector<Hook> hooks;
  const char *phase = "registration";
  guint phase_number = 0;
  gsize bytes = 0;
  bool truncated = false;
  explicit DockTrace (const char *path) {
    file = fopen (path, "w"); g_assert_nonnull (file);
    /* Registration precedes GUI startup; type registration alone does not
     * initialize GtkNotebook's class or install its signals. */
    auto *notebook_class = g_type_class_ref (GTK_TYPE_NOTEBOOK);
    for (auto *name : {"realize", "draw"}) {
      auto signal = g_signal_lookup (name, GTK_TYPE_WIDGET);
      auto id = g_signal_add_emission_hook (signal, 0, observe_widget, this, nullptr);
      g_assert_cmpuint (id, >, 0); hooks.push_back ({signal, id});
    }
    auto signal = g_signal_lookup ("page-added", GTK_TYPE_NOTEBOOK);
    auto id = g_signal_add_emission_hook (signal, 0, observe_widget, this, nullptr);
    g_assert_cmpuint (id, >, 0); hooks.push_back ({signal, id});
    g_type_class_unref (notebook_class);
    auto *clock_class = g_type_class_ref (GDK_TYPE_FRAME_CLOCK);
    for (auto *name : {"before-paint", "layout", "paint", "after-paint"}) {
      auto signal = g_signal_lookup (name, GDK_TYPE_FRAME_CLOCK);
      auto id = g_signal_add_emission_hook (signal, 0, observe_clock, this, nullptr);
      g_assert_cmpuint (id, >, 0); hooks.push_back ({signal, id});
    }
    g_type_class_unref (clock_class);
    auto *windows = gtk_window_list_toplevels ();
    for (auto *i = windows; i; i = i->next) discover (GTK_WIDGET (i->data), 0);
    g_list_free (windows);
  }
  ~DockTrace () {
    for (auto hook : hooks) g_signal_remove_emission_hook (hook.signal, hook.id);
    for (auto& book : books) if (book->widget) {
      for (auto handler : book->handlers)
        if (g_signal_handler_is_connected (book->widget, handler))
          g_signal_handler_disconnect (book->widget, handler);
      g_object_remove_weak_pointer (G_OBJECT (book->widget), reinterpret_cast<gpointer *> (&book->widget));
    }
    fclose (file);
  }
  void write (const char *text) {
    if (truncated) return;
    const auto length = strlen (text);
    const char *limit = "TRACE LIMIT REACHED\n";
    if (bytes + length > 2 * 1024 * 1024 - strlen (limit)) {
      fputs (limit, file); fflush (file); truncated = true; return;
    }
    fwrite (text, 1, length, file); fflush (file); bytes += length;
  }
  static const char *test_path () {
    const auto *path = g_test_get_path (); return path ? path : "not-running";
  }
  void set_phase (const char *value) {
    phase = value; ++phase_number;
    auto *line = g_strdup_printf ("phase=%u %s test=%s\n", phase_number, phase, test_path ());
    write (line); g_free (line);
  }
  static void append_widget (GString *text, const char *role, GtkWidget *widget) {
    if (!widget) { g_string_append_printf (text, " %s=null", role); return; }
    GtkAllocation a; gtk_widget_get_allocation (widget, &a);
    g_string_append_printf (text, " %s=%p:%s parent=%p visible=%d child-visible=%d mapped=%d realized=%d alloc=%d,%d,%d,%d",
      role, static_cast<void *> (widget), G_OBJECT_TYPE_NAME (widget),
      static_cast<void *> (gtk_widget_get_parent (widget)), gtk_widget_get_visible (widget),
      gtk_widget_get_child_visible (widget), gtk_widget_get_mapped (widget), gtk_widget_get_realized (widget),
      a.x, a.y, a.width, a.height);
  }
  static void append_window (GString *text, const char *role, GdkWindow *window) {
    if (!window) { g_string_append_printf (text, " %s=null", role); return; }
    g_string_append_printf (text, " %s=%p type=%d clock=%p", role, static_cast<void *> (window),
      gdk_window_get_window_type (window), static_cast<void *> (gdk_window_get_frame_clock (window)));
  }
  static gboolean observe_clock (GSignalInvocationHint *hint, guint, const GValue *values, gpointer data) {
    auto& trace = *static_cast<DockTrace *> (data);
    /* Observe only the bounded native insertion/settle window. No phase is
     * requested and no clock is retained or started by this diagnostic. */
    if (strcmp (trace.phase, "fixture/native-dock-open") &&
        strcmp (trace.phase, "fixture/native-dock-settle") &&
        strcmp (trace.phase, "fixture/controlled-offscreen-updates")) return TRUE;
    auto *clock = GDK_FRAME_CLOCK (g_value_get_object (&values[0]));
    auto *line = g_strdup_printf ("phase=%u %s test=%s event=clock/%s clock=%p frame=%" G_GINT64_FORMAT " monotonic=%" G_GINT64_FORMAT "\n",
      trace.phase_number, trace.phase, test_path (), g_signal_name (hint->signal_id),
      static_cast<void *> (clock), gdk_frame_clock_get_frame_counter (clock), g_get_monotonic_time ());
    trace.write (line); g_free (line);
    return TRUE; // Retain the observer, not an event/phase handling result.
  }
  void record (Book& book, const char *event) {
    if (truncated || !book.widget) return;
    auto *notebook = GTK_NOTEBOOK (book.widget);
    auto *text = g_string_new (nullptr);
    g_string_append_printf (text, "phase=%u %s test=%s event=%s allocations=%u current=%d pages=%d",
      phase_number, phase, test_path (), event, book.allocations,
      gtk_notebook_get_current_page (notebook), gtk_notebook_get_n_pages (notebook));
    append_widget (text, "book", book.widget);
    g_string_append_printf (text, " widget-clock=%p", static_cast<void *> (gtk_widget_get_frame_clock (book.widget)));
    auto *native = gtk_widget_get_window (book.widget);
    append_window (text, "native", native);
    append_window (text, "native-toplevel", native ? gdk_window_get_toplevel (native) : nullptr);
    append_window (text, "effective-toplevel", native ? gdk_window_get_effective_toplevel (native) : nullptr);
    auto *parent = gtk_widget_get_parent (book.widget);
    for (guint n = 0; parent && n < 8; ++n, parent = gtk_widget_get_parent (parent))
      append_widget (text, "ancestor", parent);
    g_string_append_c (text, '\n');
    const auto pages = std::min (16, gtk_notebook_get_n_pages (notebook));
    for (gint n = 0; n < pages; ++n) {
      auto *page = gtk_notebook_get_nth_page (notebook, n);
      g_string_append_printf (text, " page-index=%d", n);
      append_widget (text, "page", page);
      /* Enumerate only current members, never query a removed signal page. */
      append_widget (text, "tab", gtk_notebook_get_tab_label (notebook, page));
      auto *identifier = static_cast<const char *> (g_object_get_data (G_OBJECT (page), "gimp-dialog-identifier"));
      g_string_append_printf (text, " dock-id=%s\n", identifier ? identifier : "none");
    }
    if (strcmp (event, "draw-before-default") || book.last_draw != text->str) {
      if (!strcmp (event, "draw-before-default")) book.last_draw = text->str;
      write (text->str);
    }
    g_string_free (text, TRUE);
  }
  static void allocated (GtkWidget *, GtkAllocation *, gpointer data) {
    auto& book = *static_cast<Book *> (data); ++book.allocations;
    book.trace->record (book, "size-allocate-after-default");
  }
  static void page_event (GtkNotebook *, GtkWidget *, guint, gpointer data) {
    auto& book = *static_cast<Book *> (data);
    book.trace->record (book, g_signal_name (g_signal_get_invocation_hint (book.widget)->signal_id));
  }
  static void widget_event (GtkWidget *, gpointer data) {
    auto& book = *static_cast<Book *> (data);
    book.trace->record (book, g_signal_name (g_signal_get_invocation_hint (book.widget)->signal_id));
  }
  static void parent_event (GtkWidget *, GtkWidget *, gpointer data) {
    auto& book = *static_cast<Book *> (data); book.trace->record (book, "parent-set-after-default");
  }
  Book *watch (GtkWidget *widget) {
    if (!GIMP_IS_DOCKBOOK (widget)) return nullptr;
    for (auto& book : books) if (book->widget == widget) return book.get ();
    if (books.size () >= 128) return nullptr;
    auto book = std::make_unique<Book> (); book->trace = this; book->widget = widget;
    auto *result = book.get (); books.push_back (std::move (book));
    g_object_add_weak_pointer (G_OBJECT (widget), reinterpret_cast<gpointer *> (&result->widget));
    result->handlers.push_back (g_signal_connect_after (widget, "size-allocate", G_CALLBACK (allocated), result));
    for (auto *name : {"page-added", "page-removed", "switch-page"})
      result->handlers.push_back (g_signal_connect_after (widget, name, G_CALLBACK (page_event), result));
    for (auto *name : {"map", "unmap", "destroy"})
      result->handlers.push_back (g_signal_connect_after (widget, name, G_CALLBACK (widget_event), result));
    result->handlers.push_back (g_signal_connect_after (widget, "parent-set", G_CALLBACK (parent_event), result));
    record (*result, "discovered"); return result;
  }
  void discover (GtkWidget *widget, guint depth) {
    watch (widget);
    if (depth >= 64 || !GTK_IS_CONTAINER (widget)) return;
    auto *children = gtk_container_get_children (GTK_CONTAINER (widget));
    for (auto *i = children; i; i = i->next) discover (GTK_WIDGET (i->data), depth + 1);
    g_list_free (children);
  }
  static gboolean observe_widget (GSignalInvocationHint *hint, guint, const GValue *values, gpointer data) {
    auto& trace = *static_cast<DockTrace *> (data);
    auto *book = trace.watch (GTK_WIDGET (g_value_get_object (&values[0])));
    if (book) trace.record (*book, !strcmp (g_signal_name (hint->signal_id), "draw") ?
      "draw-before-default" : g_signal_name (hint->signal_id));
    return TRUE; // Keep this observer; never handle the widget's signal.
  }
};
std::unique_ptr<DockTrace> dock_trace;
void dock_phase (const char *phase) { if (dock_trace) dock_trace->set_phase (phase); }

/* ACTIVE diagnostic scheduling, distinct from passive observation and normal
 * acceptance: process only updates already pending on this exact offscreen
 * implementation before the normal GTK settle loop. A return without a
 * warning is inconclusive; the API may have had no pending update to paint. */
void controlled_dock_updates (GtkWidget *dockable) {
  const auto *flag = g_getenv ("GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES");
  if (!flag) return;
  g_assert_cmpstr (flag, ==, "1"); g_assert_nonnull (dock_trace.get ());
  auto *book = gtk_widget_get_parent (dockable); g_assert_true (GIMP_IS_DOCKBOOK (book));
  auto *native = gtk_widget_get_window (book); g_assert_nonnull (native);
  auto *offscreen = gdk_window_get_toplevel (native); g_assert_nonnull (offscreen);
  g_assert_cmpint (gdk_window_get_window_type (offscreen), ==, GDK_WINDOW_OFFSCREEN);
  g_object_ref (offscreen);
  dock_phase ("fixture/controlled-offscreen-updates");
  auto *text = g_string_new ("ACTIVE-CONTROLLED-OFFSCREEN-UPDATES BEGIN");
  DockTrace::append_widget (text, "book", book);
  DockTrace::append_widget (text, "dockable", dockable);
  DockTrace::append_window (text, "offscreen", offscreen);
  g_string_append_printf (text, " widget-clock=%p\n", static_cast<void *> (gtk_widget_get_frame_clock (book)));
  dock_trace->write (text->str); g_string_free (text, TRUE);
  /* Public but deprecated since GTK 3.22. This is an explicit scheduling
   * probe, not a production rendering API or an allocation workaround. */
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_process_updates (offscreen, FALSE);
  G_GNUC_END_IGNORE_DEPRECATIONS
  /* Dispatch may reenter and destroy widgets/windows. Only the retained
   * window object is inspected after it; no borrowed widget is dereferenced. */
  auto *returned = g_strdup_printf ("ACTIVE-CONTROLLED-OFFSCREEN-UPDATES RETURNED window=%p destroyed=%d\n",
    static_cast<void *> (offscreen), gdk_window_is_destroyed (offscreen));
  dock_trace->write (returned); g_free (returned); g_object_unref (offscreen);
}

struct Input {
  GModule *module;
  struct Hook { guint signal; gulong id; };
  std::vector<Hook> hooks;
  std::vector<std::string> trace;
  guint observations = 0;
  guint presses = 0, doubles = 0, triples = 0, releases = 0;
  bool diagnostic = g_getenv ("GIMP_TEST_COMMON_POPUP_DIAGNOSTICS") != nullptr;
  gint (*motion) (gpointer, gint, gint, gint, gulong);
  gint (*button) (gpointer, guint, gint, gulong);
  gint (*keycode) (gpointer, guint, gint, gulong);
  Input () {
    module = g_module_open ("libXtst.so.6", G_MODULE_BIND_LAZY); g_assert_nonnull (module);
    g_assert_true (g_module_symbol (module, "XTestFakeMotionEvent", reinterpret_cast<gpointer *> (&motion)));
    g_assert_true (g_module_symbol (module, "XTestFakeButtonEvent", reinterpret_cast<gpointer *> (&button)));
    g_assert_true (g_module_symbol (module, "XTestFakeKeyEvent", reinterpret_cast<gpointer *> (&keycode)));
    if (diagnostic) for (auto *name : {"button-press-event", "button-release-event", "realize", "map", "unmap"}) {
      auto signal = g_signal_lookup (name, GTK_TYPE_WIDGET);
      auto id = g_signal_add_emission_hook (signal, 0, observe, this, nullptr);
      g_assert_cmpuint (id, >, 0); hooks.push_back ({signal, id});
    }
  }
  ~Input () {
    for (auto hook : hooks) g_signal_remove_emission_hook (hook.signal, hook.id);
    g_module_close (module);
  }
  /* Buffer rather than print in the rapid-input path. Failed probes flush a
   * bounded trace; successful runs do not add timing-sensitive log traffic. */
  void record (const char *format, ...) G_GNUC_PRINTF (2, 3) {
    if (!diagnostic || observations++ >= 512) return;
    va_list args; va_start (args, format); auto *line = g_strdup_vprintf (format, args); va_end (args);
    trace.emplace_back (line); g_free (line);
  }
  static gboolean observe (GSignalInvocationHint *hint, guint count, const GValue *values, gpointer data) {
    auto& input = *static_cast<Input *> (data);
    auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
    auto *grab = gtk_grab_get_current ();
    if (count == 2) {
      auto *event = static_cast<GdkEvent *> (g_value_get_boxed (&values[1]));
      if (!event) return TRUE;
      if (event->type == GDK_BUTTON_PRESS) ++input.presses;
      else if (event->type == GDK_2BUTTON_PRESS) ++input.doubles;
      else if (event->type == GDK_3BUTTON_PRESS) ++input.triples;
      else if (event->type == GDK_BUTTON_RELEASE) ++input.releases;
      auto *target = gtk_get_event_widget (event);
      input.record ("event signal=%s type=%d time=%u button=%u window=%p device=%p root=%.1f,%.1f recipient=%p:%s:%s target=%p:%s grab=%p:%s:%s",
        g_signal_name (hint->signal_id), event->type, event->button.time, event->button.button,
        static_cast<void *> (event->button.window), static_cast<void *> (gdk_event_get_device (event)),
        event->button.x_root, event->button.y_root, static_cast<void *> (widget), G_OBJECT_TYPE_NAME (widget),
        gtk_widget_get_name (widget), static_cast<void *> (target), target ? gtk_widget_get_name (target) : "none",
        static_cast<void *> (grab), grab ? G_OBJECT_TYPE_NAME (grab) : "none", grab ? gtk_widget_get_name (grab) : "none");
    } else if (GTK_IS_POPOVER (widget) || !g_strcmp0 (G_OBJECT_TYPE_NAME (widget), "GimpPopover2")) {
      input.record ("host signal=%s widget=%p:%s:%s realized=%d mapped=%d grab=%p",
        g_signal_name (hint->signal_id), static_cast<void *> (widget), G_OBJECT_TYPE_NAME (widget),
        gtk_widget_get_name (widget), gtk_widget_get_realized (widget), gtk_widget_get_mapped (widget), static_cast<void *> (grab));
    }
    return TRUE;
  }
  void failure (const char *stage) {
    if (!diagnostic) return;
    auto *grab = gtk_grab_get_current ();
    g_test_message ("DIAGNOSTIC failure stage=%s grab=%p:%s:%s realized=%d mapped=%d emitted-press=%u double=%u triple=%u release=%u",
      stage, static_cast<void *> (grab),
      grab ? G_OBJECT_TYPE_NAME (grab) : "none", grab ? gtk_widget_get_name (grab) : "none",
      grab ? gtk_widget_get_realized (grab) : 0, grab ? gtk_widget_get_mapped (grab) : 0,
      presses, doubles, triples, releases);
    for (const auto& line : trace) g_test_message ("DIAGNOSTIC %s", line.c_str ());
    if (observations > 512) g_test_message ("DIAGNOSTIC trace limited to 512 observations");
    auto *root = gdk_get_default_root_window ();
    auto *pixels = gdk_pixbuf_get_from_window (root, 0, 0, gdk_window_get_width (root), gdk_window_get_height (root));
    if (!pixels) { g_test_message ("DIAGNOSTIC root capture unavailable"); return; }
    auto *name = g_strdup_printf ("%s-%s.png", g_test_get_path (), stage);
    for (auto *p = name; *p; ++p) if (!g_ascii_isalnum (*p) && *p != '.' && *p != '-') *p = '_';
    const auto *directory = g_getenv ("GIMP_TEST_COMMON_POPUP_DIAGNOSTICS");
    g_mkdir_with_parents (directory, 0700);
    auto *path = g_build_filename (directory, name, nullptr); GError *error = nullptr;
    if (!gdk_pixbuf_save (pixels, path, "png", &error, nullptr)) {
      g_test_message ("DIAGNOSTIC root capture failed: %s", error ? error->message : "unknown"); g_clear_error (&error);
    } else g_test_message ("DIAGNOSTIC failure root PNG %s (%dx%d)", name,
                          gdk_pixbuf_get_width (pixels), gdk_pixbuf_get_height (pixels));
    g_free (path); g_free (name); g_object_unref (pixels);
  }
  void flush (GdkDisplay *display, bool fast) {
    gdk_display_flush (display);
    if (fast) { gdk_display_sync (display); drain (); } else settle ();
  }
  struct Geometry {
    GtkAllocation widget {}, top {};
    gint x = 0, y = 0, gdk_x = 0, gdk_y = 0, scale = 1;
    bool same (const Geometry& other) const {
      return x == other.x && y == other.y && gdk_x == other.gdk_x && gdk_y == other.gdk_y && scale == other.scale &&
        widget.x == other.widget.x && widget.y == other.widget.y && widget.width == other.widget.width && widget.height == other.widget.height &&
        top.x == other.top.x && top.y == other.top.y && top.width == other.top.width && top.height == other.top.height;
    }
  };
  static Geometry geometry (GtkWidget *widget, gint x, gint y) {
    Geometry result; auto *top = gtk_widget_get_toplevel (widget); auto *window = gtk_widget_get_window (widget);
    gtk_widget_get_allocation (widget, &result.widget); gtk_widget_get_allocation (top, &result.top);
    gint tx, ty, ox, oy;
    g_assert_true (gtk_widget_translate_coordinates (widget, top, x, y, &tx, &ty));
    gdk_window_get_origin (gtk_widget_get_window (top), &ox, &oy);
    result.x = ox + tx; result.y = oy + ty;
    result.scale = gdk_window_get_scale_factor (gtk_widget_get_window (top));
    /* Independently sample the GdkWindow route, including its offscreen
     * embedder. Match GTK's allocation-to-window coordinate convention. */
    if (gtk_widget_get_has_window (widget) && gtk_widget_get_parent (widget)) {
      gint wx, wy; gdk_window_get_position (window, &wx, &wy);
      x -= wx - result.widget.x; y -= wy - result.widget.y;
    } else { x += result.widget.x; y += result.widget.y; }
    gdk_window_get_root_coords (window, x, y, &result.gdk_x, &result.gdk_y);
    return result;
  }
  void record_geometry (const char *phase, const Geometry& g) {
    record ("geometry %s gtk-root=%d,%d gdk-root=%d,%d scale=%d widget=%d,%d,%d,%d top=%d,%d,%d,%d",
      phase, g.x, g.y, g.gdk_x, g.gdk_y, g.scale,
      g.widget.x, g.widget.y, g.widget.width, g.widget.height, g.top.x, g.top.y, g.top.width, g.top.height);
  }
  void ready (GtkWidget *widget, gint x, gint y) {
    struct State { Input *input; GtkWidget *widget; gint x, y; Geometry previous; guint frames = 0, stable = 0; };
    State state {this, widget, x, y, {}};
    auto *clock = gtk_widget_get_frame_clock (widget); g_assert_nonnull (clock); g_object_ref (clock);
    const auto handler = g_signal_connect (clock, "after-paint", G_CALLBACK (+[] (GdkFrameClock *clock, gpointer data) {
      auto& s = *static_cast<State *> (data); auto current = geometry (s.widget, s.x, s.y);
      const bool unchanged = s.frames && current.same (s.previous);
      s.stable = unchanged ? s.stable + 1 : 0;
      if (!unchanged) s.input->record_geometry ("after-paint", current);
      s.previous = current; ++s.frames;
      if (s.stable < 3) { gtk_widget_queue_draw (s.widget); gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT); }
    }), &state);
    gdk_frame_clock_begin_updating (clock); gtk_widget_queue_draw (widget);
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
    const auto until = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
    while (state.stable < 3 && g_get_monotonic_time () < until) {
      g_main_context_iteration (nullptr, FALSE); g_usleep (1000);
    }
    gdk_frame_clock_end_updating (clock); g_signal_handler_disconnect (clock, handler); g_object_unref (clock);
    record ("ready frames=%u stable=%u", state.frames, state.stable);
    if (state.stable < 3) failure ("layout-not-stable");
    g_assert_cmpuint (state.stable, >=, 3);
  }
  void aim (GtkWidget *widget, gint x, gint y, guint number, bool fast) {
#ifdef GDK_WINDOWING_X11
    auto *top = gtk_widget_get_toplevel (widget); auto *display = gtk_widget_get_display (widget);
    auto *window = gtk_widget_get_window (widget);
    const bool mapped = gtk_widget_get_mapped (widget), realized = gtk_widget_get_realized (widget);
    const bool sensitive = gtk_widget_is_sensitive (widget), viewable = window && gdk_window_is_viewable (window);
    if (!mapped || !realized || !sensitive || !viewable) {
      GtkAllocation allocation; gtk_widget_get_allocation (widget, &allocation);
      record ("input precondition widget=%p:%s:%s mapped=%d realized=%d sensitive=%d window=%p visible=%d viewable=%d allocation=%d,%d,%d,%d button=%u fast=%d",
        static_cast<void *> (widget), G_OBJECT_TYPE_NAME (widget), gtk_widget_get_name (widget),
        mapped, realized, sensitive, static_cast<void *> (window), window && gdk_window_is_visible (window), viewable,
        allocation.x, allocation.y, allocation.width, allocation.height, number, fast);
      failure ("input-precondition");
    }
    g_assert_true (mapped); g_assert_true (realized); g_assert_true (sensitive); g_assert_true (viewable);
    gint local_x, local_y, root_x, root_y;
    g_assert_true (gtk_widget_translate_coordinates (widget, top, x, y, &local_x, &local_y));
    gdk_window_get_origin (gtk_widget_get_window (top), &root_x, &root_y);
    const auto scale = gdk_window_get_scale_factor (gtk_widget_get_window (top));
    auto *native = gdk_x11_display_get_xdisplay (display);
    record_geometry ("before-motion", geometry (widget, x, y));
    if (diagnostic) {
      auto *root = gdk_get_default_root_window (); GtkAllocation allocation, top_allocation;
      gtk_widget_get_allocation (widget, &allocation); gtk_widget_get_allocation (top, &top_allocation);
      GdkRectangle monitor;
      gdk_monitor_get_geometry (gdk_display_get_monitor_at_window (display, gtk_widget_get_window (top)), &monitor);
      record ("click widget=%p:%s:%s allocation=%d,%d,%d,%d mapped=%d viewable=%d top=%p allocation=%d,%d,%d,%d origin=%d,%d local=%d,%d scale=%d requested-physical=%d,%d root-logical=%dx%d monitor=%d,%d,%d,%d in-screen=%d button=%u fast=%d",
        static_cast<void *> (widget), G_OBJECT_TYPE_NAME (widget), gtk_widget_get_name (widget),
        allocation.x, allocation.y, allocation.width, allocation.height, gtk_widget_get_mapped (widget),
        gdk_window_is_viewable (gtk_widget_get_window (widget)), static_cast<void *> (top),
        top_allocation.x, top_allocation.y, top_allocation.width, top_allocation.height,
        root_x, root_y, local_x, local_y, scale, (root_x + local_x) * scale, (root_y + local_y) * scale,
        gdk_window_get_width (root), gdk_window_get_height (root), monitor.x, monitor.y, monitor.width, monitor.height,
        root_x + local_x >= 0 && root_y + local_y >= 0 && root_x + local_x < gdk_window_get_width (root) &&
        root_y + local_y < gdk_window_get_height (root), number, fast);
    }
    auto moved = motion (native, -1, (root_x + local_x) * scale, (root_y + local_y) * scale, 0);
    flush (display, fast);
    auto current = geometry (widget, x, y); record_geometry ("after-motion", current);
    if (current.x != root_x + local_x || current.y != root_y + local_y || current.scale != scale) {
      /* Motion may let a queued native overlay allocation finish. Re-aim
       * once using observed geometry, never a guessed offset or direct event. */
      moved = motion (native, -1, current.x * current.scale, current.y * current.scale, 0);
      flush (display, fast);
      auto next = geometry (widget, x, y); record_geometry ("after-remotion", next);
      if (!next.same (current)) failure ("motion-layout-changed");
      g_assert_true (next.same (current));
    }
    auto *pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
    gint px = 0, py = 0, wx = 0, wy = 0; gdk_device_get_position (pointer, nullptr, &px, &py);
    auto *hit = gdk_device_get_window_at_position (pointer, &wx, &wy); gpointer data = nullptr;
    if (hit) gdk_window_get_user_data (hit, &data);
    auto *hit_widget = data && GTK_IS_WIDGET (data) ? GTK_WIDGET (data) : nullptr;
    record ("motion return=%d actual-root-logical=%d,%d hit-window=%p local=%d,%d hit-widget=%p:%s:%s",
      moved, px, py, static_cast<void *> (hit), wx, wy, static_cast<void *> (hit_widget),
      hit_widget ? G_OBJECT_TYPE_NAME (hit_widget) : "none", hit_widget ? gtk_widget_get_name (hit_widget) : "none");
    auto *root = gdk_get_default_root_window ();
    const bool in_screen = current.x >= 0 && current.y >= 0 &&
      current.x < gdk_window_get_width (root) && current.y < gdk_window_get_height (root);
    const bool intended = hit_widget && (hit_widget == widget || gtk_widget_is_ancestor (hit_widget, widget));
    if (!moved || !in_screen || !intended || px != current.x || py != current.y ||
        current.x != current.gdk_x || current.y != current.gdk_y) failure ("wrong-physical-target");
    g_assert_cmpint (moved, !=, 0); g_assert_true (in_screen); g_assert_true (intended);
    g_assert_cmpint (px, ==, current.x); g_assert_cmpint (py, ==, current.y);
    g_assert_cmpint (current.x, ==, current.gdk_x); g_assert_cmpint (current.y, ==, current.gdk_y);
#endif
  }
  void hover (GtkWidget *widget) {
    aim (widget, gtk_widget_get_allocated_width (widget) / 2,
         gtk_widget_get_allocated_height (widget) / 2, 0, false);
  }
  void click (GtkWidget *widget, gint x, gint y, guint number = 1, bool fast = false) {
    aim (widget, x, y, number, fast);
#ifdef GDK_WINDOWING_X11
    auto *display = gtk_widget_get_display (widget);
    auto *native = gdk_x11_display_get_xdisplay (display);
    auto pressed = button (native, number, TRUE, 0); flush (display, fast);
    auto released = button (native, number, FALSE, 0); flush (display, fast);
    record ("button returns press=%d release=%d", pressed, released);
    g_assert_cmpint (pressed, !=, 0); g_assert_cmpint (released, !=, 0);
#endif
  }
  void center (GtkWidget *widget) { click (widget, gtk_widget_get_allocated_width (widget) / 2,
                                        gtk_widget_get_allocated_height (widget) / 2); }
  void key (GtkWidget *widget, guint value, bool fast = false) {
#ifdef GDK_WINDOWING_X11
    auto *display = gtk_widget_get_display (widget); GdkKeymapKey *keys = nullptr; gint count = 0;
    g_assert_true (gdk_keymap_get_entries_for_keyval (gdk_keymap_get_for_display (display), value, &keys, &count));
    g_assert_cmpint (count, >, 0); auto *native = gdk_x11_display_get_xdisplay (display);
    keycode (native, keys[0].keycode, TRUE, 0); keycode (native, keys[0].keycode, FALSE, 0);
    g_free (keys); flush (display, fast);
#endif
  }
};
struct MappedWindows {
  std::vector<GtkWidget *> before;
  guint map_signal = 0; gulong map_hook = 0; bool newly_mapped = false;
  static gboolean mapped (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
    auto& self = *static_cast<MappedWindows *> (data);
    auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
    if (GTK_IS_WINDOW (widget) && !tooltip (widget) &&
        std::find (self.before.begin (), self.before.end (), widget) == self.before.end ()) self.newly_mapped = true;
    return TRUE;
  }
  static bool tooltip (GtkWidget *widget) {
    /* Exclude only GTK's own tooltip carrier with its tooltip window hint.
     * Application dialogs, menus and other GTK_WINDOW_POPUP hosts still count. */
    return !g_strcmp0 (G_OBJECT_TYPE_NAME (widget), "GtkTooltipWindow") &&
      gtk_window_get_type_hint (GTK_WINDOW (widget)) == GDK_WINDOW_TYPE_HINT_TOOLTIP;
  }
  MappedWindows () {
    auto *windows = gtk_window_list_toplevels ();
    for (auto *i = windows; i; i = i->next) {
      auto *widget = GTK_WIDGET (i->data);
      if (gtk_widget_get_mapped (widget) && !tooltip (widget)) before.push_back (GTK_WIDGET (g_object_ref (widget)));
    }
    g_list_free (windows);
    map_signal = g_signal_lookup ("map", GTK_TYPE_WIDGET);
    map_hook = g_signal_add_emission_hook (map_signal, 0, mapped, this, nullptr); g_assert_cmpuint (map_hook, >, 0);
  }
  ~MappedWindows () {
    g_signal_remove_emission_hook (map_signal, map_hook);
    for (auto *widget : before) g_object_unref (widget);
  }
  bool has_new (Input& input) {
    bool found = newly_mapped; auto *windows = gtk_window_list_toplevels ();
    for (auto *i = windows; i; i = i->next) {
      auto *widget = GTK_WIDGET (i->data);
      if (gtk_widget_get_mapped (widget) && !tooltip (widget) &&
          std::find (before.begin (), before.end (), widget) == before.end ()) {
        input.record ("new mapped toplevel=%p:%s:%s hint=%d", static_cast<void *> (widget),
          G_OBJECT_TYPE_NAME (widget), gtk_widget_get_name (widget), gtk_window_get_type_hint (GTK_WINDOW (widget)));
        found = true;
      }
    }
    g_list_free (windows); return found;
  }
};
struct Route { bool tree, clone; const char *name; };
const Route routes[] = {{false,false,"ordinary-tile"}, {false,true,"clone-tile"},
                        {true,false,"ordinary-tree"}, {true,true,"clone-tree"}};
using NativeDockProbe = void (*) (GtkWidget *, GtkWidget *, gpointer);
using NativeCloseProbe = void (*) (GtkWidget *, gpointer);
struct Fixture {
  const Route& route; GimpDisplay *display; GimpDisplayShell *shell; GimpImage *image;
  GimpLayer *target, *source; GtkWidget *owner, *anchor, *popup = nullptr, *dockable = nullptr;
  GimpCellRendererPopup *cell = nullptr; GdkRectangle area; gint hit_x = 28, hit_y = 32;
  gint opening_left = 0, opening_top = 0, opening_width = 0, opening_height = 0;
  bool captured = false, warmed = false;
  NativeDockProbe native_probe = nullptr;
  NativeCloseProbe native_close = nullptr;
  gpointer native_data = nullptr;
  Fixture (const Route& r, bool group = false, NativeDockProbe probe = nullptr,
           NativeCloseProbe closing = nullptr, gpointer data = nullptr)
    : route (r), native_probe (probe), native_close (closing), native_data (data) {
    dock_phase ("fixture/begin");
    gimp_set_focused_once (app); g_assert_true (app->config->painter_canvas_ui);
    gboolean single_window = FALSE;
    g_object_get (app->config, "single-window-mode", &single_window, nullptr);
    g_assert_true (single_window);
    if (!keeper) {
      dock_phase ("fixture/keeper-create");
      gimp_test_utils_create_image (app, 32, 32); settle ();
      keeper = gimp_context_get_display (app->user_context); g_assert_nonnull (keeper);
      keeper_window = gtk_widget_get_toplevel (GTK_WIDGET (gimp_display_get_shell (keeper)));
      g_object_add_weak_pointer (G_OBJECT (keeper), reinterpret_cast<gpointer *> (&keeper));
      g_object_add_weak_pointer (G_OBJECT (keeper_window), reinterpret_cast<gpointer *> (&keeper_window));
      /* create_image leaves its initial image reference with the caller. The
       * native display now owns the image for the suite's remaining lifetime. */
      g_object_unref (gimp_display_get_image (keeper));
    }
    dock_phase ("fixture/test-image-create");
    gimp_test_utils_create_image (app, 320, 240); settle ();
    display = gimp_context_get_display (app->user_context); g_assert_nonnull (display);
    g_assert_true (display != keeper);
    shell = gimp_display_get_shell (display); image = gimp_display_get_image (display);
    dock_phase ("fixture/present-shell");
    gimp_display_shell_present (shell); settle ();
    g_assert_true (gimp_context_get_display (app->user_context) == display);
    g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (shell)) == keeper_window);
    dock_phase ("fixture/layer-model");
    source = GIMP_LAYER (gimp_image_get_selected_layers (image)->data);
    auto *color = gegl_color_new ("rgba(0.25,0.5,0.75,1.0)");
    gegl_buffer_set_color (gimp_drawable_get_buffer (GIMP_DRAWABLE (source)), nullptr, color);
    g_object_unref (color); gimp_drawable_update (GIMP_DRAWABLE (source), 0, 0, -1, -1);
    target = group ? gimp_group_layer_new (image) : route.clone ?
      gimp_clone_layer_new (image, source, 320, 240, "Common popup Clone", 1, GIMP_LAYER_MODE_NORMAL_LEGACY) : source;
    if (target != source) g_assert_true (gimp_image_add_layer (image, target, nullptr, 0, FALSE));
    g_object_ref (target); g_object_ref (source);
    auto *selection = g_list_prepend (nullptr, target); gimp_image_set_selected_layers (image, selection); g_list_free (selection);
    gimp_image_flush (image); g_assert_true (gimp_painter_canvas_ui_get_visible (shell)); settle ();
    auto *top = gtk_widget_get_toplevel (GTK_WIDGET (shell));
    auto *monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (top), gtk_widget_get_window (top));
    GdkRectangle screen; gdk_monitor_get_geometry (monitor, &screen);
    dock_phase ("fixture/resize-window");
    gtk_window_resize (GTK_WINDOW (top), screen.width - 40, screen.height - 80);
    gtk_window_move (GTK_WINDOW (top), screen.x, screen.y); settle ();
    if (route.tree) {
      dock_phase ("fixture/native-dock-open");
      dockable = gimp_window_strategy_show_dockable_dialog (
        GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (app)), app,
        gimp_dialog_factory_get_singleton (), monitor, "gimp-layer-list");
      g_assert_nonnull (dockable); g_object_ref (dockable);
      if (native_probe) native_probe (dockable, GTK_WIDGET (shell), native_data);
      controlled_dock_updates (dockable);
      dock_phase ("fixture/native-dock-settle"); settle ();
      g_assert_true (gtk_widget_get_toplevel (dockable) == top);
      owner = find (dockable, nullptr, GIMP_TYPE_LAYER_TREE_VIEW); g_assert_nonnull (owner);
      auto *view = GIMP_CONTAINER_TREE_VIEW (owner); anchor = GTK_WIDGET (view->view);
      auto *column = gtk_tree_view_get_column (view->view, 0);
      auto *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
      for (auto *i = cells; i; i = i->next) if (GIMP_IS_CELL_RENDERER_POPUP (i->data)) cell = GIMP_CELL_RENDERER_POPUP (i->data);
      g_list_free (cells); g_assert_nonnull (cell); g_object_ref (cell);
    } else {
      owner = find (shell->canvas, nullptr, GIMP_TYPE_PAINTER_LAYER_TILES); g_assert_nonnull (owner);
      anchor = field (owner, "painter-layer-canvas");
    }
    g_object_ref (owner); g_object_ref (anchor);
    dock_phase ("fixture/owner-settle"); settle ();
    g_assert_true (gtk_widget_get_toplevel (owner) == top);
    g_assert_true (gtk_widget_get_toplevel (anchor) == top);
    g_assert_true (gimp_context_get_display (app->user_context) == display);
  }
  const char *popup_name () const { return route.tree ? "painter-tree-layer-popup" : "painter-layer-properties-popup"; }
  const char *opacity_name () const { return route.tree ? "painter-tree-layer-opacity" : "painter-layer-opacity"; }
  const char *mode_name () const { return route.tree ? "painter-tree-layer-mode" : "painter-layer-mode"; }
  GtkTreePath *path () {
    auto *view = GIMP_CONTAINER_TREE_VIEW (owner); GtkTreeIter iter;
    for (gboolean valid = gtk_tree_model_get_iter_first (view->model, &iter); valid;
         valid = gtk_tree_model_iter_next (view->model, &iter)) {
      auto *renderer = gimp_container_tree_store_get_renderer (GIMP_CONTAINER_TREE_STORE (view->model), &iter);
      bool match = renderer->viewable == GIMP_VIEWABLE (target); g_object_unref (renderer);
      if (match) return gtk_tree_model_get_path (view->model, &iter);
    }
    g_assert_not_reached (); return nullptr;
  }
  void prepare_area () {
    if (route.tree) {
      auto *view = GIMP_CONTAINER_TREE_VIEW (owner); auto *p = path ();
      gtk_tree_view_scroll_to_cell (view->view, p, nullptr, FALSE, 0, 0); settle ();
      auto *column = gtk_tree_view_get_column (view->view, 0);
      gtk_tree_view_get_cell_area (view->view, p, column, &area);
      GtkTreePath *hit = nullptr; GtkTreeViewColumn *hit_column = nullptr;
      g_assert_true (gtk_tree_view_get_path_at_pos (view->view, area.x + area.width / 2,
        area.y + area.height / 2, &hit, &hit_column, nullptr, nullptr));
      g_assert_true (hit_column == column); g_assert_cmpint (gtk_tree_path_compare (hit, p), ==, 0); gtk_tree_path_free (hit);
      gtk_tree_view_convert_bin_window_to_widget_coords (view->view,
        area.x + area.width / 2, area.y + area.height / 2, &hit_x, &hit_y);
      gtk_tree_view_convert_bin_window_to_widget_coords (view->view, area.x, area.y, &area.x, &area.y);
      area.x = 0; area.width = gtk_widget_get_allocated_width (anchor); gtk_tree_path_free (p);
    } else area = {0, 0, 56, 64};
  }
  static gboolean realize (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
    auto& f = *static_cast<Fixture *> (data); auto *widget = GTK_WIDGET (g_value_get_object (&values[0]));
    if (g_strcmp0 (gtk_widget_get_name (widget), f.popup_name ())) return TRUE;
    /* GtkPopover can realize inside its factory, before production assigns
     * its name. The map fallback captures that baseline carrier without
     * replacing native GimpPopover2's earlier realize-time geometry. */
    if (f.popup == widget) return TRUE;
    g_assert_null (f.popup); f.popup = GTK_WIDGET (g_object_ref (widget));
    GtkAllocation allocation; gtk_widget_get_allocation (f.anchor, &allocation);
    gdk_window_get_origin (gtk_widget_get_window (f.anchor), &f.opening_left, &f.opening_top);
    if (!gtk_widget_get_has_window (f.anchor)) { f.opening_left += allocation.x; f.opening_top += allocation.y; }
    f.opening_left += f.area.x; f.opening_top += f.area.y;
    GtkRequisition natural; gtk_widget_get_preferred_size (widget, nullptr, &natural);
    f.opening_width = natural.width; f.opening_height = natural.height; f.captured = true;
    return TRUE;
  }
  void open (Input& input, bool require_popup = true, bool fast = false) {
    dock_phase ("open/prepare");
    close (); dock_phase ("open/prepare-area"); prepare_area (); captured = false;
    if (!warmed) {
      /* Finish initial shell/overlay reflow before the first real press.
       * Repeated fast opens never add frame waits or change GTK's interval. */
      input.ready (anchor, hit_x, hit_y); prepare_area (); warmed = true;
    }
    auto signal = g_signal_lookup ("realize", GTK_TYPE_WIDGET);
    auto hook = g_signal_add_emission_hook (signal, 0, realize, this, nullptr); g_assert_cmpuint (hook, >, 0);
    auto map_signal = g_signal_lookup ("map", GTK_TYPE_WIDGET);
    auto map_hook = g_signal_add_emission_hook (map_signal, 0, realize, this, nullptr); g_assert_cmpuint (map_hook, >, 0);
    dock_phase ("open/physical-input");
    input.click (anchor, hit_x, hit_y, route.tree ? 1 : 3, fast);
    if (fast && require_popup) {
      const auto until = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
      while ((!popup || !gtk_widget_get_mapped (popup)) && g_get_monotonic_time () < until) {
        g_main_context_iteration (nullptr, FALSE); g_usleep (100);
      }
    }
    g_signal_remove_emission_hook (signal, hook); g_signal_remove_emission_hook (map_signal, map_hook); drain ();
    if (require_popup) {
      if (!popup || !GTK_IS_WINDOW (popup) || !gtk_widget_get_mapped (popup) || gtk_grab_get_current () != popup)
        input.failure ("open");
      g_assert_nonnull (popup); g_assert_true (captured); g_assert_true (GTK_IS_WINDOW (popup));
      g_assert_cmpstr (G_OBJECT_TYPE_NAME (popup), ==, "GimpPopover2");
      g_assert_cmpint (gtk_window_get_window_type (GTK_WINDOW (popup)), ==, GTK_WINDOW_POPUP);
      g_assert_true (gtk_widget_get_mapped (popup)); g_assert_true (gtk_grab_get_current () == popup);
      g_assert_nonnull (field (popup, opacity_name ()));
    }
  }
  void close () { dock_phase ("close/host"); if (popup) { gtk_widget_destroy (popup); g_object_unref (popup); popup = nullptr; } drain (); }
  guint depth () { return gimp_undo_stack_get_depth (gimp_image_get_undo_stack (image)); }
  ~Fixture () {
    dock_phase ("fixture/teardown");
    close ();
    /* The dialog factory is a singleton. Never leave a hidden or emptied
     * dockable for the next destructive-lifetime fixture to inherit. */
    dock_phase ("fixture/destroy-dockable");
    if (dockable) { gtk_widget_destroy (dockable); g_object_unref (dockable); }
    if (cell) g_object_unref (cell);
    g_object_unref (anchor); g_object_unref (owner);
    dock_phase ("fixture/close-display");
    if (native_close) native_close (GTK_WIDGET (shell), native_data);
    gimp_display_close (display); g_object_unref (target); g_object_unref (source); g_object_unref (image);
    dock_phase ("fixture/final-drain"); drain ();
    g_assert_nonnull (keeper); g_assert_nonnull (keeper_window);
    g_assert_nonnull (gimp_display_get_image (keeper));
    g_assert_true (gtk_widget_get_realized (keeper_window));
    g_assert_true (gtk_widget_get_toplevel (GTK_WIDGET (gimp_display_get_shell (keeper))) == keeper_window);
  }
};
/* Independent oracle for the actual old BOTTOM_LEFT area path. Its missing
 * brace makes above-first unconditional. Clamp only the final overlarge/edge
 * result, never center it or relax an observed coordinate mismatch. */
void assert_geometry (Fixture& f) {
  auto *monitor = gdk_display_get_monitor_at_point (gtk_widget_get_display (f.anchor), f.opening_left, f.opening_top);
  GdkRectangle rect; gdk_monitor_get_geometry (monitor, &rect);
  gint left = f.opening_left, top = f.opening_top;
  gint right = left + f.area.width, bottom = top + f.area.height;
  gint x = left, y = bottom, width = f.opening_width, height = f.opening_height;
  if (x + width > rect.x + rect.width) x = right - width;
  if (y + height > rect.y + rect.height) y = top - height;
  if (x < rect.x) x = rect.x;
  if (top - height >= rect.y) y = top - height;
  else if (bottom + height < rect.y + rect.height) y = bottom;
  if (y < rect.y) {
    y = rect.y;
    if (right + width < rect.x + rect.width) x = right;
    else if (left - width >= rect.x) x = left - width;
  }
  if (width <= rect.width) x = CLAMP (x, rect.x, rect.x + rect.width - width);
  if (height <= rect.height) y = CLAMP (y, rect.y, rect.y + rect.height - height);
  gint actual_x, actual_y; gtk_window_get_position (GTK_WINDOW (f.popup), &actual_x, &actual_y);
  g_assert_cmpint (actual_x, ==, x); g_assert_cmpint (actual_y, ==, y);
}
void assert_visible (GtkWidget *popup) {
  gint x, y, width, height; gtk_window_get_position (GTK_WINDOW (popup), &x, &y);
  gtk_window_get_size (GTK_WINDOW (popup), &width, &height);
  auto *monitor = gdk_display_get_monitor_at_window (gtk_widget_get_display (popup), gtk_widget_get_window (popup));
  GdkRectangle rect; gdk_monitor_get_geometry (monitor, &rect);
  g_assert_cmpint (x, >=, rect.x); g_assert_cmpint (y, >=, rect.y);
  g_assert_cmpint (x + width, <=, rect.x + rect.width); g_assert_cmpint (y + height, <=, rect.y + rect.height);
}
/* Native portrait evidence includes the suite keeper tab as well as the active
 * test document; it is not an untouched single-document session. */
void screenshot (Fixture& f, const char *variant = "") {
  const auto *directory = g_getenv ("GIMP_TEST_COMMON_POPUP_SCREENSHOTS"); if (!directory) return;
  settle (); auto *root = gdk_get_default_root_window (); auto scale = gdk_window_get_scale_factor (root);
  gint width = gdk_window_get_width (root), height = gdk_window_get_height (root);
  g_assert_cmpint (width * scale, ==, 1080); g_assert_cmpint (height * scale, ==, 1920);
  auto *pixels = gdk_pixbuf_get_from_window (root, 0, 0, width, height); g_assert_nonnull (pixels);
  g_assert_cmpint (gdk_pixbuf_get_width (pixels), ==, 1080); g_assert_cmpint (gdk_pixbuf_get_height (pixels), ==, 1920);
  auto *name = g_strdup_printf ("%s/%s%s-scale%d.png", directory, f.route.name, variant, scale);
  GError *error = nullptr; g_assert_true (gdk_pixbuf_save (pixels, name, "png", &error, nullptr));
  g_assert_no_error (error); g_free (name); g_object_unref (pixels);
}
/* Enter the real scrollbar before waiting for its hover-only CSS transition.
 * This is confined to lower-control scrolling: rapid/open/outside input keeps
 * the original strict aim and timing. No button is sent until stable. */
void prepare_scrollbar_hover (Input& input, GtkWidget *bar) {
#ifdef GDK_WINDOWING_X11
  g_assert_true (GTK_IS_SCROLLBAR (bar));
  g_assert_true (gtk_widget_get_mapped (bar));
  g_assert_true (gtk_widget_get_realized (bar));
  g_assert_true (gtk_widget_is_sensitive (bar));
  auto *window = gtk_widget_get_window (bar); g_assert_nonnull (window);
  g_assert_true (gdk_window_is_viewable (window));
  auto *display = gtk_widget_get_display (bar);
  auto *pointer = gdk_seat_get_pointer (gdk_display_get_default_seat (display));
  auto *context = gtk_widget_get_style_context (bar);
  const bool overlay_indicator = gtk_style_context_has_class (context, "overlay-indicator");
  const auto until = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  bool hovering = false, intended = false;
  const auto require_live_bar = [&] {
    auto *current_window = gtk_widget_get_window (bar);
    const bool live = gtk_widget_get_mapped (bar) && gtk_widget_get_realized (bar) &&
      current_window && gdk_window_is_viewable (current_window);
    if (!live) input.failure ("scrollbar-hover-retired");
    g_assert_true (live);
  };
  do {
    require_live_bar ();
    const auto point = Input::geometry (bar, gtk_widget_get_allocated_width (bar) / 2,
                                       gtk_widget_get_allocated_height (bar) / 2);
    input.record_geometry ("scrollbar-hover-start", point);
    auto *root = gdk_get_default_root_window ();
    const bool in_screen = point.x >= 0 && point.y >= 0 &&
      point.x < gdk_window_get_width (root) && point.y < gdk_window_get_height (root);
    if (!in_screen || point.x != point.gdk_x || point.y != point.gdk_y)
      input.failure ("scrollbar-hover-coordinates");
    g_assert_true (in_screen);
    g_assert_cmpint (point.x, ==, point.gdk_x); g_assert_cmpint (point.y, ==, point.gdk_y);
    const auto moved = input.motion (gdk_x11_display_get_xdisplay (display), -1,
                                    point.x * point.scale, point.y * point.scale, 0);
    if (!moved) input.failure ("scrollbar-hover-motion");
    g_assert_cmpint (moved, !=, 0);
    input.flush (display, false);
    require_live_bar ();
    gint px = 0, py = 0, wx = 0, wy = 0; gdk_device_get_position (pointer, nullptr, &px, &py);
    auto *hit = gdk_device_get_window_at_position (pointer, &wx, &wy); gpointer data = nullptr;
    if (hit) gdk_window_get_user_data (hit, &data);
    auto *hit_widget = data && GTK_IS_WIDGET (data) ? GTK_WIDGET (data) : nullptr;
    intended = hit_widget && (hit_widget == bar || gtk_widget_is_ancestor (hit_widget, bar));
    const bool hover_class = gtk_style_context_has_class (context, "hovering");
    hovering = !overlay_indicator || hover_class;
    input.record ("scrollbar motion return=%d actual-root=%d,%d hit=%p:%s overlay-indicator=%d hovering-class=%d", moved, px, py,
      static_cast<void *> (hit_widget), hit_widget ? G_OBJECT_TYPE_NAME (hit_widget) : "none", overlay_indicator, hover_class);
    if (px != point.x || py != point.y) input.failure ("scrollbar-hover-pointer-position");
    g_assert_cmpint (px, ==, point.x); g_assert_cmpint (py, ==, point.y);
    g_assert_true (gtk_widget_get_mapped (bar));
    /* GTK may first reveal an indicator beneath the pointer, then enter its
     * scrollbar on a subsequent real motion. No button is sent until both
     * the actual receiver and (for overlays) GTK's hovering class are observed. */
  } while ((!intended || !hovering) && g_get_monotonic_time () < until);
  if (!intended || !hovering) input.failure ("scrollbar-hover-not-entered");
  g_assert_true (intended); g_assert_true (hovering);
  /* Stable frames now follow the semantic start of hover expansion, rather
   * than accidentally observing the still-collapsed indicator beforehand. */
  input.ready (bar, gtk_widget_get_allocated_width (bar) / 2,
               gtk_widget_get_allocated_height (bar) / 2);
  require_live_bar ();
  if (overlay_indicator && !gtk_style_context_has_class (context, "hovering"))
    input.failure ("scrollbar-hover-state-lost");
  g_assert_true (!overlay_indicator || gtk_style_context_has_class (context, "hovering"));
  input.hover (bar); // Original strict actual-hit/root checks, still no button.
#endif
}

/* A genuine wheel acts on the scrollbar rather than a layer-valued range.
 * Read adjustments only to observe GTK scrolling; never set one directly. */
void scroll_common_to_bottom (Fixture& f, Input& input) {
  auto *scroller = find (f.popup, nullptr, GTK_TYPE_SCROLLED_WINDOW);
  g_assert_nonnull (scroller);
  auto *adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroller));
  auto *bar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (scroller));
  g_assert_nonnull (bar); g_assert_true (gtk_widget_get_mapped (bar));
  const auto initial = gtk_adjustment_get_value (adjustment);
  const auto bottom = [&] {
    return std::max (gtk_adjustment_get_lower (adjustment),
      gtk_adjustment_get_upper (adjustment) - gtk_adjustment_get_page_size (adjustment));
  };
  /* The actual tile fixture overflows; an always-visible lower row is not
   * accepted as evidence that physical scrolling worked. */
  g_assert_cmpfloat (initial, <, bottom ());
  /* GTK overlay scrollbars can be mapped widgets whose indicator GdkWindow
   * is intentionally hidden. Real motion over visible scroller content asks
   * GTK to reveal it; never disable overlays or show a window directly. */
  const auto overlay = gtk_scrolled_window_get_overlay_scrolling (GTK_SCROLLED_WINDOW (scroller));
  g_object_ref (bar);
  input.hover (field (f.popup, "layers-new"));
  const auto reveal_until = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
  const auto viewable = [&] {
    auto *window = gtk_widget_get_window (bar);
    return window && gdk_window_is_viewable (window);
  };
  while (!viewable () && gtk_widget_get_mapped (f.popup) && g_get_monotonic_time () < reveal_until) {
    g_main_context_iteration (nullptr, FALSE); g_usleep (1000);
  }
  input.record ("lower scrollbar after physical hover mapped=%d realized=%d viewable=%d overlay=%d",
    gtk_widget_get_mapped (bar), gtk_widget_get_realized (bar), viewable (),
    overlay);
  if (!viewable () || !gtk_widget_get_mapped (f.popup)) input.failure ("lower-scrollbar-not-revealed");
  g_assert_true (viewable ()); g_assert_true (gtk_widget_get_mapped (f.popup));
  prepare_scrollbar_hover (input, bar);
  guint wheels = 0;
  const auto until = g_get_monotonic_time () + 8 * G_TIME_SPAN_SECOND;
  while (gtk_adjustment_get_value (adjustment) < bottom () && wheels < 32 &&
         g_get_monotonic_time () < until) {
    const auto before = gtk_adjustment_get_value (adjustment);
    input.click (bar, gtk_widget_get_allocated_width (bar) / 2,
                 gtk_widget_get_allocated_height (bar) / 2, 5);
    ++wheels;
    input.record ("lower wheel=%u adjustment=%.3f->%.3f bottom=%.3f", wheels, before,
      gtk_adjustment_get_value (adjustment), bottom ());
    g_assert_true (gtk_widget_get_mapped (f.popup));
  }
  if (!wheels || gtk_adjustment_get_value (adjustment) < bottom ()) input.failure ("lower-scroll-bottom");
  g_assert_cmpuint (wheels, >, 0);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), >, initial);
  g_assert_cmpfloat (gtk_adjustment_get_value (adjustment), >=, bottom ());
  auto *viewport = gtk_bin_get_child (GTK_BIN (scroller));
  g_assert_true (GTK_IS_VIEWPORT (viewport));
  /* The GDK view window is the actual clip, excluding viewport borders. */
  auto *view_window = gtk_viewport_get_view_window (GTK_VIEWPORT (viewport));
  g_assert_nonnull (view_window); g_assert_true (gdk_window_is_viewable (view_window));
  gint clip_x, clip_y; gdk_window_get_root_coords (view_window, 0, 0, &clip_x, &clip_y);
  for (auto *name : {"layers-delete", "layer-menu"}) {
    auto *control = field (f.popup, name); auto geometry = Input::geometry (control, 0, 0);
    g_assert_cmpint (geometry.x, ==, geometry.gdk_x); g_assert_cmpint (geometry.y, ==, geometry.gdk_y);
    const bool complete = geometry.x >= clip_x && geometry.y >= clip_y &&
      geometry.x + geometry.widget.width <= clip_x + gdk_window_get_width (view_window) &&
      geometry.y + geometry.widget.height <= clip_y + gdk_window_get_height (view_window);
    input.record ("lower visible %s control=%d,%d,%d,%d clip=%d,%d,%d,%d complete=%d", name,
      geometry.x, geometry.y, geometry.widget.width, geometry.widget.height, clip_x, clip_y,
      gdk_window_get_width (view_window), gdk_window_get_height (view_window), complete);
    if (!complete) input.failure ("lower-control-clipped");
    g_assert_true (complete);
  }
  g_object_unref (bar);
}
void lower_controls (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data)); f.open (input);
  assert_visible (f.popup);
  const auto depth = f.depth ();
  const auto opacity = gimp_layer_get_opacity (f.target);
  const auto mode = gimp_layer_get_mode (f.target);
  const auto alpha = gimp_layer_get_lock_alpha (f.target);
  auto *clone_source = f.route.clone ? gimp_clone_layer_get_source (GIMP_CLONE_LAYER (f.target)) : nullptr;
  if (!f.route.tree) {
    scroll_common_to_bottom (f, input);
    screenshot (f, "-bottom");
    input.center (field (f.popup, "layer-menu"));
    auto *menu = gtk_grab_get_current ();
    if (gtk_widget_get_realized (f.popup) || !menu || !GTK_IS_MENU (menu)) input.failure ("lower-all-actions");
    g_assert_false (gtk_widget_get_realized (f.popup));
    g_assert_nonnull (menu); g_assert_true (GTK_IS_MENU (menu));
    g_object_ref (menu); g_assert_true (gtk_widget_get_mapped (menu));
    input.key (menu, GDK_KEY_Escape);
    g_assert_false (gtk_widget_get_mapped (menu)); g_object_unref (menu);
  } else {
    /* Tree has a short, unscrolled common form: alpha is last for ordinary,
     * Clone Source is last for Clone. Do not invent tile actions in that UI. */
    g_assert_null (find (f.popup, nullptr, GTK_TYPE_SCROLLED_WINDOW));
    if (f.route.clone) {
      screenshot (f, "-bottom");
      input.center (field (f.popup, "painter-tree-edit-clone"));
      g_assert_false (gtk_widget_get_realized (f.popup));
      GtkWidget *dialog = nullptr; auto *windows = gtk_window_list_toplevels ();
      for (auto *i = windows; i; i = i->next) {
        auto *window = GTK_WIDGET (i->data);
        if (gtk_widget_get_mapped (window) &&
            !g_strcmp0 (gtk_window_get_role (GTK_WINDOW (window)), "gimp-clone-layer-source")) {
          g_assert_null (dialog); dialog = GTK_WIDGET (g_object_ref (window));
        }
      }
      g_list_free (windows);
      if (!dialog) input.failure ("lower-clone-source");
      g_assert_nonnull (dialog); g_assert_true (GTK_IS_DIALOG (dialog));
      auto *cancel = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      g_assert_nonnull (cancel); input.center (cancel);
      g_assert_false (gtk_widget_get_realized (dialog)); g_object_unref (dialog);
    } else {
      auto *control = field (f.popup, "painter-tree-layer-lock-alpha");
      input.center (control);
      g_assert_cmpint (gimp_layer_get_lock_alpha (f.target), ==, !alpha);
      g_assert_cmpuint (f.depth (), ==, depth + 1);
      screenshot (f, "-bottom"); input.key (f.popup, GDK_KEY_Escape);
      g_assert_false (gtk_widget_get_realized (f.popup));
      g_assert_true (gimp_image_undo (f.image));
    }
  }
  g_assert_null (gtk_grab_get_current ());
  g_assert_true (gimp_item_is_attached (GIMP_ITEM (f.target)));
  g_assert_true (gimp_item_get_image (GIMP_ITEM (f.target)) == f.image);
  g_assert_cmpuint (f.depth (), ==, depth);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.target), ==, opacity);
  g_assert_cmpint (gimp_layer_get_mode (f.target), ==, mode);
  g_assert_cmpint (gimp_layer_get_lock_alpha (f.target), ==, alpha);
  if (f.route.clone) g_assert_true (gimp_clone_layer_get_source (GIMP_CLONE_LAYER (f.target)) == clone_source);
}

void actual_entry (gconstpointer data) {
  if (!require_input ()) return;
  auto& route = *static_cast<const Route *> (data); Input input; Fixture f (route); f.open (input);
  assert_geometry (f); assert_visible (f.popup);
  auto *opacity = field (f.popup, f.opacity_name ()); input.center (opacity);
  auto before = gimp_layer_get_opacity (f.target); auto depth = f.depth ();
  input.key (opacity, GDK_KEY_Left);
  g_assert_cmpfloat (gimp_layer_get_opacity (f.target), <, before); g_assert_cmpuint (f.depth (), ==, depth + 1);
  const auto edited = gimp_layer_get_opacity (f.target);
  for (auto key : {GDK_KEY_Return, GDK_KEY_KP_Enter, GDK_KEY_space}) {
    input.key (f.popup, key); g_assert_true (gtk_widget_get_mapped (f.popup));
    g_assert_cmpfloat (gimp_layer_get_opacity (f.target), ==, edited);
  }
  screenshot (f); input.key (f.popup, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_false (gtk_widget_has_grab (f.popup));
  g_assert_cmpfloat (gimp_layer_get_opacity (f.target), ==, edited);
  g_assert_true (gimp_image_undo (f.image)); g_assert_cmpfloat (gimp_layer_get_opacity (f.target), ==, before);
  for (guint cycle = 0; cycle < 3; ++cycle) { f.open (input); input.key (f.popup, GDK_KEY_Escape); g_assert_false (gtk_widget_get_realized (f.popup)); }
  f.open (input);
  /* Direct vfunc result proves consumption; the real outside click below
   * proves GTK dispatch does not activate the newly uncovered target. */
  auto *event = gdk_event_new (GDK_BUTTON_PRESS); event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (f.popup)));
  event->button.x = -10; event->button.y = -10; event->button.button = 1;
  gdk_event_set_device (event, gdk_seat_get_pointer (gdk_display_get_default_seat (gtk_widget_get_display (f.popup))));
  g_assert_true (gtk_widget_event (f.popup, event)); gdk_event_free (event);
  g_assert_false (gtk_widget_get_realized (f.popup)); f.open (input);
  MappedWindows before_outside;
  input.click (f.anchor, f.hit_x, f.hit_y, route.tree ? 1 : 3);
  const bool new_window = before_outside.has_new (input);
  if (gtk_widget_get_realized (f.popup) || gtk_grab_get_current () || new_window) input.failure ("outside-dismissal");
  g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
  g_assert_false (new_window);
}
struct RapidInput { GdkWindow *window; guint button; guint32 first = 0, latest = 0; guint doubles = 0, triples = 0; };
gboolean observe_rapid_input (GSignalInvocationHint *, guint count, const GValue *values, gpointer data) {
  if (count < 2) return TRUE;
  auto& state = *static_cast<RapidInput *> (data);
  auto *event = static_cast<const GdkEventButton *> (g_value_get_boxed (&values[1]));
  if (!event || (event->type != GDK_BUTTON_PRESS && event->type != GDK_2BUTTON_PRESS &&
                 event->type != GDK_3BUTTON_PRESS) ||
      event->window != state.window || event->button != state.button) return TRUE;
  if (!state.first) state.first = event->time;
  state.latest = event->time;
  if (event->type == GDK_2BUTTON_PRESS) ++state.doubles;
  if (event->type == GDK_3BUTTON_PRESS) ++state.triples;
  return TRUE;
}
void rapid_escape_reopen (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  RapidInput observed {f.route.tree ? gtk_tree_view_get_bin_window (GTK_TREE_VIEW (f.anchor)) :
                       gtk_widget_get_window (f.anchor), f.route.tree ? 1u : 3u};
  gint interval = 0; g_object_get (gtk_widget_get_settings (f.anchor), "gtk-double-click-time", &interval, nullptr);
  g_assert_cmpint (interval, >, 0);
  /* Observe native classification before the provider's early ::event
   * consumer. A consumed companion correctly has no button-press signal. */
  auto signal = g_signal_lookup ("event", GTK_TYPE_WIDGET);
  auto hook = g_signal_add_emission_hook (signal, 0, observe_rapid_input, &observed, nullptr); g_assert_cmpuint (hook, >, 0);
  f.open (input, true, true);
  for (guint repeat = 0; repeat < 2; ++repeat) {
    input.key (f.popup, GDK_KEY_Escape, true);
    const auto until = g_get_monotonic_time () + 2 * G_TIME_SPAN_SECOND;
    while (gtk_widget_get_realized (f.popup) && g_get_monotonic_time () < until)
      g_main_context_iteration (nullptr, FALSE);
    g_assert_false (gtk_widget_get_realized (f.popup));
    f.open (input, true, true);
  }
  g_signal_remove_emission_hook (signal, hook);
  /* Prove that this actually crossed GDK's multi-press classification, rather
   * than silently treating a slow CI run as rapid-input coverage. */
  g_assert_cmpuint (observed.latest - observed.first, <, static_cast<guint> (2 * interval));
  g_assert_cmpuint (observed.doubles + observed.triples, >, 0);
  g_test_message ("Actual rapid input: %u ms, double=%u triple=%u, configured interval=%d ms",
    observed.latest - observed.first, observed.doubles, observed.triples, interval);
  g_assert_true (gtk_widget_get_mapped (f.popup));
  /* A later physical outside press must still cancel, not be blanket ignored. */
  {
    MappedWindows before_outside;
    settle (); input.click (f.anchor, f.hit_x, f.hit_y, f.route.tree ? 1 : 3);
    const bool new_window = before_outside.has_new (input);
    if (gtk_widget_get_realized (f.popup) || gtk_grab_get_current () || new_window) input.failure ("rapid-outside-dismissal");
    g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
    g_assert_false (new_window);
  }
  /* Separately prove closing-click companion handling. Readiness work must
   * not accidentally make the outside click too slow to reproduce that edge.
   * Reset classification, then use the unchanged fast XTest path twice. */
  f.close (); g_usleep ((static_cast<gulong> (interval) + 10) * 1000); drain ();
  RapidInput closing {observed.window, observed.button};
  hook = g_signal_add_emission_hook (signal, 0, observe_rapid_input, &closing, nullptr); g_assert_cmpuint (hook, >, 0);
  f.open (input, true, true);
  MappedWindows before_closing;
  input.click (f.anchor, f.hit_x, f.hit_y, f.route.tree ? 1 : 3, true);
  g_signal_remove_emission_hook (signal, hook);
  const bool new_window = before_closing.has_new (input);
  g_test_message ("Actual classified outside input: %u ms, double=%u triple=%u, configured interval=%d ms",
    closing.latest - closing.first, closing.doubles, closing.triples, interval);
  if (gtk_widget_get_realized (f.popup) || gtk_grab_get_current () || new_window || !closing.first ||
      closing.latest - closing.first >= static_cast<guint> (interval) || closing.doubles + closing.triples == 0)
    input.failure ("classified-outside-dismissal");
  g_assert_cmpuint (closing.first, >, 0);
  g_assert_cmpuint (closing.latest - closing.first, <, static_cast<guint> (interval));
  g_assert_cmpuint (closing.doubles + closing.triples, >, 0);
  g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
  g_assert_false (new_window);
}
struct OpeningEvent { GtkWidget *anchor; GdkEvent *event = nullptr; guint companions = 0; };
gboolean capture_opening_event (GSignalInvocationHint *, guint count, const GValue *values, gpointer data) {
  auto& saved = *static_cast<OpeningEvent *> (data);
  if (count < 2) return TRUE;
  auto *event = static_cast<GdkEvent *> (g_value_get_boxed (&values[1]));
  if (event && gtk_get_event_widget (event) == saved.anchor &&
      (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS)) ++saved.companions;
  if (event && event->type == GDK_BUTTON_PRESS && gtk_get_event_widget (event) == saved.anchor) {
    if (saved.event) gdk_event_free (saved.event);
    saved.event = gdk_event_copy (event);
  }
  return TRUE;
}
void opening_provenance_mismatch (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  /* Actual opening records the native event. Each direct companion probe
   * differs in exactly one provenance field and must retain outside TRUE.
   * This is a handler negative control, not a claim of physical mismatch input. */
  gint interval = 0; g_object_get (gtk_widget_get_settings (f.anchor), "gtk-double-click-time", &interval, nullptr);
  g_assert_cmpint (interval, >, 0);
  for (guint mismatch = 0; mismatch < 6; ++mismatch) {
    /* This probe must leave the pending provenance unconsumed; unlike the
     * rapid test, start beyond the real configured multi-click interval. */
    g_usleep ((static_cast<gulong> (interval) + 10) * 1000); drain ();
    OpeningEvent saved {f.anchor};
    const auto signal = g_signal_lookup ("button-press-event", GTK_TYPE_WIDGET);
    const auto hook = g_signal_add_emission_hook (signal, 0, capture_opening_event, &saved, nullptr);
    g_assert_cmpuint (hook, >, 0); f.open (input);
    g_signal_remove_emission_hook (signal, hook); g_assert_nonnull (saved.event);
    g_assert_cmpuint (saved.companions, ==, 0);
    auto *event = saved.event; event->type = GDK_2BUTTON_PRESS;
    if (mismatch == 0) ++event->button.time;
    else if (mismatch == 1) event->button.button = event->button.button == 1 ? 3 : 1;
    else if (mismatch == 2) {
      g_object_unref (event->button.window);
      event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (gtk_widget_get_toplevel (f.anchor))));
    } else if (mismatch == 3)
      gdk_event_set_device (event, gdk_seat_get_keyboard (gdk_display_get_default_seat (gtk_widget_get_display (f.anchor))));
    else if (mismatch == 4) ++event->button.x_root;
    else event->type = GDK_BUTTON_PRESS;
    g_assert_true (gtk_widget_event (f.popup, event)); gdk_event_free (event);
    g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
  }
}
/* Handler contract probes complement (not replace) the real XTest classified
 * outside pair. gtk_main_do_event supplies GTK's native current-event stack
 * for arming; direct receiver calls vary one field without asking the server
 * to generate impossible provenance combinations. */
GdkEvent *closing_event (Fixture& f, Input& input, GtkWidget *receiver, guint button,
                         gint x, gint y) {
  OpeningEvent saved {f.anchor};
  const auto signal = g_signal_lookup ("button-press-event", GTK_TYPE_WIDGET);
  const auto hook = g_signal_add_emission_hook (signal, 0, capture_opening_event, &saved, nullptr);
  g_assert_cmpuint (hook, >, 0); f.open (input); g_signal_remove_emission_hook (signal, hook);
  g_assert_nonnull (saved.event);
  auto *event = saved.event;
  if (f.route.tree && receiver == f.anchor) { x = f.hit_x; y = f.hit_y; }
  auto *window = f.route.tree && receiver == f.anchor ? gtk_tree_view_get_bin_window (GTK_TREE_VIEW (receiver)) :
    GTK_IS_BUTTON (receiver) ? gtk_button_get_event_window (GTK_BUTTON (receiver)) : gtk_widget_get_window (receiver);
  g_assert_nonnull (window); g_object_unref (event->button.window);
  event->button.window = GDK_WINDOW (g_object_ref (window));
  event->type = GDK_BUTTON_PRESS; event->button.button = button; ++event->button.time;
  const auto point = Input::geometry (receiver, x, y);
  if (f.route.tree && receiver == f.anchor)
    gtk_tree_view_convert_widget_to_bin_window_coords (GTK_TREE_VIEW (receiver), x, y, &x, &y);
  event->button.x = x; event->button.y = y;
  event->button.x_root = point.x; event->button.y_root = point.y;
  g_assert_true (gtk_get_event_widget (event) == receiver);
  return event;
}
void closing_provenance_exact (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  auto *filter = gimp_filter_layer_new (f.image, 320, 240, "Covered Filter row", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  g_assert_true (gimp_image_add_layer (f.image, filter, nullptr, 1, FALSE)); drain ();
  auto *add = field (f.owner, "painter-layer-add");
  for (guint destination = 0; destination < 4; ++destination) for (guint button : {1u, 3u})
    for (auto type : {GDK_2BUTTON_PRESS, GDK_3BUTTON_PRESS}) {
      auto *receiver = destination == 3 ? add : f.anchor;
      const gint x = destination == 3 ? gtk_widget_get_allocated_width (add) / 2 : destination == 1 ? 8 : 28;
      const gint y = destination == 3 ? gtk_widget_get_allocated_height (add) / 2 : destination == 1 ? 8 : destination == 2 ? 96 : 32;
      auto *event = closing_event (f, input, receiver, button, x, y);
      const auto depth = f.depth (); const auto count = gimp_image_get_n_layers (f.image);
      const auto visible = gimp_item_get_visible (GIMP_ITEM (f.target));
      MappedWindows before;
      gtk_main_do_event (event);
      g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
      event->type = type; g_assert_true (gtk_widget_event (receiver, event));
      event->type = GDK_BUTTON_RELEASE; gtk_widget_event (receiver, event); gdk_event_free (event); drain ();
      const bool new_window = before.has_new (input);
      if (new_window || gtk_grab_get_current ()) input.failure ("closing-provenance-exact");
      g_assert_false (new_window); g_assert_null (gtk_grab_get_current ());
      g_assert_cmpuint (f.depth (), ==, depth); g_assert_cmpint (gimp_image_get_n_layers (f.image), ==, count);
      g_assert_cmpint (gimp_item_get_visible (GIMP_ITEM (f.target)), ==, visible);
      auto *selection = gimp_image_get_selected_layers (f.image);
      g_assert_cmpuint (g_list_length (selection), ==, 1); g_assert_true (selection->data == f.target);
    }
}
void veto_cancel (GtkWidget *widget, gpointer) { g_signal_stop_emission_by_name (widget, "cancel"); }
void closing_provenance_mismatch (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  /* Exact left companions are inert; each mismatch must retain the original
   * visibility action. Release/new normal input, one-shot consumption and a
   * vetoed cancel followed by unrelated destruction also invalidate state. */
  for (guint mismatch = 0; mismatch < 11; ++mismatch) {
    auto *event = closing_event (f, input, f.anchor, 1, 8, 8);
    const auto visible = gimp_item_get_visible (GIMP_ITEM (f.target));
    gulong veto = mismatch == 10 ? g_signal_connect (f.popup, "cancel", G_CALLBACK (veto_cancel), nullptr) : 0;
    gtk_main_do_event (event);
    if (veto) {
      g_assert_true (gtk_widget_get_mapped (f.popup)); g_signal_handler_disconnect (f.popup, veto);
      gtk_widget_destroy (f.popup); // no current native event: must not confirm the staged candidate
    }
    g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
    event->type = GDK_2BUTTON_PRESS;
    if (mismatch == 0) ++event->button.time;
    else if (mismatch == 1) event->button.button = 3;
    else if (mismatch == 2) {
      g_object_unref (event->button.window);
      event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (gtk_widget_get_toplevel (f.anchor))));
    } else if (mismatch == 3)
      gdk_event_set_device (event, gdk_seat_get_keyboard (gdk_display_get_default_seat (gtk_widget_get_display (f.anchor))));
    else if (mismatch == 4) ++event->button.x_root;
    else if (mismatch == 5) ++event->button.y_root;
    else if (mismatch == 6) event->type = GDK_BUTTON_PRESS;
    else if (mismatch == 7) {
      auto *release = gdk_event_copy (event); release->type = GDK_BUTTON_RELEASE;
      gtk_widget_event (f.anchor, release); gdk_event_free (release);
    } else if (mismatch == 8) {
      g_assert_true (gtk_widget_event (f.anchor, event)); // first exact companion consumes once
      g_assert_cmpint (gimp_item_get_visible (GIMP_ITEM (f.target)), ==, visible);
    } else if (mismatch == 9) {
      auto *normal = gdk_event_copy (event); normal->type = GDK_BUTTON_PRESS; normal->button.button = 2;
      gtk_widget_event (f.anchor, normal); gdk_event_free (normal); // even unaccepted new input clears
    }
    g_assert_true (gtk_widget_event (f.anchor, event)); gdk_event_free (event); drain ();
    g_assert_cmpint (gimp_item_get_visible (GIMP_ITEM (f.target)), ==, !visible);
    g_assert_null (gtk_grab_get_current ());
  }
}
/* This observer sits after the provider's early ::event handler but before
 * native tree row activation. It records whether a deterministic probe passed
 * through, without letting deliberately malformed signatures open dialogs. */
gboolean count_tree_event (GtkWidget *, GdkEvent *event, gpointer data) {
  if (event->type == GDK_BUTTON_PRESS || event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
    ++*static_cast<guint *> (data); return TRUE;
  }
  return FALSE;
}
void tree_closing_exact (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  for (guint button : {1u, 3u}) for (auto type : {GDK_2BUTTON_PRESS, GDK_3BUTTON_PRESS}) {
    auto *event = closing_event (f, input, f.anchor, button, f.hit_x, f.hit_y);
    const auto depth = f.depth (); MappedWindows before;
    gtk_main_do_event (event);
    g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
    guint reached = 0;
    const auto hook = g_signal_connect (f.anchor, "event", G_CALLBACK (count_tree_event), &reached);
    event->type = type; g_assert_true (gtk_widget_event (f.anchor, event));
    g_signal_handler_disconnect (f.anchor, hook); gdk_event_free (event);
    g_assert_cmpuint (reached, ==, 0); g_assert_false (before.has_new (input));
    g_assert_null (gtk_grab_get_current ()); g_assert_cmpuint (f.depth (), ==, depth);
    auto *selection = gimp_image_get_selected_layers (f.image);
    g_assert_cmpuint (g_list_length (selection), ==, 1); g_assert_true (selection->data == f.target);
  }
}
void tree_closing_mismatch (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  for (guint mismatch = 0; mismatch < 11; ++mismatch) {
    auto *event = closing_event (f, input, f.anchor, 1, f.hit_x, f.hit_y);
    gulong veto = mismatch == 10 ? g_signal_connect (f.popup, "cancel", G_CALLBACK (veto_cancel), nullptr) : 0;
    gtk_main_do_event (event);
    if (veto) {
      g_assert_true (gtk_widget_get_mapped (f.popup)); g_signal_handler_disconnect (f.popup, veto);
      gtk_widget_destroy (f.popup);
    }
    g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
    event->type = GDK_2BUTTON_PRESS;
    if (mismatch == 0) ++event->button.time;
    else if (mismatch == 1) event->button.button = 3;
    else if (mismatch == 2) {
      g_object_unref (event->button.window);
      event->button.window = GDK_WINDOW (g_object_ref (gtk_widget_get_window (gtk_widget_get_toplevel (f.anchor))));
    } else if (mismatch == 3)
      gdk_event_set_device (event, gdk_seat_get_keyboard (gdk_display_get_default_seat (gtk_widget_get_display (f.anchor))));
    else if (mismatch == 4) ++event->button.x_root;
    else if (mismatch == 5) ++event->button.y_root;
    else if (mismatch == 6) event->type = GDK_BUTTON_PRESS;
    else if (mismatch == 7) {
      auto *release = gdk_event_copy (event); release->type = GDK_BUTTON_RELEASE;
      gtk_widget_event (f.anchor, release); gdk_event_free (release);
    } else if (mismatch == 8) g_assert_true (gtk_widget_event (f.anchor, event));
    else if (mismatch == 9) {
      auto *normal = gdk_event_copy (event); normal->type = GDK_BUTTON_PRESS; normal->button.button = 2;
      guint ignored = 0; const auto hook = g_signal_connect (f.anchor, "event", G_CALLBACK (count_tree_event), &ignored);
      gtk_widget_event (f.anchor, normal); g_signal_handler_disconnect (f.anchor, hook); gdk_event_free (normal);
      g_assert_cmpuint (ignored, ==, 1);
    }
    guint reached = 0; const auto hook = g_signal_connect (f.anchor, "event", G_CALLBACK (count_tree_event), &reached);
    g_assert_true (gtk_widget_event (f.anchor, event)); g_signal_handler_disconnect (f.anchor, hook); gdk_event_free (event);
    g_assert_cmpuint (reached, ==, 1); g_assert_null (gtk_grab_get_current ());
  }
  /* A real native tree activation is still reached for an unmatched double
   * signature, not just the observer above. Deterministic GTK dispatch, not
   * XTest: the original rapid case separately proves physical cancellation. */
  auto *event = closing_event (f, input, f.anchor, 1, f.hit_x, f.hit_y);
  gtk_main_do_event (event); g_assert_false (gtk_widget_get_realized (f.popup));
  event->type = GDK_2BUTTON_PRESS; ++event->button.time; gtk_main_do_event (event); gdk_event_free (event); drain ();
  GtkWidget *dialog = nullptr; auto *windows = gtk_window_list_toplevels ();
  for (auto *i = windows; i; i = i->next) {
    auto *window = GTK_WIDGET (i->data);
    if (gtk_widget_get_mapped (window) && !g_strcmp0 (gtk_window_get_role (GTK_WINDOW (window)),
        f.route.clone ? "gimp-clone-layer-source" : "gimp-layer-edit")) {
      g_assert_null (dialog); dialog = GTK_WIDGET (g_object_ref (window));
    }
  }
  g_list_free (windows); g_assert_nonnull (dialog); g_assert_true (GTK_IS_DIALOG (dialog));
  gtk_widget_destroy (dialog); g_object_unref (dialog); g_assert_null (gtk_grab_get_current ());
}
void mode_growth (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data));
  /* Fixture only: open with the shorter legacy model, then let real group
   * input rebuild that model. Arrowing within one model does not establish
   * size growth: GtkComboBox can reserve its widest row in advance. */
  gimp_layer_set_mode (f.target, GIMP_LAYER_MODE_NORMAL_LEGACY, FALSE);
  f.open (input); assert_geometry (f);
  auto *mode = field (f.popup, f.mode_name ());
  auto *combo = find (mode, nullptr, GIMP_TYPE_LAYER_MODE_COMBO_BOX); g_assert_nonnull (combo);
  GtkWidget *group = nullptr; auto *children = gtk_container_get_children (GTK_CONTAINER (mode));
  for (auto *i = children; i; i = i->next)
    if (i->data != combo && GTK_IS_COMBO_BOX (i->data)) { g_assert_null (group); group = GTK_WIDGET (i->data); }
  g_list_free (children); g_assert_nonnull (group);
  gint before_group; g_object_get (combo, "group", &before_group, nullptr);
  g_assert_cmpint (before_group, ==, GIMP_LAYER_MODE_GROUP_LEGACY);
  gint original_width, original_height; gtk_window_get_size (GTK_WINDOW (f.popup), &original_width, &original_height);
  const auto old_rows = gtk_tree_model_iter_n_children (gtk_combo_box_get_model (GTK_COMBO_BOX (combo)), nullptr);
  gtk_widget_grab_focus (group); input.key (group, GDK_KEY_Home);
  gint after_group; g_object_get (combo, "group", &after_group, nullptr);
  g_assert_cmpint (after_group, ==, GIMP_LAYER_MODE_GROUP_DEFAULT);
  g_assert_true (gtk_widget_get_mapped (f.popup));
  g_assert_cmpint (gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (mode)), ==, gimp_layer_get_mode (f.target));
  gint expanded_width, expanded_height; gtk_window_get_size (GTK_WINDOW (f.popup), &expanded_width, &expanded_height);
  const auto new_rows = gtk_tree_model_iter_n_children (gtk_combo_box_get_model (GTK_COMBO_BOX (combo)), nullptr);
  g_test_message ("Native group %d -> %d; model rows %d -> %d; width %d -> %d; mode=%d",
    before_group, after_group, old_rows, new_rows, original_width, expanded_width, gimp_layer_get_mode (f.target));
  g_assert_cmpint (expanded_width, >, original_width); assert_visible (f.popup);
  const auto mode_before = gimp_layer_get_mode (f.target); const auto depth = f.depth ();
  gtk_widget_grab_focus (combo); input.key (combo, GDK_KEY_Down);
  g_assert_cmpint (gimp_layer_get_mode (f.target), !=, mode_before);
  g_assert_cmpint (gimp_layer_mode_box_get_mode (GIMP_LAYER_MODE_BOX (mode)), ==, gimp_layer_get_mode (f.target));
  g_assert_cmpuint (f.depth (), ==, depth + 1); assert_visible (f.popup);
  input.key (f.popup, GDK_KEY_Escape); g_assert_true (gimp_image_undo (f.image));
  g_assert_cmpint (gimp_layer_get_mode (f.target), ==, mode_before);
}
struct Stage { Fixture *fixture; guint action; guint calls = 0; };
gboolean staged_child (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
  auto& stage = *static_cast<Stage *> (data); auto& f = *stage.fixture;
  auto *child = GTK_WIDGET (g_value_get_object (&values[0]));
  if (stage.calls || g_strcmp0 (gtk_widget_get_name (child), f.opacity_name ()) || !gtk_widget_get_parent (child)) return TRUE;
  ++stage.calls;
  /* No partially built native host may be published or mapped yet. */
  if (f.route.tree) g_assert_null (gimp_cell_renderer_popup_get_popup (f.cell));
  g_assert_null (f.popup);
  if (stage.action == 0) gtk_widget_hide (f.owner);
  else if (stage.action == 1) gtk_widget_destroy (f.owner);
  else if (stage.action == 2) {
    if (f.route.tree) gimp_item_tree_view_set_image (GIMP_ITEM_TREE_VIEW (f.owner), nullptr);
    else gimp_painter_layer_tiles_set_image (GIMP_PAINTER_LAYER_TILES (f.owner), nullptr);
  } else gimp_image_remove_layer (f.image, f.target, FALSE, nullptr);
  return TRUE;
}
void staged_lifetimes (gconstpointer data) {
  if (!require_input ()) return;
  const auto route_index = GPOINTER_TO_UINT (data) / 4, action = GPOINTER_TO_UINT (data) % 4;
  Input input; Fixture f (routes[route_index]); Stage stage {&f, action};
  auto signal = g_signal_lookup ("parent-set", GTK_TYPE_WIDGET);
  auto hook = g_signal_add_emission_hook (signal, 0, staged_child, &stage, nullptr); g_assert_cmpuint (hook, >, 0);
  f.open (input, false); g_signal_remove_emission_hook (signal, hook);
  g_assert_cmpuint (stage.calls, ==, 1); g_assert_null (f.popup); g_assert_null (gtk_grab_get_current ());
  auto *windows = gtk_window_list_toplevels ();
  for (auto *i = windows; i; i = i->next) g_assert_cmpstr (gtk_widget_get_name (GTK_WIDGET (i->data)), !=, f.popup_name ());
  g_list_free (windows);
}
struct ReopenStage { Fixture *fixture; GtkWidget *replacement = nullptr; guint calls = 0; };
gboolean staged_reopen (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
  auto& state = *static_cast<ReopenStage *> (data); auto& f = *state.fixture;
  auto *child = GTK_WIDGET (g_value_get_object (&values[0]));
  if (state.calls || g_strcmp0 (gtk_widget_get_name (child), f.opacity_name ()) || !gtk_widget_get_parent (child)) return TRUE;
  ++state.calls;
  if (f.route.tree) {
    g_assert_null (gimp_cell_renderer_popup_get_popup (f.cell));
    auto *path = f.path (); auto *text = gtk_tree_path_to_string (path);
    gimp_cell_renderer_popup_clicked (f.cell, G_OBJECT (f.anchor), text, nullptr);
    g_free (text); gtk_tree_path_free (path);
    state.replacement = gimp_cell_renderer_popup_get_popup (f.cell);
  } else state.replacement = gimp_painter_layer_tiles_popup (GIMP_PAINTER_LAYER_TILES (f.owner), FALSE);
  g_assert_nonnull (state.replacement); g_object_ref (state.replacement);
  return TRUE;
}
void staged_newer_request (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data)); ReopenStage state {&f};
  auto signal = g_signal_lookup ("parent-set", GTK_TYPE_WIDGET);
  auto hook = g_signal_add_emission_hook (signal, 0, staged_reopen, &state, nullptr); g_assert_cmpuint (hook, >, 0);
  f.open (input); g_signal_remove_emission_hook (signal, hook);
  g_assert_cmpuint (state.calls, ==, 1); g_assert_true (f.popup == state.replacement);
  auto *windows = gtk_window_list_toplevels (); guint hosts = 0;
  for (auto *i = windows; i; i = i->next)
    if (!g_strcmp0 (gtk_widget_get_name (GTK_WIDGET (i->data)), f.popup_name ())) ++hosts;
  g_list_free (windows); g_assert_cmpuint (hosts, ==, 1);
  gtk_range_set_value (GTK_RANGE (field (f.popup, f.opacity_name ())), 43);
  g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (f.target), .43, 1e-6);
  input.key (f.popup, GDK_KEY_Escape); g_assert_false (gtk_widget_get_realized (state.replacement));
  g_object_unref (state.replacement);
}
struct NamedHost { Fixture *fixture; GtkWidget *host = nullptr; guint destroyed = 0; };
void destroy_named_host (GObject *object, GParamSpec *, gpointer data) {
  auto& state = *static_cast<NamedHost *> (data);
  if (state.destroyed || g_strcmp0 (gtk_widget_get_name (GTK_WIDGET (object)), state.fixture->popup_name ())) return;
  ++state.destroyed; gtk_widget_destroy (GTK_WIDGET (object));
}
gboolean watch_host_name (GSignalInvocationHint *, guint, const GValue *values, gpointer data) {
  auto& state = *static_cast<NamedHost *> (data);
  if (state.host) return TRUE;
  auto *child = GTK_WIDGET (g_value_get_object (&values[0]));
  auto *host = gtk_widget_get_toplevel (child);
  if (g_strcmp0 (G_OBJECT_TYPE_NAME (host), "GimpPopover2") ||
      !find (child, state.fixture->opacity_name ())) return TRUE;
  state.host = GTK_WIDGET (g_object_ref (host));
  g_signal_connect (host, "notify::name", G_CALLBACK (destroy_named_host), &state);
  return TRUE;
}
void host_name_destroy (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data)); NamedHost state {&f};
  auto signal = g_signal_lookup ("parent-set", GTK_TYPE_WIDGET);
  auto hook = g_signal_add_emission_hook (signal, 0, watch_host_name, &state, nullptr); g_assert_cmpuint (hook, >, 0);
  f.open (input, false); g_signal_remove_emission_hook (signal, hook);
  g_assert_nonnull (state.host); g_assert_cmpuint (state.destroyed, ==, 1);
  g_assert_null (f.popup); g_assert_null (gtk_grab_get_current ());
  if (f.route.tree) g_assert_null (gimp_cell_renderer_popup_get_popup (f.cell));
  gpointer weak = state.host; g_object_add_weak_pointer (G_OBJECT (state.host), &weak);
  g_signal_handlers_disconnect_by_data (state.host, &state); g_object_unref (state.host);
  g_assert_null (weak);
  /* Destruction of one unpublished host does not permanently poison reopen. */
  f.open (input); input.key (f.popup, GDK_KEY_Escape); g_assert_false (gtk_widget_get_realized (f.popup));
}
void owner_target_lifetimes (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data)); f.open (input);
  auto *opacity = field (f.popup, f.opacity_name ()); g_object_ref (opacity);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (opacity)), opacity);
  const auto value = gimp_layer_get_opacity (f.target);
  gtk_widget_hide (f.owner); settle ();
  g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
  gtk_range_set_value (GTK_RANGE (opacity), 17); g_assert_cmpfloat (gimp_layer_get_opacity (f.target), ==, value);
  g_object_unref (opacity); gtk_widget_show (f.owner); settle (); f.open (input);
  gimp_image_remove_layer (f.image, f.target, FALSE, nullptr); drain ();
  g_assert_false (gtk_widget_get_realized (f.popup)); g_assert_null (gtk_grab_get_current ());
}
void group_context (gconstpointer data) {
  if (!require_input ()) return;
  Input input; Fixture f (*static_cast<const Route *> (data), true);
  /* Preserve the current policies: tree GROUP, tile LAYER. Carrier migration
   * does not widen the tile's mode set or introduce pass-through semantics. */
  const auto initial = f.route.tree ? GIMP_LAYER_MODE_PASS_THROUGH : GIMP_LAYER_MODE_NORMAL;
  const auto edited = f.route.tree ? GIMP_LAYER_MODE_NORMAL : GIMP_LAYER_MODE_MULTIPLY;
  gimp_layer_set_mode (f.target, initial, FALSE); f.open (input);
  auto *mode = GIMP_LAYER_MODE_BOX (field (f.popup, f.mode_name ()));
  g_assert_cmpint (gimp_layer_mode_box_get_context (mode), ==,
    f.route.tree ? GIMP_LAYER_MODE_CONTEXT_GROUP : GIMP_LAYER_MODE_CONTEXT_LAYER);
  g_assert_cmpint (gimp_layer_mode_box_get_mode (mode), ==, initial);
  gimp_layer_mode_box_set_mode (mode, edited);
  g_assert_cmpint (gimp_layer_get_mode (f.target), ==, edited);
  input.key (f.popup, GDK_KEY_Escape); g_assert_true (gimp_image_undo (f.image));
  g_assert_cmpint (gimp_layer_get_mode (f.target), ==, initial);
}
}
/* Production-independent test bridge: gate-specific code remains in its
 * own TU/registration, so the pinned306 carrier control imports no gate API. */
extern "C" void
gimp_test_common_native_dock_case (Gimp *application, NativeDockProbe inserted,
                                  NativeDockProbe exercise, NativeCloseProbe closing,
                                  gpointer data) {
  app = application;
  Input input; Fixture fixture (routes[2], false, inserted, closing, data);
  if (exercise) exercise (fixture.dockable, GTK_WIDGET (fixture.shell), data);
  fixture.open (input); assert_geometry (fixture);
  input.key (fixture.popup, GDK_KEY_Escape);
  g_assert_false (gtk_widget_get_realized (fixture.popup));
  g_assert_null (gtk_grab_get_current ());
}

void gimp_test_common_layer_popup_register (Gimp *application) {
  app = application;
  if (const auto *path = g_getenv ("GIMP_TEST_COMMON_POPUP_DOCK_TRACE"))
    dock_trace = std::make_unique<DockTrace> (path);
  for (auto& route : routes) {
    auto *path = g_strdup_printf ("/common-layer-native-popup/%s/actual-entry", route.name);
    g_test_add_data_func (path, &route, actual_entry); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/lower-controls", route.name);
    g_test_add_data_func (path, &route, lower_controls); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/opening-provenance-mismatch", route.name);
    g_test_add_data_func (path, &route, opening_provenance_mismatch); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/mode-growth", route.name);
    g_test_add_data_func (path, &route, mode_growth); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/owner-target-lifetimes", route.name);
    g_test_add_data_func (path, &route, owner_target_lifetimes); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/staged-newer-request", route.name);
    g_test_add_data_func (path, &route, staged_newer_request); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/host-name-destroy", route.name);
    g_test_add_data_func (path, &route, host_name_destroy); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/rapid-escape-reopen", route.name);
    g_test_add_data_func (path, &route, rapid_escape_reopen); g_free (path);
  }
  for (guint route : {0u, 1u}) {
    auto *path = g_strdup_printf ("/common-layer-native-popup/%s/closing-provenance-exact", routes[route].name);
    g_test_add_data_func (path, &routes[route], closing_provenance_exact); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/closing-provenance-mismatch", routes[route].name);
    g_test_add_data_func (path, &routes[route], closing_provenance_mismatch); g_free (path);
  }
  for (guint route : {2u, 3u}) {
    auto *path = g_strdup_printf ("/common-layer-native-popup/%s/closing-provenance-exact", routes[route].name);
    g_test_add_data_func (path, &routes[route], tree_closing_exact); g_free (path);
    path = g_strdup_printf ("/common-layer-native-popup/%s/closing-provenance-mismatch", routes[route].name);
    g_test_add_data_func (path, &routes[route], tree_closing_mismatch); g_free (path);
  }
  for (guint route = 0; route < G_N_ELEMENTS (routes); ++route) for (guint action = 0; action < 4; ++action) {
    auto *path = g_strdup_printf ("/common-layer-native-popup/%s/staged-child-%u", routes[route].name, action);
    g_test_add_data_func (path, GUINT_TO_POINTER (route * 4 + action), staged_lifetimes); g_free (path);
  }
  g_test_add_data_func ("/common-layer-native-popup/group-tile", &routes[0], group_context);
  g_test_add_data_func ("/common-layer-native-popup/group-tree", &routes[2], group_context);
}
