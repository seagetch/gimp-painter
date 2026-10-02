/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent FilterLayer. The GEGL source is only the last committed buffer;
 * neither graph evaluation nor the pickable path starts a job or waits for one.
 */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "core-types.h"
#include "gimpfilterlayer.h"
#include "gimpcontainer.h"
#include "gimpfilterstack.h"
#include "gimpimage.h"
#include "gimpimage-undo.h"
#include "gimpitemundo.h"
#include "gimppickable.h"
#include "gimpprojectable.h"
}
#include "gimp-painter-type-traits.hpp"
#include "gimpfilterlayer-arguments.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/filter-scheduler.hpp"
#include "painter/fair-dispatcher.hpp"
#include "painter/filter-edge.hpp"
#include "painter/filter-gauss.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter/source.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
using namespace GimpPainter;
static void gimp_filter_layer_pickable_init (GimpPickableInterface *iface);
G_DEFINE_TYPE_WITH_CODE (GimpFilterLayer, gimp_filter_layer, GIMP_TYPE_LAYER,
                        G_IMPLEMENT_INTERFACE (GIMP_TYPE_PICKABLE, gimp_filter_layer_pickable_init))
struct GimpFilterLayerUndo { GimpItemUndo parent_instance; gboolean binding_failed; };
struct GimpFilterLayerUndoClass { GimpItemUndoClass parent_class; };
static GType gimp_filter_layer_undo_get_type ();
G_DEFINE_TYPE (GimpFilterLayerUndo, gimp_filter_layer_undo, GIMP_TYPE_ITEM_UNDO)
namespace GimpPainter {
template<> struct TypeTraits<GimpFilterLayerUndo>
{ static GType type () noexcept { return gimp_filter_layer_undo_get_type (); } };
}
namespace {
struct FilterImpl;
struct FilterSlot : SlotSpec<GimpFilterLayer, FilterImpl> {};
struct BytesFree { void operator() (GBytes *p) const noexcept { if (p) g_bytes_unref (p); } };
struct ArgsFree { void operator() (GimpValueArray *p) const noexcept { if (p) gimp_value_array_unref (p); } };
using BytesRef = std::unique_ptr<GBytes, BytesFree>;
using ArgsRef = std::unique_ptr<GimpValueArray, ArgsFree>;
struct Callback
{
  explicit Callback (GimpFilterLayer *layer)
    : owner (ObjectRef<GObject>::retain (G_OBJECT (layer))),
      generation (BindingStore::require (G_OBJECT (layer)).generation ()) {}
  WeakRef<GObject> owner;
  std::uint64_t generation;
};
void free_callback (gpointer p, GClosure *) { delete static_cast<Callback *> (p); }
template<class F> void visit (gpointer data, F function) noexcept
{
  boundary_void (nullptr, [&] {
    auto *c = static_cast<Callback *> (data);
    auto owner = c->owner.lock ();
    if (!owner) return;
    auto *store = BindingStore::find (owner.get ());
    if (store && store->accepts (c->generation)) store->with<FilterSlot> (function);
  });
}
void topology_changed (GimpContainer *, GimpObject *, gpointer);
void reordered (GimpContainer *, GimpObject *, gint, gint, gpointer);
void dependency_update (GimpDrawable *, gint, gint, gint, gint, gpointer);
void dependency_active (GimpFilter *, gpointer);
void dependency_status (GimpFilterLayer *, gpointer);
void owner_ancestry (GimpViewable *, gpointer);
void owner_removed (GimpItem *, gpointer);
void owner_size (GimpViewable *, gpointer);
void owner_offset (GObject *, GParamSpec *, gpointer);
void owner_active (GimpFilter *, gpointer);
void owner_image_changed (GObject *, GParamSpec *, gpointer);
void image_disconnected (GimpObject *, gpointer);

struct FilterImpl
{
  explicit FilterImpl (GimpFilterLayer *owner) : owner (owner) {}
  ~FilterImpl () noexcept = default;
  void close () noexcept
  {
    scheduler.close ();
    pending.close ();
    dependencies.clear ();
    own_connections.clear ();
    image_connection.close ();
    staged.reset ();
    stack.reset ();
    lower_objects.clear ();
  }
  Connection connect (GObject *object, const char *signal, GCallback function)
  {
    std::unique_ptr<Callback> c (new Callback (owner));
    auto result = Connection::connect (ObjectRef<GObject>::retain (object), signal,
                                       function, c.get (), free_callback);
    c.release ();
    return result;
  }
  void attach ()
  {
    if (!own_connections.empty ()) return;
    own_connections.push_back (connect (G_OBJECT (owner), "ancestry-changed", G_CALLBACK (owner_ancestry)));
    own_connections.push_back (connect (G_OBJECT (owner), "removed", G_CALLBACK (owner_removed)));
    own_connections.push_back (connect (G_OBJECT (owner), "size-changed", G_CALLBACK (owner_size)));
    own_connections.push_back (connect (G_OBJECT (owner), "notify::offset-x", G_CALLBACK (owner_offset)));
    own_connections.push_back (connect (G_OBJECT (owner), "notify::offset-y", G_CALLBACK (owner_offset)));
    own_connections.push_back (connect (G_OBJECT (owner), "active-changed", G_CALLBACK (owner_active)));
    own_connections.push_back (connect (G_OBJECT (owner), "notify::image", G_CALLBACK (owner_image_changed)));
    watch_image ();
    refresh_connections ();
  }
  void watch_image ()
  {
    image_connection.close ();
    if (GimpImage *image = gimp_item_get_image (GIMP_ITEM (owner)))
      image_connection = connect (G_OBJECT (image), "disconnect", G_CALLBACK (image_disconnected));
  }
  GimpContainer *current_stack ()
  {
    GimpContainer *result = gimp_item_get_container (GIMP_ITEM (owner));
    if (!result)
      {
        GimpImage *image = gimp_item_get_image (GIMP_ITEM (owner));
        if (image) result = gimp_image_get_layers (image);
      }
    return result;
  }
  void watch_descendant_status (GimpObject *object)
  {
    if (GIMP_IS_FILTER_LAYER (object))
      dependencies.push_back (connect (G_OBJECT (object), "filter-state-changed", G_CALLBACK (dependency_status)));
    if (GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (object)))
      {
        dependencies.push_back (connect (G_OBJECT (children), "add", G_CALLBACK (topology_changed)));
        dependencies.push_back (connect (G_OBJECT (children), "remove", G_CALLBACK (topology_changed)));
        dependencies.push_back (connect (G_OBJECT (children), "reorder", G_CALLBACK (reordered)));
        for (int i = 0; i < gimp_container_get_n_children (children); ++i)
          watch_descendant_status (gimp_container_get_child_by_index (children, i));
      }
  }
  void refresh_connections ()
  {
    dependencies.clear ();
    GimpContainer *container = current_stack ();
    stack = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (container)));
    topology_dirty = false;
    std::vector<WeakRef<GObject>> current_lower;
    bool different = false;
    if (container)
      {
        const auto self = gimp_container_get_child_index (container, GIMP_OBJECT (owner));
        if (self >= 0)
          for (int i = self + 1; i < gimp_container_get_n_children (container); ++i)
            {
              auto *object = G_OBJECT (gimp_container_get_child_by_index (container, i));
              if (current_lower.size () >= lower_objects.size () ||
                  lower_objects[current_lower.size ()].lock ().get () != object) different = true;
              current_lower.emplace_back (ObjectRef<GObject>::retain (object));
            }
      }
    if (current_lower.size () != lower_objects.size ()) different = true;
    lower_objects = std::move (current_lower);
    if (different) invalidate ();
    if (!container) return;
    dependencies.push_back (connect (G_OBJECT (container), "add", G_CALLBACK (topology_changed)));
    dependencies.push_back (connect (G_OBJECT (container), "remove", G_CALLBACK (topology_changed)));
    dependencies.push_back (connect (G_OBJECT (container), "reorder", G_CALLBACK (reordered)));
    const auto n = gimp_container_get_n_children (container);
    for (int i = 0; i < n; ++i)
      {
        GimpObject *child = gimp_container_get_child_by_index (container, i);
        if (child == GIMP_OBJECT (owner)) continue;
        dependencies.push_back (connect (G_OBJECT (child), "update", G_CALLBACK (dependency_update)));
        dependencies.push_back (connect (G_OBJECT (child), "active-changed", G_CALLBACK (dependency_active)));
        watch_descendant_status (child);
      }
  }
  bool below (GimpItem *item)
  {
    auto container = stack.lock ();
    if (!container || item == GIMP_ITEM (owner)) return false;
    while (gimp_item_get_parent (item) && gimp_item_get_container (item) != GIMP_CONTAINER (container.get ()))
      item = gimp_item_get_parent (item);
    const auto self = gimp_container_get_child_index (GIMP_CONTAINER (container.get ()), GIMP_OBJECT (owner));
    const auto index = gimp_container_get_child_index (GIMP_CONTAINER (container.get ()), GIMP_OBJECT (item));
    return self >= 0 && index > self;
  }
  void invalidate ()
  {
    scheduler.invalidate ();
    staged.reset ();
    schedule ();
    g_signal_emit_by_name (owner, "filter-state-changed");
  }
  void structure_changed ()
  { refresh_connections (); schedule (); }
  void configure ()
  {
    FilterScheduler::Request request;
    request.width = gimp_item_get_width (GIMP_ITEM (owner));
    request.height = gimp_item_get_height (GIMP_ITEM (owner));
    if (procedure == "plug-in-edge" && args &&
        (args->size () == 5 || args->size () == 6))
      {
        const GValue *amount = args->at (3);
        const GValue *wrap = args->at (4);
        const GValue *edge = args->size () == 6 ? args->at (5) : nullptr;
        /* The original first three slots remain saved verbatim. Execution uses
         * our owned snapshot, never stale image/drawable IDs from the file. */
        if ((G_VALUE_HOLDS_DOUBLE (amount) || G_VALUE_HOLDS_FLOAT (amount)) &&
            G_VALUE_HOLDS_INT (wrap) && (!edge || G_VALUE_HOLDS_INT (edge)))
          {
            EdgeOptions options { G_VALUE_HOLDS_DOUBLE (amount) ? g_value_get_double (amount) : g_value_get_float (amount),
                                  g_value_get_int (wrap), edge ? g_value_get_int (edge) : 0 };
            if (std::isfinite (options.amount) && options.wrapmode >= 1 && options.wrapmode <= 3 &&
                options.edgemode >= 0 && options.edgemode <= 5)
              {
                auto width = request.width, height = request.height;
                request.process = [width, height, options] (const FilterScheduler::Bytes& input,
                                                            std::atomic<bool>& cancel,
                                                            FilterScheduler::Bytes& output) {
                  return filter_edge (input, width, height, options, cancel, output);
                };
              }
          }
      }
    else if (procedure == "plug-in-gauss" && args && args->size () == 6)
      {
        const GValue *horizontal = args->at (3), *vertical = args->at (4), *method = args->at (5);
        if ((G_VALUE_HOLDS_DOUBLE (horizontal) || G_VALUE_HOLDS_FLOAT (horizontal)) &&
            (G_VALUE_HOLDS_DOUBLE (vertical) || G_VALUE_HOLDS_FLOAT (vertical)) && G_VALUE_HOLDS_INT (method))
          {
            GaussOptions options {
              G_VALUE_HOLDS_DOUBLE (horizontal) ? g_value_get_double (horizontal) : g_value_get_float (horizontal),
              G_VALUE_HOLDS_DOUBLE (vertical) ? g_value_get_double (vertical) : g_value_get_float (vertical),
              g_value_get_int (method)
            };
            if (std::isfinite (options.horizontal) && std::isfinite (options.vertical) &&
                (options.horizontal > 0 || options.vertical > 0) && options.method >= 0 && options.method <= 1)
              {
                auto width = request.width, height = request.height;
                request.process = [width, height, options] (const FilterScheduler::Bytes& input,
                                                            std::atomic<bool>& cancel,
                                                            FilterScheduler::Bytes& output) {
                  return filter_gauss (input, width, height, options, cancel, output);
                };
              }
          }
      }
    scheduler.set_request (std::move (request));
    staged.reset ();
    schedule ();
    g_signal_emit_by_name (owner, "filter-state-changed");
  }
  void schedule ()
  {
    if (scheduler.state () == FilterScheduler::State::closed) return;
    /* A single source rotates one bounded quantum across all layers/images.
     * Multiple ready images cannot form an unbounded same-priority timer batch. */
    static FairDispatcher dispatcher (nullptr, 2, 150);
    if (!pending.valid ())
      {
        auto callback = std::make_shared<Callback> (owner);
        pending = dispatcher.ticket ([callback] {
          bool again = false;
          visit (callback.get (), [&] (FilterImpl& impl) { again = impl.step (); });
          return again;
        });
      }
    pending.schedule ();
  }
  bool dependency_cycle (GimpLayer *layer, std::set<GimpLayer *>& path, std::size_t& visited)
  {
    if (layer == GIMP_LAYER (owner) || !path.insert (layer).second || ++visited > 4096) return true;
    bool cycle = false;
    if (GIMP_IS_CLONE_LAYER (layer))
      {
        GimpLayer *source = gimp_clone_layer_get_source (GIMP_CLONE_LAYER (layer));
        if (source) cycle = dependency_cycle (source, path, visited);
      }
    else if (GIMP_IS_FILTER_LAYER (layer))
      {
        GimpContainer *container = gimp_item_get_container (GIMP_ITEM (layer));
        const int index = container ? gimp_container_get_child_index (container, GIMP_OBJECT (layer)) : -1;
        for (int i = index + 1; container && index >= 0 && i < gimp_container_get_n_children (container); ++i)
          {
            GimpLayer *child = GIMP_LAYER (gimp_container_get_child_by_index (container, i));
            if (gimp_item_get_visible (GIMP_ITEM (child)) && dependency_cycle (child, path, visited)) { cycle = true; break; }
          }
      }
    else if (GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer)))
      {
        for (int i = 0; i < gimp_container_get_n_children (children); ++i)
          {
            GimpLayer *child = GIMP_LAYER (gimp_container_get_child_by_index (children, i));
            if (gimp_item_get_visible (GIMP_ITEM (child)) && dependency_cycle (child, path, visited)) { cycle = true; break; }
          }
      }
    path.erase (layer);
    return cycle;
  }
  bool dependency_ready (GimpLayer *layer, bool& failed)
  {
    if (!gimp_item_get_visible (GIMP_ITEM (layer))) return true;
    if (GIMP_IS_FILTER_LAYER (layer))
      {
        const auto state = gimp_filter_layer_get_state (GIMP_FILTER_LAYER (layer));
        if (state == GIMP_FILTER_LAYER_FAILED || state == GIMP_FILTER_LAYER_CLOSED) failed = true;
        return state == GIMP_FILTER_LAYER_CLEAN;
      }
    if (GimpContainer *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer)))
      {
        for (int i = 0; i < gimp_container_get_n_children (children); ++i)
          if (!dependency_ready (GIMP_LAYER (gimp_container_get_child_by_index (children, i)), failed)) return false;
      }
    return true;
  }
  bool ready (bool& failed)
  {
    auto container = stack.lock ();
    if (!container) return false;
    const auto index = gimp_container_get_child_index (GIMP_CONTAINER (container.get ()), GIMP_OBJECT (owner));
    if (index < 0) return false;
    for (int i = index + 1; i < gimp_container_get_n_children (GIMP_CONTAINER (container.get ())); ++i)
      {
        GimpLayer *child = GIMP_LAYER (gimp_container_get_child_by_index (GIMP_CONTAINER (container.get ()), i));
        std::set<GimpLayer *> path;
        std::size_t visited = 0;
        if (gimp_item_get_visible (GIMP_ITEM (child)) && dependency_cycle (child, path, visited))
          { failed = true; return false; }
        if (!dependency_ready (child, failed)) return false;
      }
    return true;
  }
  GeglRectangle rectangle (std::size_t offset, std::size_t count)
  {
    const int width = gimp_item_get_width (GIMP_ITEM (owner));
    const int x = offset % width, y = offset / width;
    return { x, y, int (count < std::size_t (width) ? count : width),
             int (count < std::size_t (width) ? 1 : count / width) };
  }
  bool step ()
  {
    if (topology_dirty) refresh_connections ();
    if (!gimp_item_is_attached (GIMP_ITEM (owner)) || gimp_item_is_removed (GIMP_ITEM (owner)) ||
        !gimp_item_get_visible (GIMP_ITEM (owner)))
      return scheduler.step (false, {}, {}, {});
    const auto started_at = g_get_monotonic_time ();
    const auto previous = scheduler.state ();
    bool failed = false;
    const bool dependencies_ready = ready (failed);
    if (failed) scheduler.reject ("Filter dependency failed, is cyclic, or exceeds the graph limit");
    bool again = scheduler.step (dependencies_ready,
      [&] (std::size_t offset, std::size_t count, FilterScheduler::Bytes& input) {
        auto container = stack.lock ();
        if (!container) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Layer stack was removed");
        /* Build the existing stack graph outside operator evaluation, then
         * sample only this layer's input proxy: precisely the lower stack. */
        gimp_filter_stack_get_graph (GIMP_FILTER_STACK (container.get ()));
        GeglNode *node = gimp_filter_get_node (GIMP_FILTER (owner));
        GeglNode *below_node = gegl_node_get_input_proxy (node, "input");
        auto rect = rectangle (offset, count);
        rect.x += gimp_item_get_offset_x (GIMP_ITEM (owner));
        rect.y += gimp_item_get_offset_y (GIMP_ITEM (owner));
        const auto size = input.size ();
        input.resize (size + count * 4);
        gegl_node_blit (below_node, 1.0, &rect, babl_format ("R'G'B'A u8"), input.data () + size,
                        GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_DEFAULT);
      },
      [&] (std::size_t offset, std::size_t count, const std::uint8_t *pixels) {
        if (!offset)
          {
            GeglRectangle extent {0, 0, gimp_item_get_width (GIMP_ITEM (owner)), gimp_item_get_height (GIMP_ITEM (owner))};
            staged = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (&extent, gimp_drawable_get_format (GIMP_DRAWABLE (owner)))));
          }
        auto rect = rectangle (offset, count);
        gegl_buffer_set (GEGL_BUFFER (staged.get ()), &rect, 0, babl_format ("R'G'B'A u8"), pixels, GEGL_AUTO_ROWSTRIDE);
      },
      [&] (std::uint64_t token) {
        if (token != scheduler.generation ()) return;
        auto completed = std::move (staged);
        /* Swap a fully imported buffer. No partial result is visible to the
         * drawable source, save code, projection, or an upper FilterLayer. */
        gimp_drawable_set_buffer_full (GIMP_DRAWABLE (owner), FALSE, nullptr, GEGL_BUFFER (completed.get ()), nullptr, FALSE);
        /* Buffer notification may synchronously close the image or edit the
         * definition. Never publish a later drawable update for that token. */
        if (scheduler.state () == FilterScheduler::State::closed || token != scheduler.generation ()) return;
        gimp_drawable_update (GIMP_DRAWABLE (owner), 0, 0,
                              gimp_item_get_width (GIMP_ITEM (owner)), gimp_item_get_height (GIMP_ITEM (owner)));
      });
    if (scheduler.state () == FilterScheduler::State::closed) return false;
    maximum_quantum_us = std::max (maximum_quantum_us, g_get_monotonic_time () - started_at);
    if (previous != scheduler.state ()) g_signal_emit_by_name (owner, "filter-state-changed");
    return again;
  }
  GimpFilterLayer *owner;
  std::string procedure;
  BytesRef raw;
  std::shared_ptr<const FilterArguments> args;
  FilterScheduler scheduler;
  FairDispatcher::Ticket pending;
  WeakRef<GObject> stack;
  ObjectRef<GObject> staged;
  std::vector<Connection> own_connections, dependencies;
  Connection image_connection;
  std::vector<WeakRef<GObject>> lower_objects;
  bool topology_dirty = false;
  gint64 maximum_quantum_us = 0;
};
void topology_changed (GimpContainer *, GimpObject *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.structure_changed (); }); }
void reordered (GimpContainer *, GimpObject *, gint, gint, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.structure_changed (); }); }
void dependency_update (GimpDrawable *source, gint, gint, gint, gint, gpointer data)
{ visit (data, [&] (FilterImpl& impl) { if (impl.below (GIMP_ITEM (source))) impl.invalidate (); }); }
void dependency_active (GimpFilter *source, gpointer data)
{ visit (data, [&] (FilterImpl& impl) { if (impl.below (GIMP_ITEM (source))) impl.invalidate (); }); }
void dependency_status (GimpFilterLayer *source, gpointer data)
{ visit (data, [&] (FilterImpl& impl) {
    if (impl.below (GIMP_ITEM (source)))
      {
        const auto state = gimp_filter_layer_get_state (source);
        if (state == GIMP_FILTER_LAYER_WAITING || state == GIMP_FILTER_LAYER_CANCELLING) impl.invalidate ();
        else impl.schedule ();
      }
  }); }
void owner_ancestry (GimpViewable *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.structure_changed (); }); }
void owner_removed (GimpItem *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void owner_size (GimpViewable *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.configure (); }); }
void owner_offset (GObject *, GParamSpec *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void owner_active (GimpFilter *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void owner_image_changed (GObject *, GParamSpec *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.watch_image (); impl.structure_changed (); impl.invalidate (); }); }
void image_disconnected (GimpObject *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { gimp_painter_binding_close (G_OBJECT (impl.owner), nullptr); }); }
struct FilterUndoImpl
{
  void close () noexcept { raw.reset (); args.reset (); }
  std::string procedure;
  BytesRef raw;
  std::shared_ptr<const FilterArguments> args;
};
struct FilterUndoSlot : SlotSpec<GimpFilterLayerUndo, FilterUndoImpl> {};
void undo_constructed (GObject *object)
{
  G_OBJECT_CLASS (gimp_filter_layer_undo_parent_class)->constructed (object);
  auto *undo = reinterpret_cast<GimpFilterLayerUndo *> (object);
  if (!undo->binding_failed)
    undo->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
      auto& store = BindingStore::require (object);
      store.initialize<FilterUndoSlot> ([&] (FilterUndoImpl& snapshot) {
        BindingStore::require (G_OBJECT (GIMP_ITEM_UNDO (object)->item)).read<FilterSlot> ([&] (const FilterImpl& impl) {
          snapshot.procedure = impl.procedure;
          snapshot.raw = BytesRef (impl.raw ? g_bytes_ref (impl.raw.get ()) : nullptr);
          snapshot.args = impl.args;
        });
      });
      store.activate (); return TRUE;
    });
}
void undo_dispose (GObject *object)
{
  gimp_painter_binding_close (object, nullptr);
  G_OBJECT_CLASS (gimp_filter_layer_undo_parent_class)->dispose (object);
}
void undo_pop (GimpUndo *undo, GimpUndoMode mode, GimpUndoAccumulator *accum)
{
  GIMP_UNDO_CLASS (gimp_filter_layer_undo_parent_class)->pop (undo, mode, accum);
  boundary_void (nullptr, [&] {
    BindingStore::require (G_OBJECT (undo)).with<FilterUndoSlot> ([&] (FilterUndoImpl& snapshot) {
      BindingStore::require (G_OBJECT (GIMP_ITEM_UNDO (undo)->item)).with<FilterSlot> ([&] (FilterImpl& impl) {
        impl.procedure.swap (snapshot.procedure);
        impl.raw.swap (snapshot.raw);
        impl.args.swap (snapshot.args);
        impl.configure ();
        g_object_notify (G_OBJECT (impl.owner), "filter-procedure");
        g_object_notify (G_OBJECT (impl.owner), "filter-arguments");
        g_object_notify (G_OBJECT (impl.owner), "filter-original-definition");
      });
    });
  });
}
void constructed (GObject *object)
{
  if (G_OBJECT_CLASS (gimp_filter_layer_parent_class)->constructed)
    G_OBJECT_CLASS (gimp_filter_layer_parent_class)->constructed (object);
  auto *layer = GIMP_FILTER_LAYER (object);
  if (!layer->binding_failed)
    layer->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
      auto& store = BindingStore::require (object);
      store.initialize<FilterSlot> ([] (FilterImpl&) {});
      store.activate ();
      return TRUE;
    });
  if (layer->binding_failed) gimp_painter_binding_close (object, nullptr);
}
void get_property (GObject *object, guint property_id, GValue *value, GParamSpec *spec)
{
  auto *layer = GIMP_FILTER_LAYER (object);
  switch (property_id)
    {
    case 1: g_value_set_boolean (value, layer->binding_failed); break;
    case 2: g_value_take_string (value, gimp_filter_layer_dup_procedure (layer)); break;
    case 3: g_value_take_boxed (value, gimp_filter_layer_dup_args (layer)); break;
    case 4: g_value_take_boxed (value, gimp_filter_layer_ref_definition (layer)); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
    }
}
void dispose (GObject *object)
{ gimp_painter_binding_close (object, nullptr); G_OBJECT_CLASS (gimp_filter_layer_parent_class)->dispose (object); }
GimpItem *duplicate (GimpItem *item, GType type)
{
  return boundary<GimpItem *> (nullptr, nullptr, [&] {
    GimpItem *copy = GIMP_ITEM_CLASS (gimp_filter_layer_parent_class)->duplicate (item, type);
    auto partial = ObjectRef<GObject>::adopt (G_OBJECT (copy));
    if (GIMP_IS_FILTER_LAYER (copy))
      BindingStore::require (G_OBJECT (item)).read<FilterSlot> ([&] (const FilterImpl& impl) {
        BindingStore::require (G_OBJECT (copy)).with<FilterSlot> ([&] (FilterImpl& duplicate_impl) {
          duplicate_impl.procedure = impl.procedure;
          duplicate_impl.raw = BytesRef (impl.raw ? g_bytes_ref (impl.raw.get ()) : nullptr);
          duplicate_impl.args = impl.args;
          duplicate_impl.attach ();
          duplicate_impl.configure ();
          duplicate_impl.scheduler.mark_loaded ();
        });
      });
    return GIMP_ITEM (partial.release ());
  });
}
gboolean content_locked (GimpItem *item, GimpItem **locked) { if (locked) *locked = item; return TRUE; }
gdouble opacity_at (GimpPickable *, gint, gint) { return 0.0; }
} // namespace
static void gimp_filter_layer_undo_class_init (GimpFilterLayerUndoClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = undo_constructed;
  G_OBJECT_CLASS (klass)->dispose = undo_dispose;
  GIMP_UNDO_CLASS (klass)->pop = undo_pop;
}
static void gimp_filter_layer_undo_init (GimpFilterLayerUndo *undo)
{
  undo->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    BindingStore::ensure (G_OBJECT (undo)).emplace<FilterUndoSlot> (); return TRUE;
  });
}
static void gimp_filter_layer_class_init (GimpFilterLayerClass *klass)
{
  auto *object = G_OBJECT_CLASS (klass);
  object->constructed = constructed;
  object->dispose = dispose;
  object->get_property = get_property;
  g_object_class_install_property (object, 1,
    g_param_spec_boolean ("binding-failed", "Binding failed", "C++ state construction failed; object is inert", FALSE, G_PARAM_READABLE));
  g_object_class_install_property (object, 2,
    g_param_spec_string ("filter-procedure", "Filter procedure", "Saved procedure name", nullptr, G_PARAM_READABLE));
  g_object_class_install_property (object, 3,
    g_param_spec_boxed ("filter-arguments", "Filter arguments", "Resolved copy of saved execution arguments", GIMP_TYPE_VALUE_ARRAY, G_PARAM_READABLE));
  g_object_class_install_property (object, 4,
    g_param_spec_boxed ("filter-original-definition", "Original definition", "Unmodified original serialized definition", G_TYPE_BYTES, G_PARAM_READABLE));
  GIMP_ITEM_CLASS (klass)->duplicate = duplicate;
  GIMP_ITEM_CLASS (klass)->is_content_locked = content_locked;
  GIMP_VIEWABLE_CLASS (klass)->default_icon_name = "gimp-gegl";
  GIMP_VIEWABLE_CLASS (klass)->default_name = "Filter Layer";
  g_signal_new ("filter-state-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                0, nullptr, nullptr, nullptr, G_TYPE_NONE, 0);
}
static void gimp_filter_layer_init (GimpFilterLayer *layer)
{
  layer->binding_failed = !boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    BindingStore::ensure (G_OBJECT (layer)).emplace<FilterSlot> (layer); return TRUE;
  });
}
static void gimp_filter_layer_pickable_init (GimpPickableInterface *iface) { iface->get_opacity_at = opacity_at; }
GimpLayer *gimp_filter_layer_new (GimpImage *image, gint width, gint height, const gchar *name,
                                 gdouble opacity, GimpLayerMode mode)
{
  g_return_val_if_fail (GIMP_IS_IMAGE (image), nullptr);
  return boundary<GimpLayer *> (nullptr, nullptr, [&] {
    if (width <= 0) width = gimp_image_get_width (image);
    if (height <= 0) height = gimp_image_get_height (image);
    auto *layer = GIMP_FILTER_LAYER (gimp_drawable_new (GIMP_TYPE_FILTER_LAYER, image, name, 0, 0, width, height,
                                                       gimp_image_get_layer_format (image, TRUE)));
    if (!layer) return static_cast<GimpLayer *> (nullptr);
    auto partial = ObjectRef<GObject>::adopt (G_OBJECT (layer));
    if (layer->binding_failed) return static_cast<GimpLayer *> (nullptr);
    gimp_layer_set_opacity (GIMP_LAYER (layer), opacity, FALSE);
    gimp_layer_set_mode (GIMP_LAYER (layer), mode, FALSE);
    BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([] (FilterImpl& impl) { impl.attach (); });
    return GIMP_LAYER (partial.release ());
  });
}
static gboolean set_definition (GimpFilterLayer *layer, const gchar *procedure,
                                GBytes *raw, const GimpValueArray *args, bool push_undo, GError **error,
                                const std::shared_ptr<const FilterArguments> *imported = nullptr)
{
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    if (!GIMP_IS_FILTER_LAYER (layer)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected FilterLayer");
    std::string name = procedure ? procedure : "";
    BytesRef bytes (raw ? g_bytes_ref (raw) : nullptr);
    std::shared_ptr<const FilterArguments> arguments = imported ? *imported :
                                                       args ? std::make_shared<FilterArguments> (args) : nullptr;
    BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([&] (FilterImpl& impl) {
      if (push_undo && gimp_item_is_attached (GIMP_ITEM (layer)))
        {
          auto *undo = gimp_image_undo_push (gimp_item_get_image (GIMP_ITEM (layer)),
                                            gimp_filter_layer_undo_get_type (), GIMP_UNDO_FILTER_LAYER_DEFINITION,
                                            "Filter layer definition", GimpDirtyMask (GIMP_DIRTY_ITEM | GIMP_DIRTY_ITEM_META | GIMP_DIRTY_DRAWABLE),
                                            "item", layer, nullptr);
          if (undo && reinterpret_cast<GimpFilterLayerUndo *> (undo)->binding_failed)
            throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter definition Undo construction failed");
        }
      impl.procedure = std::move (name); impl.raw = std::move (bytes); impl.args = std::move (arguments);
      impl.attach (); impl.configure ();
      g_object_notify (G_OBJECT (layer), "filter-procedure");
      g_object_notify (G_OBJECT (layer), "filter-arguments");
      g_object_notify (G_OBJECT (layer), "filter-original-definition");
    });
    return TRUE;
  });
}
gboolean gimp_filter_layer_set_definition (GimpFilterLayer *layer, const gchar *procedure,
                                          GBytes *raw, const GimpValueArray *args, GError **error)
{ return set_definition (layer, procedure, raw, args, false, error); }
gboolean gimp_filter_layer_edit_definition (GimpFilterLayer *layer, const gchar *procedure,
                                           GBytes *raw, const GimpValueArray *args, GError **error)
{ return set_definition (layer, procedure, raw, args, true, error); }
#define FILTER_READ(type, fallback, expression) \
  return boundary<type> (nullptr, fallback, [&] { \
    if (!GIMP_IS_FILTER_LAYER (layer)) return fallback; \
    return BindingStore::require (G_OBJECT (layer)).read<FilterSlot> ([&] (const FilterImpl& impl) -> type { return expression; }); \
  })
gchar *gimp_filter_layer_dup_procedure (GimpFilterLayer *layer)
{ FILTER_READ (gchar *, static_cast<gchar *> (nullptr), g_strdup (impl.procedure.c_str ())); }
GBytes *gimp_filter_layer_ref_definition (GimpFilterLayer *layer)
{ FILTER_READ (GBytes *, static_cast<GBytes *> (nullptr), impl.raw ? g_bytes_ref (impl.raw.get ()) : nullptr); }
GimpValueArray *gimp_filter_layer_dup_args (GimpFilterLayer *layer)
{ FILTER_READ (GimpValueArray *, static_cast<GimpValueArray *> (nullptr), impl.args ? impl.args->copy_values () : nullptr); }
GimpFilterLayerState gimp_filter_layer_get_state (GimpFilterLayer *layer)
{ FILTER_READ (GimpFilterLayerState, GIMP_FILTER_LAYER_CLOSED, static_cast<GimpFilterLayerState> (impl.scheduler.state ())); }
gchar *gimp_filter_layer_dup_error (GimpFilterLayer *layer)
{ FILTER_READ (gchar *, static_cast<gchar *> (nullptr), g_strdup (impl.scheduler.error ().c_str ())); }
guint64 gimp_filter_layer_get_generation (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.scheduler.generation ()); }
guint64 gimp_filter_layer_get_cache_generation (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.scheduler.cache_generation ()); }
guint64 gimp_filter_layer_get_run_count (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.scheduler.starts ()); }
gint64 gimp_filter_layer_get_max_quantum_us (GimpFilterLayer *layer)
{ FILTER_READ (gint64, gint64 (0), impl.maximum_quantum_us); }
#undef FILTER_READ
void gimp_filter_layer_mark_as_loaded (GimpFilterLayer *layer)
{ boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([] (FilterImpl& impl) {
    impl.scheduler.mark_loaded (); impl.staged.reset (); }); }); }
void gimp_filter_layer_invalidate (GimpFilterLayer *layer)
{ boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([] (FilterImpl& impl) { impl.invalidate (); }); }); }

gboolean gimp_filter_layer_get_argument_reference (GimpFilterLayer *layer, guint argument, guint element,
                                                  GType *type, gint64 *id, gboolean *expired)
{
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    if (!GIMP_IS_FILTER_LAYER (layer)) return FALSE;
    return BindingStore::require (G_OBJECT (layer)).read<FilterSlot> ([&] (const FilterImpl& impl) -> gboolean {
      const auto *ref = impl.args ? impl.args->reference (argument, element) : nullptr;
      if (!ref) return FALSE;
      if (type) *type = ref->type;
      if (id) *id = ref->id;
      if (expired) *expired = ref->had_object && !ref->target.lock ();
      return TRUE;
    });
  });
}

gboolean gimp_filter_layer_get_snapshot_state (GimpFilterLayer *layer, GimpFilterLayerSnapshot *snapshot)
{
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    if (!GIMP_IS_FILTER_LAYER (layer) || !snapshot) return FALSE;
    return BindingStore::require (G_OBJECT (layer)).read<FilterSlot> ([&] (const FilterImpl& impl) -> gboolean {
      const auto saved = impl.scheduler.snapshot ();
      *snapshot = {1, saved.generation, saved.cache_generation, saved.cache_complete,
                   static_cast<GimpFilterLayerState> (impl.scheduler.state ())};
      return TRUE;
    });
  });
}
gboolean gimp_filter_layer_restore_snapshot_state (GimpFilterLayer *layer,
                                                   const GimpFilterLayerSnapshot *snapshot, GError **error)
{
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    if (!GIMP_IS_FILTER_LAYER (layer)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected FilterLayer");
    if (!snapshot || snapshot->version != 1 ||
        (snapshot->cache_complete != FALSE && snapshot->cache_complete != TRUE) ||
        snapshot->state < GIMP_FILTER_LAYER_CLEAN || snapshot->state > GIMP_FILTER_LAYER_CLOSED ||
        snapshot->cache_generation > snapshot->generation || snapshot->generation > G_MAXINT64)
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Invalid FilterLayer snapshot schema");
    BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([&] (FilterImpl& impl) {
      impl.scheduler.restore_cache ({snapshot->generation, snapshot->cache_generation, bool (snapshot->cache_complete)});
      impl.staged.reset ();
      impl.schedule ();
      g_signal_emit_by_name (impl.owner, "filter-state-changed");
    });
    return TRUE;
  });
}

struct _GimpFilterArgumentsSnapshot
{
  explicit _GimpFilterArgumentsSnapshot (std::shared_ptr<const FilterArguments> arguments)
    : arguments (std::move (arguments)) {}
  std::shared_ptr<const FilterArguments> arguments;
};
GimpFilterArgumentsSnapshot *gimp_filter_layer_snapshot_arguments (GimpFilterLayer *layer)
{
  return boundary<GimpFilterArgumentsSnapshot *> (nullptr, nullptr, [&] {
    if (!GIMP_IS_FILTER_LAYER (layer)) return static_cast<GimpFilterArgumentsSnapshot *> (nullptr);
    return BindingStore::require (G_OBJECT (layer)).read<FilterSlot> ([&] (const FilterImpl& impl) -> GimpFilterArgumentsSnapshot * {
      return impl.args ? new GimpFilterArgumentsSnapshot (impl.args) : nullptr;
    });
  });
}
void gimp_filter_arguments_snapshot_free (GimpFilterArgumentsSnapshot *snapshot) { delete snapshot; }
guint gimp_filter_arguments_snapshot_count (const GimpFilterArgumentsSnapshot *snapshot)
{ return snapshot ? snapshot->arguments->size () : 0; }
GType gimp_filter_arguments_snapshot_type (const GimpFilterArgumentsSnapshot *snapshot, guint argument)
{
  return snapshot && argument < snapshot->arguments->size () ?
         G_VALUE_TYPE (snapshot->arguments->at (argument)) : G_TYPE_INVALID;
}
gboolean gimp_filter_arguments_snapshot_value (const GimpFilterArgumentsSnapshot *snapshot, guint argument, GValue *value)
{
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    if (!snapshot || !value || G_IS_VALUE (value) || argument >= snapshot->arguments->size () ||
        !snapshot->arguments->scalar (argument)) return FALSE;
    const GValue *source = snapshot->arguments->at (argument);
    g_value_init (value, G_VALUE_TYPE (source)); g_value_copy (source, value);
    return TRUE;
  });
}
guint gimp_filter_arguments_snapshot_reference_count (const GimpFilterArgumentsSnapshot *snapshot, guint argument)
{ return snapshot ? snapshot->arguments->reference_count (argument) : 0; }
gboolean gimp_filter_arguments_snapshot_reference (const GimpFilterArgumentsSnapshot *snapshot, guint argument,
                                                   guint element, GimpFilterArgumentReference *reference)
{
  return boundary<gboolean> (nullptr, FALSE, [&] () -> gboolean {
    const auto *ref = snapshot ? snapshot->arguments->reference (argument, element) : nullptr;
    if (!ref || !reference) return FALSE;
    *reference = {ref->type, ref->id, ref->had_object, ref->had_object && !ref->target.lock ()};
    return TRUE;
  });
}
GimpFilterArgumentsSnapshot *gimp_filter_arguments_snapshot_nested (const GimpFilterArgumentsSnapshot *snapshot, guint argument)
{
  return boundary<GimpFilterArgumentsSnapshot *> (nullptr, nullptr, [&] {
    auto nested = snapshot ? snapshot->arguments->nested (argument) : nullptr;
    return nested ? new GimpFilterArgumentsSnapshot (std::move (nested)) : nullptr;
  });
}

gboolean gimp_filter_arguments_snapshot_is_null (const GimpFilterArgumentsSnapshot *snapshot, guint argument)
{
  return snapshot && argument < snapshot->arguments->size () && snapshot->arguments->is_null (argument);
}

GimpFilterArgumentsSnapshot *gimp_filter_arguments_snapshot_import (guint count,
                                                                   const GimpFilterArgumentSpec *specs, GError **error)
{
  return boundary<GimpFilterArgumentsSnapshot *> (error, nullptr, [&] {
    std::size_t remaining = 65536;
    auto arguments = std::make_shared<FilterArguments> (count, specs, 0, remaining);
    return new GimpFilterArgumentsSnapshot (std::move (arguments));
  });
}
gboolean gimp_filter_layer_set_definition_with_snapshot (GimpFilterLayer *layer, const gchar *procedure,
                                                         GBytes *raw, const GimpFilterArgumentsSnapshot *snapshot, GError **error)
{
  std::shared_ptr<const FilterArguments> arguments = snapshot ? snapshot->arguments : nullptr;
  return set_definition (layer, procedure, raw, nullptr, false, error, &arguments);
}
