/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Separate controlled-scheduling tests. These do not replace the 58 real-input
 * common popup cases or claim universal GTK allocation readiness. */
#include "config.h"
#include <gtk/gtk.h>
#include <memory>
#include <vector>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpoverlaybox.h"
#include "widgets/gimpoverlaychild.h"
void gimp_test_common_native_dock_case (Gimp *,
  void (*) (GtkWidget *, GtkWidget *, gpointer),
  void (*) (GtkWidget *, GtkWidget *, gpointer),
  void (*) (GtkWidget *, gpointer), gpointer);
void gimp_test_overlay_paint_gate_register (Gimp *);
}
namespace {
Gimp *app;
struct Connection {
  GWeakRef object; gulong id;
  Connection (GObject *value, const char *signal, GCallback callback, gpointer data, bool after = true) {
    g_weak_ref_init (&object, value);
    id = g_signal_connect_data (value, signal, callback, data, nullptr,
      after ? G_CONNECT_AFTER : GConnectFlags (0));
  }
  ~Connection () {
    auto *value = g_weak_ref_get (&object);
    if (value) { if (g_signal_handler_is_connected (value, id)) g_signal_handler_disconnect (value, id); g_object_unref (value); }
    g_weak_ref_clear (&object);
  }
};
template<class Predicate> void await (const char *what, Predicate predicate) {
  const auto deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
  while (!predicate () && g_get_monotonic_time () < deadline) {
    g_main_context_iteration (nullptr, FALSE); g_usleep (100);
  }
  g_test_message ("gate predicate: %s", what); g_assert_true (predicate ());
}
void process_existing (GdkWindow *window) {
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_process_updates (window, FALSE);
  G_GNUC_END_IGNORE_DEPRECATIONS
}
struct Probe {
  struct TabWatch {
    Probe& owner;
    GWeakRef page, book, tab;
    std::unique_ptr<Connection> notify, allocation;
    guint64 allocation_serial = 0;
    guint bindings = 0;
    TabWatch (Probe& value, GtkWidget *book_widget, GtkWidget *page_widget) : owner (value) {
      g_weak_ref_init (&page, page_widget); g_weak_ref_init (&book, book_widget); g_weak_ref_init (&tab, nullptr);
      notify = std::make_unique<Connection> (G_OBJECT (page_widget), "child-notify::tab-label", G_CALLBACK (changed), this);
      bind ();
    }
    ~TabWatch () {
      notify.reset (); allocation.reset ();
      g_weak_ref_clear (&tab); g_weak_ref_clear (&book); g_weak_ref_clear (&page);
    }
    void bind () {
      auto *page_widget = GTK_WIDGET (g_weak_ref_get (&page));
      auto *book_widget = GTK_WIDGET (g_weak_ref_get (&book));
      GtkWidget *current = page_widget && book_widget && gtk_widget_get_parent (page_widget) == book_widget ?
        gtk_notebook_get_tab_label (GTK_NOTEBOOK (book_widget), page_widget) : nullptr;
      auto *previous = GTK_WIDGET (g_weak_ref_get (&tab));
      if (current != previous) {
        allocation.reset (); allocation_serial = 0; ++bindings;
        g_weak_ref_set (&tab, current);
        if (current) allocation = std::make_unique<Connection> (G_OBJECT (current), "size-allocate", G_CALLBACK (allocated), this);
        if (owner.selected == this) {
          g_weak_ref_set (&owner.tab, current); owner.tab_serial = 0;
        }
      }
      if (previous) g_object_unref (previous);
      if (book_widget) g_object_unref (book_widget);
      if (page_widget) g_object_unref (page_widget);
    }
    static void changed (GtkWidget *, GParamSpec *, gpointer data) { static_cast<TabWatch *> (data)->bind (); }
    static void allocated (GtkWidget *widget, GtkAllocation *, gpointer data) {
      auto& watch = *static_cast<TabWatch *> (data);
      auto *current = g_weak_ref_get (&watch.tab);
      if (current == G_OBJECT (widget)) {
        watch.allocation_serial = ++watch.owner.serial; ++watch.owner.allocations;
        if (watch.owner.selected == &watch) watch.owner.tab_serial = watch.allocation_serial;
      }
      if (current) g_object_unref (current);
    }
  };
  GWeakRef host, book, tab;
  std::vector<std::unique_ptr<TabWatch>> pages;
  TabWatch *selected = nullptr;
  guint page_added_signal = 0; gulong page_added_hook = 0;
  GdkWindow *window = nullptr;
  GdkFrameClock *clock = nullptr;
  std::vector<std::unique_ptr<Connection>> connections;
  guint64 serial = 0, layout_serial = 0, after_serial = 0, tab_serial = 0;
  guint64 first_draw_tab = 0, first_draw_layout = 0, first_draw_after = 0;
  guint draws = 0, draw_callbacks = 0, unallocated_draws = 0, allocations = 0, layouts = 0, afters = 0;
  guintptr first_draw_window = 0;
  gint first_draw_window_type = -1;
  bool arm_close = false, close_armed = false;
  gulong old_layout_id = 0, old_after_id = 0;
  Probe () {
    g_weak_ref_init (&host, nullptr); g_weak_ref_init (&book, nullptr); g_weak_ref_init (&tab, nullptr);
    auto *klass = g_type_class_ref (GTK_TYPE_NOTEBOOK);
    page_added_signal = g_signal_lookup ("page-added", GTK_TYPE_NOTEBOOK); g_assert_cmpuint (page_added_signal, >, 0);
    page_added_hook = g_signal_add_emission_hook (page_added_signal, 0, page_added, this, nullptr);
    g_assert_cmpuint (page_added_hook, >, 0); g_type_class_unref (klass);
  }
  ~Probe () {
    g_signal_remove_emission_hook (page_added_signal, page_added_hook);
    connections.clear (); pages.clear ();
    g_weak_ref_clear (&tab); g_weak_ref_clear (&book); g_weak_ref_clear (&host);
    if (clock) g_object_unref (clock);
    if (window) g_object_unref (window);
  }
  static gboolean page_added (GSignalInvocationHint *, guint count, const GValue *values, gpointer data) {
    auto& p = *static_cast<Probe *> (data);
    if (count >= 3 && GIMP_IS_DOCKBOOK (g_value_get_object (&values[0]))) {
      auto *book = GTK_WIDGET (g_value_get_object (&values[0]));
      auto *page = GTK_WIDGET (g_value_get_object (&values[1]));
      g_assert_cmpuint (p.pages.size (), <, 16);
      p.pages.push_back (std::make_unique<TabWatch> (p, book, page));
    }
    return TRUE;
  }
  void connect (GObject *object, const char *signal, GCallback callback, bool after = true) {
    connections.push_back (std::make_unique<Connection> (object, signal, callback, this, after));
  }
  template<class Function> void with_child (Function function) {
    auto *widget = GTK_WIDGET (g_weak_ref_get (&host)); g_assert_nonnull (widget);
    auto *parent = gtk_widget_get_parent (widget); g_assert_true (GIMP_IS_OVERLAY_BOX (parent));
    auto *child = gimp_overlay_child_find (GIMP_OVERLAY_BOX (parent), widget); g_assert_nonnull (child);
    g_assert_true (child->window == window); function (child, widget); g_object_unref (widget);
  }
  bool frozen () { bool result = false; with_child ([&] (GimpOverlayChild *child, GtkWidget *) { result = child->paint_frozen; }); return result; }
  void queue_redraw () {
    auto *widget = GTK_WIDGET (g_weak_ref_get (&book)); g_assert_nonnull (widget);
    gtk_widget_queue_draw (widget); g_object_unref (widget);
  }
  void queue_resize () {
    auto *widget = GTK_WIDGET (g_weak_ref_get (&book)); g_assert_nonnull (widget);
    gtk_widget_queue_resize (widget); g_object_unref (widget);
  }
  static void allocated (GtkWidget *, GtkAllocation *, gpointer data) {
    auto& p = *static_cast<Probe *> (data); ++p.serial;
    ++p.allocations;
  }
  static void layout (GdkFrameClock *, gpointer data) {
    auto& p = *static_cast<Probe *> (data); p.layout_serial = ++p.serial; ++p.layouts;
  }
  static void after (GdkFrameClock *, gpointer data) {
    auto& p = *static_cast<Probe *> (data); p.after_serial = ++p.serial; ++p.afters;
  }
  static gboolean draw (GtkWidget *, cairo_t *cr, gpointer data) {
    auto& p = *static_cast<Probe *> (data);
    if (!p.draw_callbacks++) {
      auto *context = gdk_cairo_get_drawing_context (cr);
      auto *window = context ? gdk_drawing_context_get_window (context) : nullptr;
      p.first_draw_window = reinterpret_cast<guintptr> (window);
      if (window) p.first_draw_window_type = gdk_window_get_window_type (window);
    }
    if (gtk_cairo_should_draw_window (cr, p.window)) {
      ++p.serial;
      if (!p.tab_serial) ++p.unallocated_draws;
      if (!p.draws) {
        p.first_draw_tab = p.tab_serial; p.first_draw_layout = p.layout_serial; p.first_draw_after = p.after_serial;
      }
      ++p.draws;
    }
    return FALSE; // Observe native child drawing, never consume it.
  }
  void report (const char *phase) {
    auto *host_widget = GTK_WIDGET (g_weak_ref_get (&host));
    auto *book_widget = GTK_WIDGET (g_weak_ref_get (&book));
    auto *tab_widget = GTK_WIDGET (g_weak_ref_get (&tab));
    GtkWidget *current_tab = nullptr;
    gint current_page = -1;
    if (book_widget) {
      current_page = gtk_notebook_get_current_page (GTK_NOTEBOOK (book_widget));
      auto *page = current_page >= 0 ? gtk_notebook_get_nth_page (GTK_NOTEBOOK (book_widget), current_page) : nullptr;
      if (page) current_tab = gtk_notebook_get_tab_label (GTK_NOTEBOOK (book_widget), page);
    }
    auto widget_state = [] (const char *label, GtkWidget *widget) {
      GtkAllocation allocation = {0, 0, 0, 0};
      if (widget) gtk_widget_get_allocation (widget, &allocation);
      g_test_message ("gate widget %s=%p visible=%d mapped=%d realized=%d allocation=%d,%d,%d,%d",
        label, static_cast<void *> (widget), widget && gtk_widget_get_visible (widget),
        widget && gtk_widget_get_mapped (widget), widget && gtk_widget_get_realized (widget),
        allocation.x, allocation.y, allocation.width, allocation.height);
    };
    g_test_message ("gate state phase=%s draw-callbacks=%u native-draws=%u unallocated-native-draws=%u allocations=%u layouts=%u afters=%u tab-serial=%" G_GUINT64_FORMAT " first-native-tab=%" G_GUINT64_FORMAT " first-native-layout=%" G_GUINT64_FORMAT " first-native-after=%" G_GUINT64_FORMAT " native-window=%p first-draw-context-window=0x%" G_GINTPTR_MODIFIER "x first-context-type=%d current-page=%d same-tab=%d",
      phase, draw_callbacks, draws, unallocated_draws, allocations, layouts, afters, tab_serial,
      first_draw_tab, first_draw_layout, first_draw_after, static_cast<void *> (window),
      first_draw_window, first_draw_window_type, current_page, tab_widget && tab_widget == current_tab);
    g_test_message ("gate observed page-added=%u selected-tab-bindings=%u", static_cast<guint> (pages.size ()), selected ? selected->bindings : 0);
    widget_state ("host", host_widget); widget_state ("book", book_widget);
    widget_state ("observed-tab", tab_widget); widget_state ("current-tab", current_tab);
    if (host_widget) {
      auto *parent = gtk_widget_get_parent (host_widget);
      auto *child = GIMP_IS_OVERLAY_BOX (parent) ? gimp_overlay_child_find (GIMP_OVERLAY_BOX (parent), host_widget) : nullptr;
      if (child) g_test_message ("gate witness frozen=%d seen=%d generation=%" G_GUINT64_FORMAT " layout-generation=%" G_GUINT64_FORMAT " logical-clock=%p native-clock=%p",
        child->paint_frozen, child->paint_layout_seen, child->paint_generation,
        child->paint_layout_generation, static_cast<void *> (child->paint_clock),
        static_cast<void *> (window && !gdk_window_is_destroyed (window) ? gdk_window_get_frame_clock (window) : nullptr));
    }
    if (tab_widget) g_object_unref (tab_widget);
    if (book_widget) g_object_unref (book_widget);
    if (host_widget) g_object_unref (host_widget);
  }
  static void inserted (GtkWidget *dockable, GtkWidget *, gpointer data) {
    auto& p = *static_cast<Probe *> (data);
    auto *book = gtk_widget_get_parent (dockable); g_assert_true (GIMP_IS_DOCKBOOK (book));
    auto *host = book;
    while (host && g_strcmp0 (gtk_widget_get_name (host), "painter-right-docks")) host = gtk_widget_get_parent (host);
    g_assert_nonnull (host); g_assert_true (GIMP_IS_OVERLAY_BOX (gtk_widget_get_parent (host)));
    for (auto& watch : p.pages) {
      auto *page = g_weak_ref_get (&watch->page);
      if (page == G_OBJECT (dockable)) p.selected = watch.get ();
      if (page) g_object_unref (page);
    }
    // The native strategy must actually insert this page, not reuse a session tab.
    g_assert_nonnull (p.selected);
    auto *observed_book = g_weak_ref_get (&p.selected->book);
    g_assert_true (observed_book == G_OBJECT (book)); if (observed_book) g_object_unref (observed_book);
    auto *tab = gtk_notebook_get_tab_label (GTK_NOTEBOOK (book), dockable); g_assert_nonnull (tab);
    auto *observed = g_weak_ref_get (&p.selected->tab);
    g_assert_true (observed == G_OBJECT (tab)); if (observed) g_object_unref (observed);
    p.tab_serial = p.selected->allocation_serial;
    g_weak_ref_set (&p.host, host); g_weak_ref_set (&p.book, book); g_weak_ref_set (&p.tab, tab);
    auto *native = gtk_widget_get_window (book); g_assert_nonnull (native);
    p.window = GDK_WINDOW (g_object_ref (gdk_window_get_toplevel (native)));
    g_assert_cmpint (gdk_window_get_window_type (p.window), ==, GDK_WINDOW_OFFSCREEN);
    p.clock = GDK_FRAME_CLOCK (g_object_ref (gtk_widget_get_frame_clock (host)));
    g_assert_true (p.clock != gdk_window_get_frame_clock (p.window));
    p.connect (G_OBJECT (host), "draw", G_CALLBACK (draw), false);
    p.connect (G_OBJECT (book), "size-allocate", G_CALLBACK (allocated));
    p.connect (G_OBJECT (p.clock), "layout", G_CALLBACK (layout));
    p.connect (G_OBJECT (p.clock), "after-paint", G_CALLBACK (after));
    p.with_child ([&] (GimpOverlayChild *child, GtkWidget *) {
      g_assert_true (child->paint_frozen); g_assert_true (child->paint_clock == p.clock);
    });
    p.report ("inserted-before-existing-updates");
    process_existing (p.window); g_assert_cmpuint (p.draws, ==, 0);
    g_assert_true (p.frozen ());
  }
  void wait_painted () {
    report ("wait-painted-start");
    const auto deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;
    bool reported = false;
    await ("real allocation and native child paint", [&] {
      if (!reported && !(draws && tab_serial && afters) && g_get_monotonic_time () >= deadline) {
        report ("wait-painted-timeout"); reported = true;
      }
      return draws && tab_serial && afters;
    });
    g_assert_cmpuint (first_draw_tab, >, 0); g_assert_cmpuint (first_draw_layout, >, 0);
    g_assert_cmpuint (first_draw_after, >, 0); g_assert_cmpuint (unallocated_draws, ==, 0);
    auto *widget = GTK_WIDGET (g_weak_ref_get (&tab)); g_assert_nonnull (widget);
    g_assert_true (gtk_widget_get_visible (widget));
    g_assert_cmpint (gtk_widget_get_allocated_width (widget), >, 1);
    g_assert_cmpint (gtk_widget_get_allocated_height (widget), >, 1); g_object_unref (widget);
  }
  void await_quiet_allocations () {
    guint stable = 0, previous_allocations = allocations, previous_afters = afters;
    gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
    await ("three real frames without allocation churn", [&] {
      if (afters > previous_afters) {
        stable = allocations == previous_allocations && !frozen () ? stable + 1 : 0;
        previous_allocations = allocations; previous_afters = afters;
        if (stable < 3) gdk_frame_clock_request_phase (clock, GDK_FRAME_CLOCK_PHASE_AFTER_PAINT);
      }
      return stable >= 3;
    });
  }
  static void exercise (GtkWidget *, GtkWidget *, gpointer data) { static_cast<Probe *> (data)->wait_painted (); }
  static void closing (GtkWidget *, gpointer data) {
    auto& p = *static_cast<Probe *> (data); if (!p.arm_close) return;
    p.with_child ([&] (GimpOverlayChild *child, GtkWidget *host) {
      const auto generation = child->paint_generation;
      gtk_widget_queue_draw (host);
      g_assert_true (child->paint_frozen); g_assert_true (child->paint_generation > generation);
      p.old_layout_id = child->paint_layout_id; p.old_after_id = child->paint_after_id;
      g_assert_cmpuint (p.old_layout_id, >, 0); g_assert_cmpuint (p.old_after_id, >, 0);
      p.close_armed = true;
    });
  }
};
void ensure_native_warmup () {
  static bool warmed = false;
  if (warmed) return;
  // The first native strategy may reuse a restored session tab. Close that
  // real fixture normally so each observed case must prove a new page-added.
  gimp_test_common_native_dock_case (app, nullptr, nullptr, nullptr, nullptr);
  warmed = true;
}
void native_insertion () {
  ensure_native_warmup (); Probe p; gimp_test_common_native_dock_case (app, Probe::inserted, Probe::exercise, nullptr, &p);
}
void pending_teardown () {
  ensure_native_warmup (); Probe p; p.arm_close = true;
  gimp_test_common_native_dock_case (app, Probe::inserted, Probe::exercise, Probe::closing, &p);
  g_assert_true (p.close_armed); g_assert_true (gdk_window_is_destroyed (p.window));
  g_assert_null (g_object_get_data (G_OBJECT (p.window), "gimp-overlay-child-paint-gate"));
  g_assert_false (g_signal_handler_is_connected (p.clock, p.old_layout_id));
  g_assert_false (g_signal_handler_is_connected (p.clock, p.old_after_id));
  const auto before = p.afters;
  gdk_frame_clock_request_phase (p.clock, static_cast<GdkFrameClockPhase> (GDK_FRAME_CLOCK_PHASE_LAYOUT | GDK_FRAME_CLOCK_PHASE_AFTER_PAINT));
  await ("old clock genuinely dispatched after owner teardown", [&] { return p.afters > before; });
}
struct Late : Probe {
  bool paint = false, fired = false, checked = false;
  guint layouts_at_mutation = 0, draws_at_mutation = 0;
  static void mutate (GdkFrameClock *, gpointer data) {
    auto& p = *static_cast<Late *> (static_cast<Probe *> (data)); if (p.fired) return;
    p.fired = true; p.layouts_at_mutation = p.layouts; p.draws_at_mutation = p.draws;
    p.queue_resize (); g_assert_true (p.frozen ()); process_existing (p.window);
    g_assert_cmpuint (p.draws, ==, p.draws_at_mutation);
  }
  static void check_after (GdkFrameClock *, gpointer data) {
    auto& p = *static_cast<Late *> (static_cast<Probe *> (data));
    if (p.fired && p.layouts == p.layouts_at_mutation) {
      g_assert_true (p.frozen ()); g_assert_cmpuint (p.draws, ==, p.draws_at_mutation); p.checked = true;
    }
  }
  static void exercise_late (GtkWidget *, GtkWidget *, gpointer data) {
    auto& p = *static_cast<Late *> (static_cast<Probe *> (data)); p.wait_painted ();
    /* Attach after the gate's existing handlers. The one-shot mutation must
     * invalidate its earlier witness; a later real layout may be same-frame. */
    Connection mutation (G_OBJECT (p.clock), p.paint ? "paint" : "layout", G_CALLBACK (mutate), static_cast<Probe *> (&p), true);
    Connection check (G_OBJECT (p.clock), "after-paint", G_CALLBACK (check_after), static_cast<Probe *> (&p), true);
    p.queue_redraw ();
    await ("one-shot late mutation then later layout and native draw", [&] {
      return p.fired && p.layouts > p.layouts_at_mutation && p.draws > p.draws_at_mutation;
    });
    if (p.paint) g_assert_true (p.checked);
  }
};
void late_layout () { ensure_native_warmup (); Late p; gimp_test_common_native_dock_case (app, Probe::inserted, Late::exercise_late, nullptr, static_cast<Probe *> (&p)); }
void late_paint () { ensure_native_warmup (); Late p; p.paint = true; gimp_test_common_native_dock_case (app, Probe::inserted, Late::exercise_late, nullptr, static_cast<Probe *> (&p)); }
struct Redraw : Probe {
  static void exercise_redraw (GtkWidget *, GtkWidget *, gpointer data) {
    auto& p = *static_cast<Redraw *> (static_cast<Probe *> (data)); p.wait_painted ();
    p.await_quiet_allocations ();
    const auto before_draw = p.draws, before_alloc = p.allocations;
    guint64 generation = 0; p.with_child ([&] (GimpOverlayChild *child, GtkWidget *) { generation = child->paint_generation; });
    gdk_window_freeze_updates (p.window); // A distinct test-owned token.
    for (guint i = 0; i < 4; ++i) p.queue_redraw ();
    p.with_child ([&] (GimpOverlayChild *child, GtkWidget *) {
      g_assert_true (child->paint_frozen); g_assert_true (child->paint_generation > generation);
    });
    process_existing (p.window); g_assert_cmpuint (p.draws, ==, before_draw);
    const auto before_after = p.afters;
    await ("redraw-only gate completes without stealing external freeze", [&] { return p.afters > before_after && !p.frozen (); });
    process_existing (p.window); g_assert_cmpuint (p.draws, ==, before_draw);
    gdk_window_thaw_updates (p.window);
    await ("external token release permits native paint", [&] { return p.draws > before_draw; });
    g_test_message ("redraw-only allocation callbacks before=%u after=%u", before_alloc, p.allocations);
    g_assert_cmpuint (p.allocations, ==, before_alloc);
  }
};
void redraw_token_balance () { ensure_native_warmup (); Redraw p; gimp_test_common_native_dock_case (app, Probe::inserted, Redraw::exercise_redraw, nullptr, static_cast<Probe *> (&p)); }
struct Hidden : Probe {
  static void exercise_hidden (GtkWidget *, GtkWidget *, gpointer data) {
    auto& p = *static_cast<Hidden *> (static_cast<Probe *> (data)); p.wait_painted ();
    p.await_quiet_allocations ();
    auto *host = GTK_WIDGET (g_weak_ref_get (&p.host)); g_assert_nonnull (host);
    p.queue_redraw (); g_assert_true (p.frozen ());
    gtk_widget_hide (host);
    p.with_child ([&] (GimpOverlayChild *child, GtkWidget *) {
      g_assert_true (child->paint_frozen); g_assert_null (child->paint_clock);
      g_assert_cmpuint (child->paint_layout_id, ==, 0); g_assert_cmpuint (child->paint_after_id, ==, 0);
    });
    const auto draw = p.draws, after = p.afters;
    gdk_frame_clock_request_phase (p.clock, static_cast<GdkFrameClockPhase> (GDK_FRAME_CLOCK_PHASE_LAYOUT | GDK_FRAME_CLOCK_PHASE_AFTER_PAINT));
    await ("real frame while native host hidden", [&] { return p.afters > after; });
    g_assert_false (gtk_widget_get_mapped (host)); process_existing (p.window); g_assert_cmpuint (p.draws, ==, draw);
    const auto remap_layouts = p.layouts, remap_afters = p.afters;
    gtk_widget_show (host); g_object_unref (host);
    p.with_child ([&] (GimpOverlayChild *child, GtkWidget *) {
      g_assert_true (child->paint_clock == p.clock); g_assert_true (child->paint_frozen);
      g_assert_false (child->paint_layout_seen);
    });
    await ("remap receives fresh layout and native paint", [&] {
      return p.layouts > remap_layouts && p.afters > remap_afters && p.draws > draw;
    });
  }
};
void hide_remap () { ensure_native_warmup (); Hidden p; gimp_test_common_native_dock_case (app, Probe::inserted, Hidden::exercise_hidden, nullptr, static_cast<Probe *> (&p)); }
}
extern "C" void gimp_test_overlay_paint_gate_register (Gimp *application) {
  app = application;
  g_assert_null (g_getenv ("GIMP_TEST_COMMON_POPUP_DOCK_EARLY_UPDATES"));
  g_test_add_func ("/overlay-paint-gate/native-insertion", native_insertion);
  g_test_add_func ("/overlay-paint-gate/pending-teardown", pending_teardown);
  g_test_add_func ("/overlay-paint-gate/late-layout", late_layout);
  g_test_add_func ("/overlay-paint-gate/late-paint", late_paint);
  g_test_add_func ("/overlay-paint-gate/redraw-token-balance", redraw_token_balance);
  g_test_add_func ("/overlay-paint-gate/hide-remap", hide_remap);
}
