/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpcontainertreestore.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpitemtreeview.h"
#include "widgets/gimplayertreeview.h"
#include "widgets/gimplayermodebox.h"
#include "widgets/gimpviewrenderer.h"
#include "menus/menus.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
#include "widgets/gimpcellrendererpopup.h"
}
#include "painter/binding-store.hpp"
#include "widgets/gimpcellrendererpopup-handle.hpp"
#include <algorithm>
#include <atomic>
#include <vector>
#include <memory>
#include <string>

using namespace GimpPainter;
static Gimp *gimp;

struct Counts {
  std::atomic<unsigned> closure {0}, observer {0};
};
/* Override the provider with a counting-only closure. In particular, the
 * wrong-thread red control must never run the native GTK popup provider. */
struct CountingPopup {
  GimpCellRendererPopup parent_instance;
  Counts *counts;
};
struct CountingPopupClass { GimpCellRendererPopupClass parent_class; };
G_DEFINE_TYPE (CountingPopup, counting_popup, GIMP_TYPE_CELL_RENDERER_POPUP)
static void counting_clicked (GimpCellRendererPopup *cell, GObject *,
                              const gchar *, GdkRectangle *) {
  auto *self = reinterpret_cast<CountingPopup *> (cell);
  if (self->counts) ++self->counts->closure;
}
static void counting_popup_class_init (CountingPopupClass *klass) {
  klass->parent_class.parent_clicked = counting_clicked;
}
static void counting_popup_init (CountingPopup *self) { self->counts = nullptr; }
static void count_observer (GimpCellRendererPopup *, GObject *, const gchar *,
                            GdkRectangle *, gpointer data) {
  ++static_cast<Counts *> (data)->observer;
}
struct Fixture {
  GtkWidget *tree;
  GimpCellRendererPopup *cell;
  Counts counts;
  Fixture () {
    tree = gtk_tree_view_new (); g_object_ref_sink (tree);
    cell = GIMP_CELL_RENDERER_POPUP (g_object_new (counting_popup_get_type (), nullptr));
    g_object_ref_sink (cell);
    reinterpret_cast<CountingPopup *> (cell)->counts = &counts;
    g_assert_true (GTK_IS_TREE_VIEW (tree));
    g_assert_true (GIMP_IS_CELL_RENDERER_POPUP (cell));
    g_signal_connect (cell, "parent-clicked", G_CALLBACK (count_observer), &counts);
  }
  ~Fixture () {
    g_object_unref (cell);
    gtk_widget_destroy (tree); g_object_unref (tree);
  }
};
static void assert_counts (const Counts& counts, unsigned expected) {
  g_test_message ("class closure=%u, external observer=%u", counts.closure.load (), counts.observer.load ());
  g_assert_cmpuint (counts.observer.load (), ==, expected);
  g_assert_cmpuint (counts.closure.load (), ==, expected);
}
static void call_entry (bool named, GimpCellRendererPopup *cell, GObject *tree,
                        const gchar *path = "0", GdkRectangle *area = nullptr) {
  if (named) CellRendererPopupRef::retain (cell).clicked (tree, path, area);
  else gimp_cell_renderer_popup_clicked (cell, tree, path, area);
}
static void closed_entry () {
  Fixture fixture;
  auto *store = BindingStore::find (G_OBJECT (fixture.cell));
  g_assert_nonnull (store);
  const auto generation = store->generation ();
  store->close ();
  g_assert_true (store->state () == BindingStore::State::closed);
  g_assert_cmpuint (store->generation (), ==, generation + 1);
  /* Original C entry: this reproduces both observer and class-closure leakage
   * on the pre-fix implementation. Retaining storage does not reopen a store. */
  gimp_cell_renderer_popup_clicked (fixture.cell, G_OBJECT (fixture.tree), "0", nullptr);
  assert_counts (fixture.counts, 0);
  g_assert_true (BindingStore::find (G_OBJECT (fixture.cell)) == store);
  g_assert_cmpuint (store->generation (), ==, generation + 1);
}
struct WorkerCall {
  GimpCellRendererPopup *cell;
  GObject *tree;
  const CellRendererPopupRef *handle = nullptr;
};
static gpointer click_from_worker (gpointer data) {
  auto *call = static_cast<WorkerCall *> (data);
  /* All object creation, type warmup and teardown happens on the main thread.
   * The sole entry tested here is C; both callbacks only increment atomics. */
  if (call->handle) call->handle->clicked (call->tree, "0", nullptr);
  else gimp_cell_renderer_popup_clicked (call->cell, call->tree, "0", nullptr);
  return nullptr;
}
static void wrong_thread_entry_impl (bool named) {
  Fixture fixture;
  auto handle = CellRendererPopupRef::retain (fixture.cell);
  auto *store = BindingStore::find (G_OBJECT (fixture.cell));
  const auto generation = store->generation ();
  WorkerCall call {fixture.cell, G_OBJECT (fixture.tree), named ? &handle : nullptr};
  GThread *worker = g_thread_new ("popup-entry-control", click_from_worker, &call);
  g_thread_join (worker);
  assert_counts (fixture.counts, 0);
  g_assert_true (BindingStore::find (G_OBJECT (fixture.cell)) == store);
  g_assert_true (store->state () == BindingStore::State::active);
  g_assert_cmpuint (store->generation (), ==, generation);
  gimp_cell_renderer_popup_clicked (fixture.cell, G_OBJECT (fixture.tree), "0", nullptr);
  assert_counts (fixture.counts, 1);
}
static void wrong_thread_entry () { wrong_thread_entry_impl (false); }

static void invalid_arguments () {
  Fixture fixture;
  GObject *plain = G_OBJECT (g_object_new (G_TYPE_OBJECT, nullptr));
  auto *store = BindingStore::find (G_OBJECT (fixture.cell));
  const auto generation = store->generation ();
  const auto popup_generation = gimp_cell_renderer_popup_get_generation (fixture.cell);
  g_assert_null (BindingStore::find (plain));
  g_assert_null (BindingStore::find (G_OBJECT (fixture.tree)));

  g_test_expect_message (nullptr, G_LOG_LEVEL_CRITICAL,
                        "*gimp_cell_renderer_popup_clicked*GIMP_IS_CELL_RENDERER_POPUP (cell)*failed*");
  gimp_cell_renderer_popup_clicked (nullptr, G_OBJECT (fixture.tree), "0", nullptr);
  g_test_assert_expected_messages ();
  /* A valid, live GObject of the wrong type; never an arbitrary bad pointer. */
  g_test_expect_message (nullptr, G_LOG_LEVEL_CRITICAL,
                        "*gimp_cell_renderer_popup_clicked*GIMP_IS_CELL_RENDERER_POPUP (cell)*failed*");
  gimp_cell_renderer_popup_clicked (reinterpret_cast<GimpCellRendererPopup *> (plain),
                                    G_OBJECT (fixture.tree), "0", nullptr);
  g_test_assert_expected_messages ();
  for (GObject *widget : {plain, static_cast<GObject *> (nullptr)}) {
    g_test_expect_message (nullptr, G_LOG_LEVEL_CRITICAL,
                          "*gimp_cell_renderer_popup_clicked*GTK_IS_TREE_VIEW (widget)*failed*");
    gimp_cell_renderer_popup_clicked (fixture.cell, widget, "0", nullptr);
    g_test_assert_expected_messages ();
  }
  g_test_expect_message (nullptr, G_LOG_LEVEL_CRITICAL,
                        "*gimp_cell_renderer_popup_clicked*path != nullptr*failed*");
  gimp_cell_renderer_popup_clicked (fixture.cell, G_OBJECT (fixture.tree), nullptr, nullptr);
  g_test_assert_expected_messages ();

  assert_counts (fixture.counts, 0);
  g_assert_true (BindingStore::find (G_OBJECT (fixture.cell)) == store);
  g_assert_true (store->state () == BindingStore::State::active);
  g_assert_cmpuint (store->generation (), ==, generation);
  g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (fixture.cell), ==, popup_generation);
  g_assert_null (BindingStore::find (plain));
  g_assert_null (BindingStore::find (G_OBJECT (fixture.tree)));
  g_object_unref (plain);
}

struct PhaseProbe {
  GObject *tree;
  Counts counts;
  bool named, closing;
  unsigned calls = 0;
  PhaseProbe (GObject *tree, bool named, bool closing)
    : tree (tree), named (named), closing (closing) {}
};
static PhaseProbe *building_probe;
static void phase_entry (GimpCellRendererPopup *cell, PhaseProbe& probe) {
  auto *store = BindingStore::find (G_OBJECT (cell));
  const auto generation = store->generation ();
  g_assert_true (store->state () == (probe.closing ? BindingStore::State::closing :
                                                   BindingStore::State::constructing));
  ++probe.calls;
  call_entry (probe.named, cell, probe.tree);
  assert_counts (probe.counts, 0);
  g_assert_true (BindingStore::find (G_OBJECT (cell)) == store);
  g_assert_cmpuint (store->generation (), ==, generation);
}
struct CloseProbe {
  GimpCellRendererPopup *cell;
  PhaseProbe *probe;
  CloseProbe (GimpCellRendererPopup *cell, PhaseProbe *probe) : cell (cell), probe (probe) {}
  void close () noexcept { phase_entry (cell, *probe); }
};
struct CloseProbeSlot : SlotSpec<GimpCellRendererPopup, CloseProbe> {};
struct PhasePopup { CountingPopup parent_instance; PhaseProbe *probe; };
struct PhasePopupClass { CountingPopupClass parent_class; };
G_DEFINE_TYPE (PhasePopup, phase_popup, counting_popup_get_type ())
static void phase_constructed (GObject *object) {
  auto *self = reinterpret_cast<PhasePopup *> (object);
  if (!self->probe->closing) phase_entry (GIMP_CELL_RENDERER_POPUP (object), *self->probe);
  G_OBJECT_CLASS (phase_popup_parent_class)->constructed (object);
}
static void phase_popup_class_init (PhasePopupClass *klass) {
  G_OBJECT_CLASS (klass)->constructed = phase_constructed;
}
static void phase_popup_init (PhasePopup *self) {
  self->probe = building_probe;
  g_assert_nonnull (self->probe);
  self->parent_instance.counts = &self->probe->counts;
  g_signal_connect (self, "parent-clicked", G_CALLBACK (count_observer), &self->probe->counts);
  if (self->probe->closing)
    BindingStore::require (G_OBJECT (self)).emplace<CloseProbeSlot> (
      GIMP_CELL_RENDERER_POPUP (self), self->probe);
}
static void phase_rejection (bool named, bool closing) {
  GtkWidget *tree = gtk_tree_view_new (); g_object_ref_sink (tree);
  PhaseProbe probe (G_OBJECT (tree), named, closing);
  building_probe = &probe;
  auto *cell = GIMP_CELL_RENDERER_POPUP (g_object_new (phase_popup_get_type (), nullptr));
  building_probe = nullptr;
  g_object_ref_sink (cell);
  auto& store = BindingStore::require (G_OBJECT (cell));
  g_assert_true (store.state () == BindingStore::State::active);
  if (closing) {
    store.close ();
    g_assert_true (store.state () == BindingStore::State::closed);
    assert_counts (probe.counts, 0);
  } else {
    call_entry (named, cell, G_OBJECT (tree));
    assert_counts (probe.counts, 1);
  }
  g_assert_cmpuint (probe.calls, ==, 1);
  g_object_unref (cell); gtk_widget_destroy (tree); g_object_unref (tree);
}
static void retained_closed_or_disposed (bool disposed) {
  Fixture fixture;
  auto handle = CellRendererPopupRef::retain (fixture.cell);
  auto& store = BindingStore::require (G_OBJECT (fixture.cell));
  const auto generation = store.generation ();
  if (disposed) {
    g_object_run_dispose (G_OBJECT (fixture.cell));
    /* Connect after dispose so rejection cannot pass merely because GObject
     * cleared the original observer while retaining native storage. */
    g_signal_connect (fixture.cell, "parent-clicked", G_CALLBACK (count_observer), &fixture.counts);
  } else store.close ();
  g_assert_true (store.state () == BindingStore::State::closed);
  handle.clicked (G_OBJECT (fixture.tree), "0", nullptr);
  assert_counts (fixture.counts, 0);
  g_assert_true (BindingStore::find (G_OBJECT (handle.get ())) == &store);
  g_assert_cmpuint (store.generation (), ==, generation + 1);
}

struct ToggleClose {
  GObject *owner;
  bool armed = false;
  unsigned calls = 0;
};
static void toggle_close (gpointer data, GObject *, gboolean is_last_ref) {
  auto *state = static_cast<ToggleClose *> (data);
  if (!is_last_ref && state->armed) {
    state->armed = false; ++state->calls;
    BindingStore::require (state->owner).close ();
  }
}
static void final_admission_after_retain (bool tree_retain) {
  Fixture fixture;
  GObject *toggled = tree_retain ? G_OBJECT (fixture.tree) : G_OBJECT (fixture.cell);
  auto& store = BindingStore::require (G_OBJECT (fixture.cell));
  const auto generation = store.generation ();
  ToggleClose state {G_OBJECT (fixture.cell)};
  g_object_add_toggle_ref (toggled, toggle_close, &state);
  g_object_unref (toggled); // the toggle now owns the fixture's only reference
  state.armed = true;
  gimp_cell_renderer_popup_clicked (fixture.cell, G_OBJECT (fixture.tree), "0", nullptr);
  g_assert_cmpuint (state.calls, ==, 1);
  assert_counts (fixture.counts, 0);
  g_assert_true (store.state () == BindingStore::State::closed);
  g_assert_cmpuint (store.generation (), ==, generation + 1);
  /* Restore the fixture's ordinary ownership before dropping the toggle. */
  g_object_ref (toggled); g_object_remove_toggle_ref (toggled, toggle_close, &state);
}

static void named_type_and_ownership (bool retain_column_owned, bool column_first) {
  GtkTreeViewColumn *column = gtk_tree_view_column_new (); g_object_ref_sink (column);
  std::unique_ptr<CellRendererPopupRef> handle;
  GimpCellRendererPopup *cell;
  if (retain_column_owned) {
    cell = GIMP_CELL_RENDERER_POPUP (gimp_cell_renderer_popup_new ());
    g_assert_true (g_object_is_floating (cell));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), GTK_CELL_RENDERER (cell), FALSE);
    handle.reset (new CellRendererPopupRef (CellRendererPopupRef::retain (cell)));
  } else {
    handle.reset (new CellRendererPopupRef (CellRendererPopupRef::create ()));
    cell = handle->get ();
    g_assert_false (g_object_is_floating (cell));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), GTK_CELL_RENDERER (cell), FALSE);
  }
  g_assert_true (handle->get () == cell);
  g_assert_false (g_object_is_floating (cell));
  g_assert_true (G_OBJECT_TYPE (cell) == gimp_cell_renderer_popup_get_type ());
  g_assert_true (TypeTraits<GimpCellRendererPopup>::type () == G_OBJECT_TYPE (cell));
  g_assert_true (g_type_parent (G_OBJECT_TYPE (cell)) == GTK_TYPE_CELL_RENDERER_PIXBUF);
  gpointer weak = cell; g_object_add_weak_pointer (G_OBJECT (cell), &weak);
  if (column_first) {
    g_object_unref (column); g_assert_nonnull (weak); handle.reset ();
  } else {
    handle.reset (); g_assert_nonnull (weak); g_object_unref (column);
  }
  g_assert_null (weak);
}
static void named_copy_move_ownership () {
  gpointer weak;
  std::unique_ptr<CellRendererPopupRef> copy;
  {
    auto original = CellRendererPopupRef::create ();
    weak = original.get (); g_object_add_weak_pointer (G_OBJECT (original.get ()), &weak);
    copy.reset (new CellRendererPopupRef (original));
    auto moved = std::move (*copy);
    g_assert_true (moved.get () == original.get ());
    g_assert_null (copy->get ());
    *copy = moved;
  }
  g_assert_nonnull (weak); copy.reset (); g_assert_null (weak);
  bool rejected = false;
  try { CellRendererPopupRef::retain (nullptr); }
  catch (const Error& error) { rejected = error.code () == GIMP_PAINTER_ERROR_WRONG_TYPE; }
  g_assert_true (rejected);
}

struct Delivery {
  GObject *tree;
  const gchar *path;
  GdkRectangle *area;
  unsigned calls = 0;
};
static void delivered (GimpCellRendererPopup *, GObject *tree, const gchar *path,
                       GdkRectangle *area, gpointer data) {
  auto *delivery = static_cast<Delivery *> (data);
  g_assert_true (tree == delivery->tree);
  g_assert_cmpstr (path, ==, delivery->path);
  g_assert_true (area == delivery->area);
  ++delivery->calls;
}
static void named_active_unbound_arguments () {
  auto handle = CellRendererPopupRef::create ();
  GtkWidget *tree = gtk_tree_view_new (); g_object_ref_sink (tree);
  GdkRectangle area {3, 5, 101, 27};
  Delivery delivery {G_OBJECT (tree), "999999", &area};
  auto *store = BindingStore::find (G_OBJECT (handle.get ()));
  const auto generation = store->generation ();
  const auto popup_generation = gimp_cell_renderer_popup_get_generation (handle.get ());
  g_signal_connect (handle.get (), "parent-clicked", G_CALLBACK (delivered), &delivery);
  g_signal_connect_after (handle.get (), "parent-clicked", G_CALLBACK (delivered), &delivery);
  gboolean active = TRUE;
  g_object_get (handle.get (), "active", &active, nullptr); g_assert_false (active);
  for (unsigned value = 0; value < 2; ++value) {
    g_object_set (handle.get (), "active", static_cast<gboolean> (value), nullptr);
    for (bool null_area : {false, true}) {
      delivery.area = null_area ? nullptr : &area;
      handle.clicked (G_OBJECT (tree), delivery.path, delivery.area);
    }
  }
  g_assert_cmpuint (delivery.calls, ==, 8);
  g_assert_null (gimp_cell_renderer_popup_get_popup (handle.get ()));
  g_assert_true (BindingStore::find (G_OBJECT (handle.get ())) == store);
  g_assert_true (store->state () == BindingStore::State::active);
  g_assert_cmpuint (store->generation (), ==, generation);
  g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (handle.get ()), ==, popup_generation);
  g_assert_null (BindingStore::find (G_OBJECT (tree)));
  gtk_widget_destroy (tree); g_object_unref (tree);
}

struct Reentry {
  CellRendererPopupRef *handle;
  Counts *counts;
  bool close, nested = false;
  std::string trace;
};
static void named_reenter (GimpCellRendererPopup *cell, GObject *tree, const gchar *,
                           GdkRectangle *area, gpointer data) {
  auto *state = static_cast<Reentry *> (data);
  state->trace += state->nested ? 'I' : 'O';
  if (state->nested) return;
  g_assert_cmpuint (state->counts->closure.load (), ==, 1);
  state->nested = true;
  if (state->close) BindingStore::require (G_OBJECT (cell)).close ();
  state->handle->clicked (tree, "inner", area);
  state->nested = false;
  state->trace += 'R';
}
static void named_reenter_after (GimpCellRendererPopup *, GObject *, const gchar *,
                                 GdkRectangle *, gpointer data) {
  auto *state = static_cast<Reentry *> (data);
  state->trace += state->nested ? 'i' : 'o';
}
static void named_nested_or_close (bool close) {
  Fixture fixture;
  auto handle = CellRendererPopupRef::retain (fixture.cell);
  Reentry state {&handle, &fixture.counts, close};
  g_signal_connect (fixture.cell, "parent-clicked", G_CALLBACK (named_reenter), &state);
  g_signal_connect_after (fixture.cell, "parent-clicked", G_CALLBACK (named_reenter_after), &state);
  handle.clicked (G_OBJECT (fixture.tree), "outer", nullptr);
  g_assert_cmpstr (state.trace.c_str (), ==, close ? "ORo" : "OIiRo");
  assert_counts (fixture.counts, close ? 1 : 2);
}

struct DropHandle {
  std::unique_ptr<CellRendererPopupRef> handle;
  gpointer cell_weak, tree_weak;
  unsigned calls = 0;
};
static void drop_handle (GimpCellRendererPopup *, GObject *tree, const gchar *,
                         GdkRectangle *, gpointer data) {
  auto *drop = static_cast<DropHandle *> (data);
  drop->handle.reset (); g_object_unref (tree);
  g_assert_nonnull (drop->cell_weak); g_assert_nonnull (drop->tree_weak);
  ++drop->calls;
}
static void after_drop_handle (GimpCellRendererPopup *cell, GObject *tree, const gchar *,
                               GdkRectangle *, gpointer data) {
  auto *drop = static_cast<DropHandle *> (data);
  g_assert_true (drop->cell_weak == cell); g_assert_true (drop->tree_weak == tree);
  ++drop->calls;
}
static void named_last_reference_leases () {
  DropHandle drop;
  drop.handle.reset (new CellRendererPopupRef (CellRendererPopupRef::create ()));
  GtkWidget *tree = gtk_tree_view_new (); g_object_ref_sink (tree);
  auto *cell = drop.handle->get ();
  drop.cell_weak = cell; drop.tree_weak = tree;
  g_object_add_weak_pointer (G_OBJECT (cell), &drop.cell_weak);
  g_object_add_weak_pointer (G_OBJECT (tree), &drop.tree_weak);
  g_signal_connect (cell, "parent-clicked", G_CALLBACK (drop_handle), &drop);
  g_signal_connect_after (cell, "parent-clicked", G_CALLBACK (after_drop_handle), &drop);
  drop.handle->clicked (G_OBJECT (tree), "0", nullptr);
  g_assert_cmpuint (drop.calls, ==, 2);
  g_assert_null (drop.cell_weak); g_assert_null (drop.tree_weak);
}

static GtkWidget *find_widget (GtkWidget *root, const gchar *name) {
  if (g_strcmp0 (gtk_widget_get_name (root), name) == 0) return root;
  if (!GTK_IS_CONTAINER (root)) return nullptr;
  GList *children = gtk_container_get_children (GTK_CONTAINER (root));
  GtkWidget *found = nullptr;
  for (GList *it = children; it && !found; it = it->next)
    found = find_widget (GTK_WIDGET (it->data), name);
  g_list_free (children); return found;
}
struct NativeTree {
  GimpImage *image;
  GimpLayer *a, *b;
  GimpContext *context;
  GtkWidget *window, *tree;
  GimpContainerTreeView *view;
  NativeTree () {
    gimp_set_focused_once (gimp);
    image = gimp_image_new (gimp, 16, 16, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
    a = gimp_layer_new (image, 16, 16, gimp_image_get_layer_format (image, TRUE),
                       "Named clicked A", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
    b = gimp_layer_new (image, 16, 16, gimp_image_get_layer_format (image, TRUE),
                       "Named selected B", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
    g_assert_true (gimp_image_add_layer (image, a, nullptr, 0, FALSE));
    g_assert_true (gimp_image_add_layer (image, b, nullptr, 0, FALSE));
    context = gimp_context_new (gimp, "popup-handle-test", gimp_get_user_context (gimp));
    gimp_context_set_image (context, image);
    tree = gimp_item_tree_view_new (GIMP_TYPE_LAYER_TREE_VIEW, 32, 1, TRUE,
      image, menus_get_global_menu_factory (gimp), "<Layers>", "/layers-popup");
    g_object_ref_sink (tree); view = GIMP_CONTAINER_TREE_VIEW (tree);
    gimp_container_view_set_context (GIMP_CONTAINER_VIEW (tree), context);
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
    gtk_window_set_default_size (GTK_WINDOW (window), 400, 500);
    gtk_container_add (GTK_CONTAINER (window), tree);
    gtk_widget_show_all (window); gimp_test_run_mainloop_until_idle ();
  }
  ~NativeTree () {
    gtk_widget_destroy (window); g_object_unref (window); g_object_unref (tree);
    g_object_unref (context); g_object_unref (image); gimp_test_run_mainloop_until_idle ();
  }
  GimpCellRendererPopup *cell () const {
    auto *column = gtk_tree_view_get_column (view->view, 0);
    GList *cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
    GimpCellRendererPopup *result = nullptr;
    for (GList *it = cells; it; it = it->next)
      if (GIMP_IS_CELL_RENDERER_POPUP (it->data)) result = GIMP_CELL_RENDERER_POPUP (it->data);
    g_list_free (cells); g_assert_nonnull (result); return result;
  }
  gchar *path (GimpLayer *layer) const {
    GtkTreeIter iter; gboolean valid = gtk_tree_model_get_iter_first (view->model, &iter);
    while (valid) {
      auto *renderer = gimp_container_tree_store_get_renderer (GIMP_CONTAINER_TREE_STORE (view->model), &iter);
      const bool matches = renderer->viewable == GIMP_VIEWABLE (layer); g_object_unref (renderer);
      if (matches) {
        auto *path = gtk_tree_model_get_path (view->model, &iter);
        auto *text = gtk_tree_path_to_string (path); gtk_tree_path_free (path); return text;
      }
      valid = gtk_tree_model_iter_next (view->model, &iter);
    }
    g_assert_not_reached (); return nullptr;
  }
};
struct ReorderTarget { NativeTree *fixture; unsigned calls = 0; };
static void reorder_target (GimpCellRendererPopup *cell, GObject *, const gchar *,
                            GdkRectangle *, gpointer data) {
  auto *state = static_cast<ReorderTarget *> (data);
  auto& f = *state->fixture;
  g_assert_nonnull (gimp_cell_renderer_popup_get_popup (cell));
  g_assert_true (gimp_image_reorder_item (f.image, GIMP_ITEM (f.a), nullptr, 0, FALSE, nullptr));
  gimp_container_view_set_1_selected (GIMP_CONTAINER_VIEW (f.tree), GIMP_VIEWABLE (f.b));
  ++state->calls;
}
static void named_bound_controls_and_teardown () {
  NativeTree fixture;
  auto handle = CellRendererPopupRef::retain (fixture.cell ());
  auto native_tree = ObjectRef<GObject>::retain (G_OBJECT (fixture.view->view));
  ReorderTarget reorder {&fixture};
  g_signal_connect (handle.get (), "parent-clicked", G_CALLBACK (reorder_target), &reorder);
  for (guint active = 0; active < 2; ++active) {
    g_object_set (handle.get (), "active", static_cast<gboolean> (active), nullptr);
    gchar *path = fixture.path (fixture.a);
    handle.clicked (native_tree.get (), path, nullptr); g_free (path);
    auto *popup = gimp_cell_renderer_popup_get_popup (handle.get ()); g_assert_nonnull (popup);
    auto *opacity = find_widget (popup, "painter-tree-layer-opacity"); g_assert_nonnull (opacity);
    auto *mode = find_widget (popup, "painter-tree-layer-mode"); g_assert_nonnull (mode);
    gtk_range_set_value (GTK_RANGE (opacity), 34 + 20 * active);
    gimp_layer_mode_box_set_mode (GIMP_LAYER_MODE_BOX (mode), GIMP_LAYER_MODE_MULTIPLY_LEGACY);
    g_assert_cmpfloat_with_epsilon (gimp_layer_get_opacity (fixture.a), .34 + .20 * active, .000001);
    g_assert_cmpint (gimp_layer_get_mode (fixture.a), ==, GIMP_LAYER_MODE_MULTIPLY_LEGACY);
    g_assert_cmpfloat (gimp_layer_get_opacity (fixture.b), ==, 1);
    g_assert_cmpint (gimp_layer_get_mode (fixture.b), ==, GIMP_LAYER_MODE_NORMAL_LEGACY);
  }
  g_assert_cmpuint (reorder.calls, ==, 2);
  g_signal_handlers_disconnect_by_data (handle.get (), &reorder);
  Delivery delivery {native_tree.get (), "999999", nullptr};
  g_signal_connect (handle.get (), "parent-clicked", G_CALLBACK (delivered), &delivery);
  auto *popup = gimp_cell_renderer_popup_get_popup (handle.get ());
  const auto generation = gimp_cell_renderer_popup_get_generation (handle.get ());
  handle.clicked (native_tree.get (), delivery.path, nullptr);
  g_assert_cmpuint (delivery.calls, ==, 1);
  g_assert_true (gimp_cell_renderer_popup_get_popup (handle.get ()) == popup);
  g_assert_cmpuint (gimp_cell_renderer_popup_get_generation (handle.get ()), ==, generation);
  gimp_item_tree_view_set_image (GIMP_ITEM_TREE_VIEW (fixture.tree), nullptr);
  g_assert_null (gimp_cell_renderer_popup_get_popup (handle.get ()));
  gtk_widget_destroy (fixture.tree);
  g_assert_true (BindingStore::require (G_OBJECT (handle.get ())).state () == BindingStore::State::closed);
  handle.clicked (native_tree.get (), delivery.path, nullptr);
  g_assert_cmpuint (delivery.calls, ==, 1);
}
/* The popup delegates pixels, measurement and drawing to GtkCellRendererPixbuf.
 * These tests exercise that owner with actual parent rendering; closing the
 * application's PopupSlot is deliberately not the pixel-finalization oracle. */
static void count_finalized (gpointer data, GObject *) {
  ++*static_cast<unsigned *> (data);
}
static GdkPixbuf *counted_pixbuf (unsigned& finalized, guint32 color) {
  auto *pixels = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 9, 7);
  g_assert_nonnull (pixels);
  gdk_pixbuf_fill (pixels, color);
  g_object_weak_ref (G_OBJECT (pixels), count_finalized, &finalized);
  return pixels;
}
static void assert_pixbuf (GimpCellRendererPopup *cell, GdkPixbuf *expected) {
  GdkPixbuf *actual = nullptr;
  g_object_get (cell, "pixbuf", &actual, nullptr);
  g_assert_true (actual == expected);
  /* The getter transfers a reference, including for the same-value test. */
  g_clear_object (&actual);
}
struct PixelDraw {
  GtkWidget *window, *widget;
  PixelDraw () {
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL); g_object_ref_sink (window);
    widget = gtk_tree_view_new ();
    gtk_container_add (GTK_CONTAINER (window), widget);
    gtk_widget_show_all (window); gimp_test_run_mainloop_until_idle ();
    g_assert_true (gtk_widget_get_realized (widget));
  }
  ~PixelDraw () {
    gtk_widget_destroy (window); g_object_unref (window);
    gimp_test_run_mainloop_until_idle ();
  }
  void check (GimpCellRendererPopup *popup, bool empty) {
    auto *cell = GTK_CELL_RENDERER (popup);
    for (auto direction : {GTK_TEXT_DIR_LTR, GTK_TEXT_DIR_RTL}) {
      gtk_widget_set_direction (widget, direction);
      for (bool sensitive : {true, false}) {
        gtk_widget_set_sensitive (widget, sensitive);
        g_object_set (cell, "sensitive", sensitive, nullptr);
        GtkRequisition minimum {}, natural {};
        gtk_cell_renderer_get_preferred_size (cell, widget, &minimum, &natural);
        g_assert_cmpint (minimum.width, >=, 0);
        g_assert_cmpint (minimum.height, >=, 0);
        g_assert_cmpint (natural.width, >=, minimum.width);
        g_assert_cmpint (natural.height, >=, minimum.height);
        if (empty) {
          g_assert_cmpint (natural.width, ==, 0);
          g_assert_cmpint (natural.height, ==, 0);
        }
        auto *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 64, 64);
        auto *cr = cairo_create (surface);
        cairo_set_source_rgb (cr, .125, .25, .375); cairo_paint (cr);
        cairo_surface_flush (surface);
        const auto bytes = cairo_image_surface_get_stride (surface) * 64;
        auto *data = cairo_image_surface_get_data (surface);
        std::vector<unsigned char> before (data, data + bytes);
        GdkRectangle area {0, 0, 64, 64};
        gtk_cell_renderer_render (cell, cr, widget, &area, &area,
          sensitive ? static_cast<GtkCellRendererState> (0) : GTK_CELL_RENDERER_INSENSITIVE);
        g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
        cairo_surface_flush (surface);
        g_assert_cmpint (cairo_surface_status (surface), ==, CAIRO_STATUS_SUCCESS);
        const bool unchanged = std::equal (before.begin (), before.end (), data);
        if (empty) g_assert_true (unchanged);
        else g_assert_false (unchanged);
        cairo_destroy (cr); cairo_surface_destroy (surface);
      }
    }
    g_object_set (cell, "sensitive", TRUE, nullptr);
  }
};
static void pixbuf_initial_and_empty () {
  unsigned finalized = 0;
  {
    auto cell = CellRendererPopupRef::create ();
    g_object_weak_ref (G_OBJECT (cell.get ()), count_finalized, &finalized);
    assert_pixbuf (cell.get (), nullptr);
    gchar *icon = nullptr; g_object_get (cell.get (), "icon-name", &icon, nullptr);
    g_assert_cmpstr (icon, ==, GIMP_ICON_MENU_RIGHT); g_free (icon);
    auto *klass = GTK_CELL_RENDERER_GET_CLASS (cell.get ());
    auto *parent = static_cast<GtkCellRendererClass *> (g_type_class_peek (GTK_TYPE_CELL_RENDERER_PIXBUF));
    g_assert_true (klass->render == parent->render);
    g_assert_true (klass->get_size == parent->get_size);
    g_assert_true (klass->get_preferred_width == parent->get_preferred_width);
    g_assert_true (klass->get_preferred_height == parent->get_preferred_height);
    /* No measurement or draw before this fresh renderer's destruction. */
  }
  g_assert_cmpuint (finalized, ==, 1);
  PixelDraw draw;
  auto cell = CellRendererPopupRef::create ();
  assert_pixbuf (cell.get (), nullptr);
  draw.check (cell.get (), false); // Initial named icon, not an empty image.
  for (unsigned i = 0; i != 2; ++i) {
    g_object_set (cell.get (), "pixbuf", nullptr, nullptr);
    assert_pixbuf (cell.get (), nullptr);
    draw.check (cell.get (), true);
  }
}
static void pixbuf_replace_same_reset () {
  PixelDraw draw;
  unsigned a_final = 0, b_final = 0, cell_final = 0;
  {
    auto cell = CellRendererPopupRef::create ();
    g_object_weak_ref (G_OBJECT (cell.get ()), count_finalized, &cell_final);
    auto *a = counted_pixbuf (a_final, 0xff3300ff);
    g_object_set (cell.get (), "pixbuf", a, nullptr); g_object_unref (a);
    assert_pixbuf (cell.get (), a); draw.check (cell.get (), false);
    g_assert_cmpuint (a_final, ==, 0);
    /* Borrowed same-value assignment while the renderer is the sole owner. */
    g_object_set (cell.get (), "pixbuf", a, nullptr);
    g_assert_cmpuint (a_final, ==, 0); assert_pixbuf (cell.get (), a);
    GdkPixbuf *held = nullptr; g_object_get (cell.get (), "pixbuf", &held, nullptr);
    g_object_set (cell.get (), "pixbuf", held, nullptr); g_object_unref (held);
    g_assert_cmpuint (a_final, ==, 0); draw.check (cell.get (), false);
    auto *b = counted_pixbuf (b_final, 0x22ff44ff);
    g_object_set (cell.get (), "pixbuf", b, nullptr);
    g_assert_cmpuint (a_final, ==, 1);
    g_object_unref (b); assert_pixbuf (cell.get (), b);
    draw.check (cell.get (), false); g_assert_cmpuint (b_final, ==, 0);
    for (unsigned i = 0; i != 2; ++i) {
      g_object_set (cell.get (), "pixbuf", nullptr, nullptr);
      g_assert_cmpuint (b_final, ==, 1); assert_pixbuf (cell.get (), nullptr);
      draw.check (cell.get (), true);
    }
  }
  g_assert_cmpuint (a_final, ==, 1); g_assert_cmpuint (b_final, ==, 1);
  g_assert_cmpuint (cell_final, ==, 1);
}
static void pixbuf_recovery () {
  PixelDraw draw;
  unsigned a_final = 0, b_final = 0, c_final = 0;
  {
    auto cell = CellRendererPopupRef::create ();
    auto *a = counted_pixbuf (a_final, 0xff2200ff);
    g_object_set (cell.get (), "pixbuf", a, nullptr); g_object_unref (a);
    draw.check (cell.get (), false);
    g_object_set (cell.get (), "pixbuf", nullptr, nullptr);
    g_assert_cmpuint (a_final, ==, 1); draw.check (cell.get (), true);
    auto *b = counted_pixbuf (b_final, 0x22ff00ff);
    g_object_set (cell.get (), "pixbuf", b, nullptr); g_object_unref (b);
    draw.check (cell.get (), false);
    /* GTK 3.24 uses image-missing fallback for lookup failure. This is NOT
     * allocator-failure/OOM simulation and does not claim such resilience. */
    const gchar *missing = "gimp-painter-pixbuf-owner-nonexistent-08-016-9fcecab9";
    g_assert_false (gtk_icon_theme_has_icon (gtk_icon_theme_get_for_screen (
      gtk_widget_get_screen (draw.widget)), missing));
    g_object_set (cell.get (), "icon-name", missing, nullptr);
    g_assert_cmpuint (b_final, ==, 1); assert_pixbuf (cell.get (), nullptr);
    draw.check (cell.get (), false);
    g_object_set (cell.get (), "icon-name", GIMP_ICON_MENU_RIGHT, nullptr);
    draw.check (cell.get (), false);
    g_object_set (cell.get (), "pixbuf", nullptr, nullptr); draw.check (cell.get (), true);
    auto *c = counted_pixbuf (c_final, 0x2255ffff);
    g_object_set (cell.get (), "pixbuf", c, nullptr); g_object_unref (c);
    draw.check (cell.get (), false); g_assert_cmpuint (c_final, ==, 0);
  }
  g_assert_cmpuint (a_final, ==, 1); g_assert_cmpuint (b_final, ==, 1);
  g_assert_cmpuint (c_final, ==, 1);
}
static void pixbuf_column_finalization (bool column_first) {
  PixelDraw draw;
  unsigned pixels_final = 0, cell_final = 0;
  auto handle = std::make_unique<CellRendererPopupRef> (CellRendererPopupRef::create ());
  auto *cell = handle->get ();
  g_object_weak_ref (G_OBJECT (cell), count_finalized, &cell_final);
  auto *column = gtk_tree_view_column_new (); g_object_ref_sink (column);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), GTK_CELL_RENDERER (cell), FALSE);
  auto *pixels = counted_pixbuf (pixels_final, 0xff55aaff);
  g_object_set (cell, "pixbuf", pixels, nullptr); g_object_unref (pixels);
  draw.check (cell, false);
  if (column_first) g_object_unref (column);
  else handle.reset ();
  g_assert_cmpuint (pixels_final, ==, 0); g_assert_cmpuint (cell_final, ==, 0);
  assert_pixbuf (cell, pixels); draw.check (cell, false);
  if (column_first) handle.reset ();
  else g_object_unref (column);
  g_assert_cmpuint (pixels_final, ==, 1); g_assert_cmpuint (cell_final, ==, 1);
}
static void pixbuf_retained_after_tree_close () {
  PixelDraw draw;
  unsigned pixels_final = 0, cell_final = 0;
  std::unique_ptr<CellRendererPopupRef> handle;
  {
    NativeTree tree;
    handle = std::make_unique<CellRendererPopupRef> (CellRendererPopupRef::retain (tree.cell ()));
    auto *cell = handle->get ();
    g_object_weak_ref (G_OBJECT (cell), count_finalized, &cell_final);
    auto *pixels = counted_pixbuf (pixels_final, 0xff55aaff);
    g_object_set (cell, "pixbuf", pixels, nullptr); g_object_unref (pixels);
    gchar *path = tree.path (tree.a);
    handle->clicked (G_OBJECT (tree.view->view), path, nullptr); g_free (path);
    g_assert_nonnull (gimp_cell_renderer_popup_get_popup (cell));
    gtk_widget_destroy (tree.tree);
    g_assert_true (BindingStore::require (G_OBJECT (cell)).state () == BindingStore::State::closed);
    g_assert_null (gimp_cell_renderer_popup_get_popup (cell));
    g_assert_cmpuint (pixels_final, ==, 0); assert_pixbuf (cell, pixels);
  }
  g_assert_cmpuint (pixels_final, ==, 0); g_assert_cmpuint (cell_final, ==, 0);
  draw.check (handle->get (), false);
  handle.reset ();
  g_assert_cmpuint (pixels_final, ==, 1); g_assert_cmpuint (cell_final, ==, 1);
}

int main (int argc, char **argv) {
  g_test_init (&argc, &argv, nullptr);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/painter-popup-handles/admission/closed-entry", closed_entry);
  g_test_add_func ("/painter-popup-handles/admission/wrong-thread-entry", wrong_thread_entry);
  g_test_add_func ("/painter-popup-handles/admission/invalid-arguments", invalid_arguments);
  g_test_add_func ("/painter-popup-handles/admission/named-wrong-thread-entry", [] { wrong_thread_entry_impl (true); });
  g_test_add_func ("/painter-popup-handles/admission/constructing-c", [] { phase_rejection (false, false); });
  g_test_add_func ("/painter-popup-handles/admission/constructing-named", [] { phase_rejection (true, false); });
  g_test_add_func ("/painter-popup-handles/admission/closing-c", [] { phase_rejection (false, true); });
  g_test_add_func ("/painter-popup-handles/admission/closing-named", [] { phase_rejection (true, true); });
  g_test_add_func ("/painter-popup-handles/admission/named-closed", [] { retained_closed_or_disposed (false); });
  g_test_add_func ("/painter-popup-handles/admission/named-disposed", [] { retained_closed_or_disposed (true); });
  g_test_add_func ("/painter-popup-handles/admission/renderer-retain-closes", [] { final_admission_after_retain (false); });
  g_test_add_func ("/painter-popup-handles/admission/tree-retain-closes", [] { final_admission_after_retain (true); });
  g_test_add_func ("/painter-popup-handles/ownership/create-column-first", [] { named_type_and_ownership (false, true); });
  g_test_add_func ("/painter-popup-handles/ownership/create-handle-first", [] { named_type_and_ownership (false, false); });
  g_test_add_func ("/painter-popup-handles/ownership/retain-column-first", [] { named_type_and_ownership (true, true); });
  g_test_add_func ("/painter-popup-handles/ownership/retain-handle-first", [] { named_type_and_ownership (true, false); });
  g_test_add_func ("/painter-popup-handles/ownership/copy-move", named_copy_move_ownership);
  g_test_add_func ("/painter-popup-handles/named/active-unbound-arguments", named_active_unbound_arguments);
  g_test_add_func ("/painter-popup-handles/named/nested-emission", [] { named_nested_or_close (false); });
  g_test_add_func ("/painter-popup-handles/named/close-during-emission", [] { named_nested_or_close (true); });
  g_test_add_func ("/painter-popup-handles/named/last-reference-leases", named_last_reference_leases);
  g_test_add_func ("/painter-popup-handles/named/bound-controls-and-teardown", named_bound_controls_and_teardown);
  g_test_add_func ("/painter-popup-handles/pixbuf/initial-and-empty", pixbuf_initial_and_empty);
  g_test_add_func ("/painter-popup-handles/pixbuf/replace-same-reset", pixbuf_replace_same_reset);
  g_test_add_func ("/painter-popup-handles/pixbuf/recovery", pixbuf_recovery);
  g_test_add_func ("/painter-popup-handles/pixbuf/column-first", [] { pixbuf_column_finalization (true); });
  g_test_add_func ("/painter-popup-handles/pixbuf/handle-first", [] { pixbuf_column_finalization (false); });
  g_test_add_func ("/painter-popup-handles/pixbuf/retained-after-tree-close", pixbuf_retained_after_tree_close);
  g_application_run (gimp->app, 0, nullptr);
  const gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app);
  return result;
}
