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
#include "gimpchannel.h"
#include "gimpcontext.h"
#include "gimp.h"
#include "gimpimage-undo.h"
#include "gimpitemundo.h"
#include "gimppickable.h"
#include "gimpprojectable.h"
#include "gegl/gimp-babl.h"
}
#include "gimp-painter-type-traits.hpp"
#include "gimpfilterlayer-arguments.hpp"
#include "gimpfilterpaths.hpp"
#include "gimpfiltercontext.hpp"
#include "painter/binding-store.hpp"
#include "painter/connection.hpp"
#include "painter/filter-scheduler.hpp"
#include "config/gimppainterfilterconfig.hpp"
#include "painter/fair-dispatcher.hpp"
#include "painter/filter-edge.hpp"
#include "painter/filter-gauss.hpp"
#include "painter/filter-raster-kernels.hpp"
#include "painter/filter-native-kernels.hpp"
#include "painter/filter-process.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter/source.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <limits>
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
struct CloneReferenceFree
{ void operator() (GimpCloneLayerReference *p) const noexcept { gimp_clone_layer_reference_free (p); } };
using CloneReference = std::unique_ptr<GimpCloneLayerReference, CloneReferenceFree>;
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
void graph_status (GimpFilterLayer *, gpointer);
void graph_update (GimpDrawable *, gint, gint, gint, gint, gpointer);
void owner_ancestry (GimpViewable *, gpointer);
void owner_removed (GimpItem *, gpointer);
void owner_size (GimpViewable *, gpointer);
void owner_offset (GObject *, GParamSpec *, gpointer);
void owner_active (GimpFilter *, gpointer);
void owner_image_changed (GObject *, GParamSpec *, gpointer);
void owner_format_changed (GimpDrawable *, gpointer);
void image_profile_changed (GObject *, gpointer);
void filter_budget_changed (GObject *, GParamSpec *, gpointer);
void image_disconnected (GimpObject *, gpointer);
void selection_invalidated (GimpImage *, gpointer);

struct FilterImpl
{
  explicit FilterImpl (GimpFilterLayer *owner) : owner (owner) {}
  ~FilterImpl () noexcept = default;
  void close () noexcept
  {
    native_context.reset ();
    scheduler.close ();
    pending.close ();
    dependencies.clear ();
    graph_connections.clear ();
    graph_filters.clear ();
    graph_clones.clear ();
    graph_nodes.clear ();
    own_connections.clear ();
    selection_connection.close ();
    context_selection.reset ();
    image_connection.close ();
    profile_connection.close ();
    config_connection.close ();
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
    own_connections.push_back (connect (G_OBJECT (owner), "format-changed", G_CALLBACK (owner_format_changed)));
    watch_image ();
    refresh_connections ();
  }
  void watch_image ()
  {
    selection_connection.close ();
    context_selection.reset ();
    native_context.reset ();
    image_connection.close ();
    profile_connection.close ();
    config_connection.close ();
    if (GimpImage *image = gimp_item_get_image (GIMP_ITEM (owner)))
      {
        image_connection = connect (G_OBJECT (image), "disconnect", G_CALLBACK (image_disconnected));
        selection_connection = connect (G_OBJECT (image), "selection-invalidate", G_CALLBACK (selection_invalidated));
        profile_connection = connect (G_OBJECT (image), "profile-changed", G_CALLBACK (image_profile_changed));
        config_connection = connect (G_OBJECT (image->gimp->config), "notify::painter-filter-spill-size", G_CALLBACK (filter_budget_changed));
      }
  }
  void refresh_context_selection ()
  {
    auto *image = gimp_item_get_image (GIMP_ITEM (owner));
    if (!image) throw std::runtime_error ("Filter selection image was closed");
    auto *selection = G_OBJECT (gimp_image_get_mask (image));
    if (context_selection.lock ().get () != selection)
      {
        /* XCF replaces the selection object after loading layers, without
         * notifying the layer's image property. Identity is an owner epoch,
         * not a Filter input generation and never restarts a clean Filter. */
        context_selection = WeakRef<GObject> (ObjectRef<GObject>::retain (selection));
        native_context.selection_changed ();
      }
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
    graph_valid = false;
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
        if (below (GIMP_ITEM (child))) watch_descendant_status (child);
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
    graph_valid = false;
    read_budget = 1024;
    native_context.reset ();
    scheduler.invalidate ();
    staged.reset ();
    schedule ();
    g_signal_emit_by_name (owner, "filter-state-changed");
  }
  void dependency_dirty ()
  {
    /* A waiting generation has captured no input. Repeated paths through a
     * dependency DAG must not emit another WAITING signal and recursively
     * amplify the same edit across every ancestor. */
    if (scheduler.state () == FilterScheduler::State::waiting)
      { graph_valid = false; read_budget = 1024; schedule (); }
    else
      invalidate ();
  }
  void structure_changed ()
  { refresh_connections (); schedule (); }
  void definition_installed ()
  {
    if (definition_revision == std::numeric_limits<std::uint64_t>::max ())
      throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter definition revision exhausted");
    ++definition_revision;
  }
  bool definition_current (std::uint64_t binding_token, std::uint64_t revision) const
  {
    auto *store = BindingStore::find (G_OBJECT (owner));
    return store && store->accepts (binding_token) && definition_revision == revision;
  }
  bool publish_definition (GeglBuffer *restore_buffer = nullptr, bool cache_complete = true)
  {
    auto& store = BindingStore::require (G_OBJECT (owner));
    const auto token = store.generation (), revision = definition_revision;
    configure ();
    if (!definition_current (token, revision)) return false;
    if (restore_buffer)
      {
        /* Undo owns the old committed pixels, not a claim about current lower
         * content. Keep them visible, but only a successful rerun certifies
         * freshness. In particular, an opaque model must never inherit the
         * newer procedure's pixels or be marked complete/current by Undo. */
        native_context.reset ();
        scheduler.restore_cache ({1, 0, cache_complete});
        staged.reset (); schedule ();
        publish_buffer (restore_buffer, scheduler.generation (), false);
        if (!definition_current (token, revision)) return false;
      }
    for (const char *property : {"filter-procedure", "filter-arguments", "filter-original-definition", "filter-opaque-arguments"})
      {
        g_object_notify (G_OBJECT (owner), property);
        if (!definition_current (token, revision)) return false;
      }
    return true;
  }
  void configure ()
  {
    graph_valid = false;
    read_budget = 1024;
    native_procedure.reset ();
    native_process_options.reset ();
    native_outcome.reset ();
    native_context.reset ();
    FilterScheduler::Request request;
    request.width = gimp_item_get_width (GIMP_ITEM (owner));
    request.height = gimp_item_get_height (GIMP_ITEM (owner));
    const bool real = real_samples ();
    request.bytes_per_pixel = real ? 32 : 4;
    const bool spill = request.height && request.width > (4 * 1024 * 1024 / request.bytes_per_pixel) / request.height;
    if (spill)
      {
        /* Independent queues + transpose/coefficient bookkeeping + worst IIR
         * line state. GEGL input/staging tiles use the host shared cache/swap
         * budget; no full input or result vector is allocated by this route. */
        const std::uint64_t peak = (real ? 16 : 8) * 1024 * 1024 + std::uint64_t (std::max (request.width, request.height)) * (real ? 128 : 72);
        request.peak_bytes = std::size_t (std::min (peak, std::uint64_t (std::numeric_limits<std::size_t>::max ())));
      }
    else if (request.height && request.width <= FilterScheduler::maximum_pixels / request.height)
      {
        /* Input/result/staging plus maximum Gaussian IIR line buffers. The
         * 2 MiB allowance covers bounded RLE coefficients and native chunks.
         * Existing completed GEGL caches remain the host's own swap policy. */
        const std::uint64_t peak = std::uint64_t (request.width) * request.height * (real ? 128 : 12) +
                                  std::uint64_t (std::max (request.width, request.height)) * (real ? 128 : 72) + 2 * 1024 * 1024;
        request.peak_bytes = std::size_t (std::min (peak, std::uint64_t (std::numeric_limits<std::size_t>::max ())));
      }
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
                if (spill)
                  request.raster_process = [width, height, options] (FilterRaster& input, FilterRaster& output,
                    std::atomic<bool>& cancel, const FilterRasterFactory&) {
                    return filter_edge_raster (input, width, height, options, cancel, output);
                  };
                if (real)
                  {
                    FilterNativeProcess process = [width,height,options] (FilterRaster& input, FilterRaster& output,
                      std::atomic<bool>& cancel, const FilterRasterFactory&) {
                      return filter_edge_real_raster (input,output,width,height,options,cancel);
                    };
                    request.process = [process] (const FilterScheduler::Bytes& input, std::atomic<bool>& cancel,
                                                  FilterScheduler::Bytes& output) {
                      return filter_native_vector (input,cancel,output,process);
                    };
                    if (spill) request.raster_process = std::move (process);
                  }
              }
          }
      }
    else if (args && (procedure == "plug-in-gauss" || procedure == "plug-in-gauss-iir" ||
                      procedure == "plug-in-gauss-rle" || procedure == "plug-in-gauss-iir2" ||
                      procedure == "plug-in-gauss-rle2"))
      {
        const bool flags = procedure == "plug-in-gauss-iir" || procedure == "plug-in-gauss-rle";
        const bool canonical = procedure == "plug-in-gauss";
        const auto floating = [] (const GValue *value) { return G_VALUE_HOLDS_DOUBLE (value) || G_VALUE_HOLDS_FLOAT (value); };
        const auto argument_real = [] (const GValue *value) { return G_VALUE_HOLDS_DOUBLE (value) ? g_value_get_double (value) : g_value_get_float (value); };
        if (args->size () == (flags || canonical ? 6 : 5))
          {
            const GValue *a = args->at (3), *b = args->at (4), *c = args->size () == 6 ? args->at (5) : nullptr;
            GaussOptions options;
            bool valid = floating (a);
            if (flags)
              {
                valid = valid && G_VALUE_HOLDS_INT (b) && G_VALUE_HOLDS_INT (c);
                if (valid)
                  {
                    const auto radius = argument_real (a);
                    valid = std::isfinite (radius) && radius > 0;
                    options.horizontal = g_value_get_int (b) ? radius : 0;
                    options.vertical = g_value_get_int (c) ? radius : 0;
                  }
              }
            else
              {
                valid = valid && floating (b) && (!canonical || G_VALUE_HOLDS_INT (c));
                if (valid)
                  {
                    options.horizontal = argument_real (a); options.vertical = argument_real (b);
                    valid = std::isfinite (options.horizontal) && std::isfinite (options.vertical) &&
                            (options.horizontal > 0 || options.vertical > 0);
                  }
              }
            if (valid)
              {
                options.method = canonical ? g_value_get_int (c) :
                  (procedure == "plug-in-gauss-rle" || procedure == "plug-in-gauss-rle2" ? 1 : 0);
                valid = options.method >= 0 && options.method <= 1;
              }
            if (valid)
              {
                const bool identity = flags && options.horizontal == 0 && options.vertical == 0;
                if (spill) {
                  const std::uint64_t multiplier = request.bytes_per_pixel * (options.vertical > 0 ? 3 : 2);
                  if (std::uint64_t (request.width) > std::numeric_limits<std::uint64_t>::max () / multiplier / request.height)
                    throw std::invalid_argument ("Gaussian spill reservation exceeds64-bit accounting");
                  request.peak_spill_bytes = std::uint64_t (request.width) * request.height * multiplier;
                }
                const auto width = request.width, height = request.height;
                request.process = [width, height, options, identity] (const FilterScheduler::Bytes& input,
                                                                      std::atomic<bool>& cancel,
                                                                      FilterScheduler::Bytes& output) {
                  if (!identity) return filter_gauss (input, width, height, options, cancel, output);
                  if (cancel.load (std::memory_order_relaxed)) return false;
                  FilterScheduler::Bytes copy (input);
                  if (cancel.load (std::memory_order_relaxed)) return false;
                  output.swap (copy); return true;
                };
                if (spill)
                  request.raster_process = [width, height, options, identity] (FilterRaster& input, FilterRaster& output,
                    std::atomic<bool>& cancel, const FilterRasterFactory& scratch) {
                    if (!identity) return filter_gauss_raster (input, width, height, options, cancel, output, scratch);
                    FilterScheduler::Bytes chunk (FilterScheduler::pixel_budget * 4);
                    for (std::uint64_t offset = 0; offset < input.size ();)
                      {
                        if (cancel.load (std::memory_order_relaxed)) return false;
                        const auto count = std::size_t (std::min (std::uint64_t (chunk.size ()),input.size () - offset));
                        input.read (offset,count,chunk.data ()); output.write (offset,count,chunk.data ()); offset += count;
                      }
                    output.flush (); return !cancel.load (std::memory_order_relaxed);
                  };
                if (real && !identity)
                  {
                    FilterNativeProcess process = [width,height,options] (FilterRaster& input, FilterRaster& output,
                      std::atomic<bool>& cancel, const FilterRasterFactory& scratch) {
                      return filter_gauss_real_raster (input,output,width,height,options,cancel,scratch);
                    };
                    request.process = [process] (const FilterScheduler::Bytes& input, std::atomic<bool>& cancel,
                                                  FilterScheduler::Bytes& output) {
                      return filter_native_vector (input,cancel,output,process);
                    };
                    if (spill) request.raster_process = std::move (process);
                  }
              }
          }
      }
    else if (args &&
             ((procedure == "plug-in-vinvert" && args->size () == 3 && !gray ()) ||
              (procedure == "plug-in-max-rgb" && args->size () == 4 && !gray () && G_VALUE_HOLDS_INT (args->at (3))) ||
              (procedure == "plug-in-threshold-alpha" && args->size () == 4 && G_VALUE_HOLDS_INT (args->at (3)))))
      {
        const auto operation = procedure == "plug-in-vinvert" ? FilterPoint::value_invert :
          procedure == "plug-in-max-rgb" ? FilterPoint::max_rgb : FilterPoint::threshold_alpha;
        const auto argument = args->size () == 4 ? g_value_get_int (args->at (3)) : 0;
        const auto width = request.width, height = request.height;
        FilterNativeProcess process = [width,height,operation,argument,real] (FilterRaster& input, FilterRaster& output,
          std::atomic<bool>& cancel, const FilterRasterFactory&) {
          return filter_point_raster (input,output,width,height,operation,argument,real,cancel);
        };
        request.process = [process] (const FilterScheduler::Bytes& input, std::atomic<bool>& cancel,
                                      FilterScheduler::Bytes& output) {
          return filter_native_vector (input,cancel,output,process);
        };
        if (spill) request.raster_process = std::move (process);
      }
    else if (!real && args &&
             ((procedure == "plug-in-blinds" && args->size () == 7 &&
               G_VALUE_HOLDS_INT (args->at (3)) && G_VALUE_HOLDS_INT (args->at (4)) &&
               G_VALUE_HOLDS_INT (args->at (5)) && G_VALUE_HOLDS_INT (args->at (6))) ||
              (procedure == "plug-in-small-tiles" && args->size () == 4 &&
               G_VALUE_HOLDS_INT (args->at (0)) && G_VALUE_HOLDS_INT (args->at (1)) &&
               G_VALUE_HOLDS_INT (args->at (2)) && G_VALUE_HOLDS_INT (args->at (3))) ||
              (procedure == "plug-in-retinex" && !gray () && args->size () == 7 &&
               G_VALUE_HOLDS_INT (args->at (0)) && G_VALUE_HOLDS_INT (args->at (1)) &&
               G_VALUE_HOLDS_INT (args->at (2)) && G_VALUE_HOLDS_INT (args->at (3)) &&
               G_VALUE_HOLDS_INT (args->at (4)) && G_VALUE_HOLDS_INT (args->at (5)) &&
               G_VALUE_HOLDS_DOUBLE (args->at (6)))))
      {
        const bool tiles = procedure == "plug-in-small-tiles";
        const bool retinex = procedure == "plug-in-retinex";
        const auto angle = tiles || retinex ? 0 : g_value_get_int (args->at (3));
        const auto segments = tiles || retinex ? 1 : g_value_get_int (args->at (4));
        const auto factor = tiles ? g_value_get_int (args->at (3)) : 2;
        const auto scale = retinex ? g_value_get_int (args->at (3)) : 240;
        const auto nscales = retinex ? g_value_get_int (args->at (4)) : 3;
        const auto scales_mode = retinex ? g_value_get_int (args->at (5)) : 0;
        const auto cvar = retinex ? g_value_get_double (args->at (6)) : 1.2;
        if (angle >= 0 && angle <= 90 && segments >= 1 && segments <= 100 && factor >= 0 && factor <= 6 &&
            (!retinex || (request.width >= 16 && request.height >= 16 &&
              std::uint64_t (request.width) * request.height <= std::uint64_t (G_MAXINT) / 4 &&
              scale >= 16 && scale <= 256 && nscales >= 0 && nscales <= 8 &&
              scales_mode >= 0 && scales_mode <= 2 && std::isfinite (cvar) && cvar >= 0 && cvar <= 4)))
          {
            auto descriptor = std::make_shared<FilterProcedureRequest> ();
            descriptor->width = request.width; descriptor->height = request.height;
            descriptor->angle = angle; descriptor->segments = segments;
            descriptor->procedure = retinex ? FilterProcedure::retinex :
                                    tiles ? FilterProcedure::small_tiles : FilterProcedure::blinds;
            descriptor->tiles = factor;
            descriptor->scale = scale; descriptor->nscales = nscales;
            descriptor->scales_mode = scales_mode; descriptor->cvar = cvar;
            descriptor->orientation = tiles || retinex ? 0 : g_value_get_int (args->at (5));
            descriptor->transparent = tiles || retinex ? 0 : g_value_get_int (args->at (6)); descriptor->gray = gray ();
            auto options = std::make_shared<FilterProcessOptions> ();
            auto outcome = std::make_shared<FilterProcedureResult> ();
            /* Parent input/result and three child rasters (snapshot, drawable,
             * shadow), plus an owner native input and final selection snapshot.
             * Conservatively reserve the owner logical size in BOTH memory and
             * spill budgets; GEGL actual tiles still obey its shared cache/swap.
             * Other live caches and child libraries are not total-RSS bounds. */
            const auto raster = descriptor->bytes ();
            const auto owner_context_bytes = std::uint64_t (request.width) * request.height * (gray () ? 3 : 5);
            request.peak_spill_bytes = raster * 5 + owner_context_bytes;
            const auto peak = owner_context_bytes + std::uint64_t (256) * 1024 * 1024 + descriptor->scratch_bytes ();
            request.peak_bytes = std::size_t (std::min (peak, std::uint64_t (std::numeric_limits<std::size_t>::max ())));
            request.raster_process = [descriptor,options,outcome] (FilterRaster& input, FilterRaster& output,
              std::atomic<bool>& cancel, const FilterRasterFactory&) {
                /* Filesystem lookup belongs to the independent worker. Copy
                 * owner-prepared scalars instead of mutating shared options. */
                auto resolved = *options;
                resolved.executable = filter_worker_path ();
                return filter_process (*descriptor, input, output, cancel, resolved, outcome);
              };
            native_procedure = std::move (descriptor); native_process_options = std::move (options);
            native_outcome = std::move (outcome);
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
  struct FilterDependency
  {
    WeakRef<GObject> object;
    std::uint64_t generation;
  };
  struct CloneDependency { WeakRef<GObject> object, source; bool had_source; };
  bool plan_graph_nodes ()
  {
    graph_nodes.clear (); graph_node_cursor = 0; graph_fish_cursor = 0;
    auto container = stack.lock ();
    if (!container || !GIMP_IS_LIST (container.get ())) return false;
    /* Node construction is the expensive part of the host stack's synchronous
     * get_graph(). Snapshot a child-before-parent plan without entering any
     * get_node vfunc or resolving Clone references during borrowed traversal.
     * Include active layers above us: the host links the entire stack. */
    struct Frame { GimpFilter *filter; GList *next = nullptr; bool entered = false; };
    std::vector<Frame> frames;
    std::set<GimpFilter *> visited;
    for (GList *root = GIMP_LIST (container.get ())->queue->head; root; root = root->next)
      {
        auto *filter = GIMP_FILTER (root->data);
        if (!gimp_filter_get_active (filter)) continue;
        frames.push_back ({filter});
        while (!frames.empty ())
          {
            Frame& frame = frames.back ();
            if (!frame.entered)
              {
                if (gimp_filter_peek_node (frame.filter)) { frames.pop_back (); continue; }
                if (!visited.insert (frame.filter).second || visited.size () > 4096) return false;
                frame.entered = true;
                if (auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (frame.filter)))
                  {
                    if (!GIMP_IS_LIST (children)) return false;
                    frame.next = GIMP_LIST (children)->queue->head;
                  }
              }
            while (frame.next && !gimp_filter_get_active (GIMP_FILTER (frame.next->data))) frame.next = frame.next->next;
            if (frame.next)
              {
                auto *child = GIMP_FILTER (frame.next->data); frame.next = frame.next->next;
                frames.push_back ({child}); continue;
              }
            graph_nodes.emplace_back (ObjectRef<GObject>::retain (G_OBJECT (frame.filter)));
            frames.pop_back ();
          }
      }
    return true;
  }
  bool prepare_graph_quantum ()
  {
    /* The host layer-mode prepare eagerly creates all twelve conversion fishes
     * in one call. Prime the same immutable Babl cache entries cooperatively for
     * default and native image spaces; no pixels or graph state are evaluated.
     * This is a performance hint only: host nodes still prepare their own exact
     * formats, including any other space a custom operation requests. */
    const auto started = g_get_monotonic_time ();
    static const char *formats[] = {"RGBA float", "R~G~B~A float", "R'G'B'A float", "CIE Lab alpha float"};
    static const unsigned pairs[12][2] = {{0,1},{0,2},{0,3},{2,0},{1,0},{1,3},{2,3},{3,0},{3,1},{3,2},{1,2},{2,1}};
    while (graph_fish_cursor < 24)
      {
        const auto index = graph_fish_cursor++;
        const auto from = pairs[index % 12][0], to = pairs[index % 12][1];
        const Babl *space = index < 12 ? nullptr : babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (owner)));
        babl_fish (babl_format_with_space (formats[from],space), babl_format_with_space (formats[to],space));
        if (g_get_monotonic_time () - started >= 2000) return true;
      }
    while (graph_node_cursor < graph_nodes.size ())
      {
        // Publish the cursor and end every vector borrow before callback entry.
        auto object = graph_nodes[graph_node_cursor++].lock ();
        if (!object) { graph_valid = false; return true; }
        if (gimp_filter_peek_node (GIMP_FILTER (object.get ()))) continue;
        auto image = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (owner))));
        if (!image) { scheduler.close (); return false; }
        gimp_filter_get_node (GIMP_FILTER (object.get ()));
        // Reentry may clear the whole plan, invalidate dependencies or close.
        // Do not consult old plan indices/references or read pixels this turn.
        return true;
      }
    return false;
  }
  bool validate_graph ()
  {
    graph_connections.clear ();
    graph_filters.clear ();
    graph_clones.clear ();
    const auto invalid = [&] {
      graph_connections.clear (); graph_filters.clear (); graph_clones.clear (); graph_nodes.clear (); return false;
    };
    auto container = stack.lock ();
    if (!container || !GIMP_IS_LIST (container.get ())) return invalid ();
    GList *root = g_list_find (GIMP_LIST (container.get ())->queue->head, owner);
    if (!root) return invalid ();
    struct Frame { GimpLayer *layer; GList *next = nullptr; GimpLayer *source = nullptr; bool entered = false; };
    std::vector<Frame> frames;
    std::vector<CloneReference> clone_snapshots;
    std::set<GimpLayer *> path, complete;
    std::size_t visited = 0;
    for (root = root->next; root; root = root->next)
      {
        auto *layer = GIMP_LAYER (root->data);
        if (!gimp_item_get_visible (GIMP_ITEM (layer))) continue;
        frames.push_back ({layer});
        while (!frames.empty ())
          {
            Frame& frame = frames.back ();
            layer = frame.layer;
            if (!frame.entered)
              {
                if (complete.count (layer)) { frames.pop_back (); continue; }
                if (layer == GIMP_LAYER (owner) || !path.insert (layer).second || ++visited > 4096) return invalid ();
                frame.entered = true;
                if (GIMP_IS_CLONE_LAYER (layer))
                  {
                    CloneReference reference (gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (layer), nullptr));
                    if (!reference) return invalid ();
                    frame.source = reference->source;
                    graph_clones.push_back ({WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (layer))),
                      WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (frame.source))), frame.source != nullptr});
                    /* Also observe clones reached through another clone, whose
                     * referenced image may be outside our own layer stack. */
                    graph_connections.push_back (connect (G_OBJECT (layer), "update", G_CALLBACK (graph_update)));
                    clone_snapshots.push_back (std::move (reference));
                  }
                else if (GIMP_IS_FILTER_LAYER (layer))
                  {
                    graph_filters.push_back ({WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (layer))),
                      gimp_filter_layer_get_generation (GIMP_FILTER_LAYER (layer))});
                    graph_connections.push_back (connect (G_OBJECT (layer), "filter-state-changed", G_CALLBACK (graph_status)));
                    auto *children = gimp_item_get_container (GIMP_ITEM (layer));
                    if (children && GIMP_IS_LIST (children))
                      {
                        GList *self = g_list_find (GIMP_LIST (children)->queue->head, layer);
                        frame.next = self ? self->next : nullptr;
                      }
                  }
                else if (auto *children = gimp_viewable_get_children (GIMP_VIEWABLE (layer)))
                  {
                    if (!GIMP_IS_LIST (children)) return invalid ();
                    frame.next = GIMP_LIST (children)->queue->head;
                  }
              }
            if (frame.source)
              {
                auto *source = frame.source;
                frame.source = nullptr;
                /* Clone renders its source even when that source is hidden. */
                frames.push_back ({source});
                continue;
              }
            while (frame.next && !gimp_item_get_visible (GIMP_ITEM (frame.next->data))) frame.next = frame.next->next;
            if (frame.next)
              {
                auto *child = GIMP_LAYER (frame.next->data);
                frame.next = frame.next->next;
                frames.push_back ({child});
                continue;
              }
            path.erase (layer);
            complete.insert (layer);
            frames.pop_back ();
          }
      }
    if (!plan_graph_nodes ()) return invalid ();
    graph_valid = true;
    return true;
  }
  bool ready (bool& failed)
  {
    if (!graph_valid && !validate_graph ()) { failed = true; return false; }
    /* Validation is cached, but not ownership or freshness. Weak endpoints,
     * live clone targets and Filter states/generations are checked at every
     * scheduling checkpoint, including a missed or manually suppressed signal. */
    bool changed = false, all_ready = true;
    for (const auto& entry : graph_clones)
      {
        auto object = entry.object.lock (), source = entry.source.lock ();
        CloneReference reference (object ? gimp_clone_layer_dup_reference (GIMP_CLONE_LAYER (object.get ()), nullptr) : nullptr);
        if (!reference || (entry.had_source && !source) || source.get () != G_OBJECT (reference->source))
          { changed = true; break; }
      }
    if (!changed)
      for (const auto& entry : graph_filters)
        {
          auto object = entry.object.lock ();
          if (!object) { changed = true; break; }
          auto *filter = GIMP_FILTER_LAYER (object.get ());
          const auto state = gimp_filter_layer_get_state (filter);
          if (state == GIMP_FILTER_LAYER_FAILED || state == GIMP_FILTER_LAYER_CLOSED) { failed = true; return false; }
          if (entry.generation != gimp_filter_layer_get_generation (filter)) { changed = true; break; }
          if (state != GIMP_FILTER_LAYER_CLEAN) all_ready = false;
        }
    if (changed)
      {
        /* All owned snapshots and iteration references above have ended.
         * Invalidation emits user callbacks: do not traverse again afterwards
         * in this quantum. A fresh scheduled turn checks owner/generation. */
        graph_recheck = true;
        invalidate ();
        return false;
      }
    return all_ready;
  }
  bool input_current (std::uint64_t generation)
  {
    if (generation != scheduler.generation () || scheduler.state () != FilterScheduler::State::preparing) return false;
    bool failed = false;
    const bool current = ready (failed);
    if (failed) scheduler.reject ("Filter dependency failed, is cyclic, or exceeds the graph limit");
    else if (!current && generation == scheduler.generation () && scheduler.state () == FilterScheduler::State::preparing)
      invalidate ();
    return current && generation == scheduler.generation () && scheduler.state () == FilterScheduler::State::preparing;
  }
  static void tune_budget (std::size_t count, gint64 elapsed, std::size_t& budget)
  {
    /* Cost depends on the lower graph, not just pixel count. Begin with a
     * small probe; only grow after a cheap measured sample, and damp changes.
     * This is cooperative latency control, not a hard real-time guarantee. */
    if (elapsed > 4000) budget = std::max (std::size_t (1024), count / 2);
    else if (elapsed < 2000) budget = std::min (std::size_t (FilterScheduler::pixel_budget), budget * 2);
  }
  bool gray () const
  { return gimp_drawable_get_base_type (GIMP_DRAWABLE (owner)) == GIMP_GRAY; }
  bool real_samples () const
  { return gimp_drawable_get_precision (GIMP_DRAWABLE (owner)) != GIMP_PRECISION_U8_NON_LINEAR; }
  const Babl *encoded_format () const
  {
    if (real_samples ())
      return gimp_babl_format (gray () ? GIMP_GRAY : GIMP_RGB,
        gimp_babl_precision (GIMP_COMPONENT_TYPE_DOUBLE, gimp_drawable_get_trc (GIMP_DRAWABLE (owner))), TRUE,
        babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (owner))));
    /* Legacy plug-ins calculate on encoded native bytes, not a conversion to
     * default sRGB. Input and import must use the same drawable color space. */
    return babl_format_with_space (gray () ? "Y'A u8" : "R'G'B'A u8",
      babl_format_get_space (gimp_drawable_get_format (GIMP_DRAWABLE (owner))));
  }
  GeglRectangle rectangle (std::size_t offset, std::size_t count)
  {
    const int width = gimp_item_get_width (GIMP_ITEM (owner));
    const int x = offset % width, y = offset / width;
    return { x, y, int (count < std::size_t (width) ? count : width),
             int (count < std::size_t (width) ? 1 : count / width) };
  }
  void publish_buffer (GeglBuffer *completed, std::uint64_t token, bool flush)
  {
    if (token != scheduler.generation () || scheduler.state () == FilterScheduler::State::closed) return;
    /* Swap a fully imported buffer. No partial result is visible to the
     * drawable source, save code, projection, or an upper FilterLayer. */
    gimp_drawable_set_buffer_full (GIMP_DRAWABLE (owner), FALSE, nullptr, completed, nullptr, FALSE);
    /* Buffer notification may synchronously close the image or edit the
     * definition. Never publish a later drawable update for that token. */
    if (scheduler.state () == FilterScheduler::State::closed || token != scheduler.generation ()) return;
    gimp_drawable_update (GIMP_DRAWABLE (owner), 0, 0,
                          gimp_item_get_width (GIMP_ITEM (owner)), gimp_item_get_height (GIMP_ITEM (owner)));
    if (scheduler.state () == FilterScheduler::State::closed || token != scheduler.generation ()) return;
    /* Asynchronous completion also flushes the image, scheduling chunked
     * projection without waiting. Synchronous Undo uses its caller's flush. */
    if (flush)
      {
        auto image = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (owner))));
        if (image) gimp_image_flush (GIMP_IMAGE (image.get ()));
      }
  }
  bool step ()
  {
    /* Include every early return, callback and local destructor. Recording
     * before state-change emission hid synchronous observer/teardown cost. The
     * BindingStore borrow keeps this implementation alive through scope exit. */
    struct QuantumTimer
    {
      gint64& maximum;
      gint64 started = g_get_monotonic_time ();
      ~QuantumTimer () noexcept { maximum = std::max (maximum, g_get_monotonic_time () - started); }
    } timer { maximum_quantum_us };
    if (topology_dirty) refresh_connections ();
    if (!gimp_item_is_attached (GIMP_ITEM (owner)) || gimp_item_is_removed (GIMP_ITEM (owner)) ||
        !gimp_item_get_visible (GIMP_ITEM (owner)))
      {
        native_context.reset ();
        const bool again = scheduler.step (false, {}, {}, {});
        if (scheduler.state () != FilterScheduler::State::importing) staged.reset ();
        return again;
      }
    const auto previous = scheduler.state ();
    if ((previous == FilterScheduler::State::waiting || previous == FilterScheduler::State::cancelling) &&
        scheduler.uses_spool ())
      {
        /* This is the expanded, trusted application swap setting, not an XCF
         * path argument or an implicit /tmp (which may be RAM-backed). The
         * worker copies it at admission; no owner-side file I/O is performed. */
        gchar *directory = nullptr;
        g_object_get (gegl_config (), "swap", &directory, nullptr);
        std::unique_ptr<gchar, decltype (&g_free)> guard (directory, g_free);
        scheduler.set_spool_directory (directory ? directory : "");
        if (native_process_options)
          {
            preparing_process_directory = directory ? directory : "";
            if (!directory || !*directory || !g_path_is_absolute (directory))
              scheduler.reject ("Bundled PDB Filter execution requires configured file swap; definition and cache are retained");
          }
      }
    if (auto* image = gimp_item_get_image (GIMP_ITEM (owner))) {
      try { scheduler.set_admission (filter_admission_for_config (G_OBJECT (image->gimp->config))); }
      catch (const std::exception& error) {
        if (scheduler.state () != FilterScheduler::State::failed) scheduler.reject (error.what ());
        return scheduler.step (false, {}, {}, {});
      }
    }
    scheduler.set_pixel_budget (previous == FilterScheduler::State::importing ? import_budget : read_budget);
    /* Legacy RGB/Gray U8 nonlinear stays on the byte oracle. Other native
     * RGB/Gray precisions/TRCs use the documented double extension. Indexed
     * palette semantics cannot be represented by an independent RGBA cache. */
    if (previous != FilterScheduler::State::clean && previous != FilterScheduler::State::failed &&
        gimp_drawable_get_base_type (GIMP_DRAWABLE (owner)) != GIMP_RGB && !gray ())
      scheduler.reject ("Filter execution requires RGB or Gray; indexed definition and cache are retained");
    bool failed = false;
    graph_recheck = false;
    const bool dependencies_ready = ready (failed);
    if (scheduler.state () == FilterScheduler::State::closed) return false;
    if (failed) scheduler.reject ("Filter dependency failed, is cyclic, or exceeds the graph limit");
    if (dependencies_ready && scheduler.state () == FilterScheduler::State::waiting && scheduler.has_processor ())
      {
        const auto preparing_started = g_get_monotonic_time ();
        const bool prepared_one = prepare_graph_quantum ();
        maximum_graph_us = std::max (maximum_graph_us, g_get_monotonic_time () - preparing_started);
        if (prepared_one)
          {
            return scheduler.state () != FilterScheduler::State::closed;
          }
      }
    if (scheduler.state () == FilterScheduler::State::closed) return false;
    bool again = scheduler.step (dependencies_ready,
      [&] (std::size_t offset, std::size_t count, FilterScheduler::Bytes& input) {
        const auto read_started = g_get_monotonic_time ();
        const auto generation = scheduler.generation ();
        auto image = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (owner))));
        if (!image) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Layer image was closed");
        auto container = stack.lock ();
        if (!container) throw Error (GIMP_PAINTER_ERROR_CLOSED, "Layer stack was removed");
        /* Build the existing stack graph outside operator evaluation, then
         * sample only this layer's input proxy: precisely the lower stack. */
        gimp_filter_stack_get_graph (GIMP_FILTER_STACK (container.get ()));
        /* Node construction can emit callbacks or resolve a pending reference.
         * Do not evaluate a graph exposed by such a change until revalidated. */
        if (!input_current (generation)) return;
        GeglNode *node = gimp_filter_get_node (GIMP_FILTER (owner));
        if (!input_current (generation)) return;
        GeglNode *below_node = gegl_node_get_input_proxy (node, "input");
        auto rect = rectangle (offset, count);
        rect.x += gimp_item_get_offset_x (GIMP_ITEM (owner));
        rect.y += gimp_item_get_offset_y (GIMP_ITEM (owner));
        const auto size = input.size ();
        const bool real = real_samples ();
        input.resize (size + count * (real ? 32 : 4));
        if (real)
          {
            /* Aligned staging avoids treating byte-vector storage as live
             * double objects. Gray channels are replicated without conversion. */
            std::vector<double> native (count * (gray () ? 2 : 4));
            gegl_node_blit (below_node,1.0,&rect,encoded_format (),native.data (),GEGL_AUTO_ROWSTRIDE,GEGL_BLIT_CACHE);
            if (gray ())
              for (std::size_t i = 0; i < count; ++i)
                {
                  const double rgba[] = {native[i*2],native[i*2],native[i*2],native[i*2+1]};
                  std::memcpy (input.data ()+size+i*32,rgba,32);
                }
            else std::memcpy (input.data ()+size,native.data (),count*32);
          }
        else if (gray ())
          {
            /* Gray is one encoded channel, not RGB luminance. Replication lets
             * the channel-independent legacy kernels retain their exact byte
             * arithmetic without introducing an ICC RGB/Gray conversion. */
            std::vector<std::uint8_t> native (count * 2);
            gegl_node_blit (below_node, 1.0, &rect, encoded_format (), native.data (),
                            GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_CACHE);
            for (std::size_t i = 0; i < count; ++i)
              {
                input[size + i * 4] = input[size + i * 4 + 1] = input[size + i * 4 + 2] = native[i * 2];
                input[size + i * 4 + 3] = native[i * 2 + 1];
              }
          }
        else
          gegl_node_blit (below_node, 1.0, &rect, encoded_format (), input.data () + size,
                          GEGL_AUTO_ROWSTRIDE, GEGL_BLIT_CACHE);
        if (!input_current (generation)) return;
        if (native_process_options && !offset)
          {
            /* The old worker is gone before first read. This is the exact
             * directory copied into this admitted spool, not a later setting. */
            native_process_options->temporary_directory = preparing_process_directory;
          }
        if (native_procedure)
          native_context.capture_input (GIMP_DRAWABLE (owner), offset, count, input.data () + size);
        maximum_read_us = std::max (maximum_read_us, g_get_monotonic_time () - read_started);
        tune_budget (count, g_get_monotonic_time () - read_started, read_budget);
      },
      [&] (std::size_t offset, std::size_t count, const std::uint8_t *pixels) {
        const auto import_started = g_get_monotonic_time ();
        std::vector<std::uint8_t> merged;
        if (native_procedure)
          { native_context.merge_chunk (offset, count, pixels, merged); pixels = merged.data (); }
        if (!offset)
          {
            GeglRectangle extent {0, 0, gimp_item_get_width (GIMP_ITEM (owner)), gimp_item_get_height (GIMP_ITEM (owner))};
            staged = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_new (&extent, gimp_drawable_get_format (GIMP_DRAWABLE (owner)))));
          }
        auto rect = rectangle (offset, count);
        if (real_samples ())
          {
            std::vector<double> native (count * (gray () ? 2 : 4));
            if (gray ())
              for (std::size_t i = 0; i < count; ++i)
                {
                  double rgba[4]; std::memcpy (rgba,pixels+i*32,32);
                  if (rgba[0] != rgba[1] || rgba[0] != rgba[2])
                    throw Error (GIMP_PAINTER_ERROR_INVALID_STATE,"Native Gray executor returned unequal channels");
                  native[i*2]=rgba[0]; native[i*2+1]=rgba[3];
                }
            else std::memcpy (native.data (),pixels,count*32);
            gegl_buffer_set (GEGL_BUFFER (staged.get ()),&rect,0,encoded_format (),native.data (),GEGL_AUTO_ROWSTRIDE);
          }
        else if (gray ())
          {
            std::vector<std::uint8_t> native (count * 2);
            for (std::size_t i = 0; i < count; ++i)
              {
                if (pixels[i * 4] != pixels[i * 4 + 1] || pixels[i * 4] != pixels[i * 4 + 2])
                  throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Legacy Gray executor returned unequal channels");
                native[i * 2] = pixels[i * 4];
                native[i * 2 + 1] = pixels[i * 4 + 3];
              }
            gegl_buffer_set (GEGL_BUFFER (staged.get ()), &rect, 0, encoded_format (), native.data (), GEGL_AUTO_ROWSTRIDE);
          }
        else
          gegl_buffer_set (GEGL_BUFFER (staged.get ()), &rect, 0, encoded_format (), pixels, GEGL_AUTO_ROWSTRIDE);
        tune_budget (count, g_get_monotonic_time () - import_started, import_budget);
      },
      [&] (std::uint64_t token) {
        if (token != scheduler.generation ()) return;
        native_context.reset ();
        auto completed = std::move (staged);
        publish_buffer (GEGL_BUFFER (completed.get ()), token, true);
      },
      native_procedure ? FilterScheduler::Gate ([&] {
        refresh_context_selection ();
        if (!native_context.before_process (GIMP_DRAWABLE (owner), *native_procedure, read_budget)) return false;
        /* Retinex's native entry rejects an outside or narrower-than-16 ROI.
         * Validate the captured start rectangle before launching any helper. */
        if (native_procedure->procedure == FilterProcedure::retinex) native_procedure->bytes ();
        native_outcome->reset ();
        return true;
      }) : FilterScheduler::Gate (),
      native_procedure ? FilterScheduler::Gate ([&] {
        refresh_context_selection ();
        return native_context.before_import (GIMP_DRAWABLE (owner), native_outcome->disposition (), import_budget);
      }) : FilterScheduler::Gate ());
    if (scheduler.state () == FilterScheduler::State::closed) return false;
    if (scheduler.state () != FilterScheduler::State::importing) staged.reset ();
    if (scheduler.state () == FilterScheduler::State::clean || scheduler.state () == FilterScheduler::State::failed)
      native_context.reset ();
    if (previous != scheduler.state ()) g_signal_emit_by_name (owner, "filter-state-changed");
    return again || graph_recheck;
  }
  GimpFilterLayer *owner;
  std::string procedure;
  BytesRef raw, opaque_arguments;
  std::shared_ptr<const FilterArguments> args;
  std::uint64_t definition_revision = 0;
  FilterScheduler scheduler;
  std::shared_ptr<FilterProcedureRequest> native_procedure;
  std::shared_ptr<FilterProcessOptions> native_process_options;
  std::shared_ptr<FilterProcedureResult> native_outcome;
  FilterOwnerContext native_context;
  std::string preparing_process_directory;
  FairDispatcher::Ticket pending;
  WeakRef<GObject> stack, context_selection;
  ObjectRef<GObject> staged;
  std::vector<Connection> own_connections, dependencies, graph_connections;
  std::vector<FilterDependency> graph_filters;
  std::vector<CloneDependency> graph_clones;
  std::vector<WeakRef<GObject>> graph_nodes;
  std::size_t graph_node_cursor = 0, graph_fish_cursor = 0;
  bool graph_valid = false, graph_recheck = false;
  Connection image_connection, profile_connection, config_connection, selection_connection;
  std::vector<WeakRef<GObject>> lower_objects;
  bool topology_dirty = false;
  gint64 maximum_quantum_us = 0, maximum_graph_us = 0, maximum_read_us = 0;
  std::size_t read_budget = 1024, import_budget = FilterScheduler::pixel_budget;
};
void topology_changed (GimpContainer *container, GimpObject *, gpointer data)
{ visit (data, [&] (FilterImpl& impl) {
    if (container != impl.current_stack ()) impl.invalidate ();
    impl.structure_changed ();
  }); }
void reordered (GimpContainer *container, GimpObject *, gint, gint, gpointer data)
{ topology_changed (container, nullptr, data); }
void dependency_update (GimpDrawable *source, gint, gint, gint, gint, gpointer data)
{ visit (data, [&] (FilterImpl& impl) { if (impl.below (GIMP_ITEM (source))) impl.dependency_dirty (); }); }
void dependency_active (GimpFilter *source, gpointer data)
{ visit (data, [&] (FilterImpl& impl) { if (impl.below (GIMP_ITEM (source))) impl.dependency_dirty (); }); }
void dependency_status (GimpFilterLayer *source, gpointer data)
{ visit (data, [&] (FilterImpl& impl) {
    if (impl.below (GIMP_ITEM (source)))
      {
        const auto state = gimp_filter_layer_get_state (source);
        if (state == GIMP_FILTER_LAYER_WAITING || state == GIMP_FILTER_LAYER_CANCELLING) impl.dependency_dirty ();
        else impl.schedule ();
      }
  }); }
void graph_status (GimpFilterLayer *source, gpointer data)
{ visit (data, [&] (FilterImpl& impl) {
    const auto state = gimp_filter_layer_get_state (source);
    if (state == GIMP_FILTER_LAYER_WAITING || state == GIMP_FILTER_LAYER_CANCELLING) impl.dependency_dirty ();
    else impl.schedule ();
  }); }
void graph_update (GimpDrawable *, gint, gint, gint, gint, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.dependency_dirty (); }); }
void owner_ancestry (GimpViewable *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.structure_changed (); }); }
void owner_removed (GimpItem *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void owner_size (GimpViewable *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.configure (); }); }
void filter_budget_changed (GObject *, GParamSpec *, gpointer data)
{ visit (data, [] (FilterImpl& impl) {
    if (impl.scheduler.state () != FilterScheduler::State::clean) impl.invalidate ();
  }); }
void owner_offset (GObject *, GParamSpec *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void owner_active (GimpFilter *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void owner_image_changed (GObject *, GParamSpec *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.watch_image (); impl.structure_changed (); impl.invalidate (); }); }
void owner_format_changed (GimpDrawable *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.configure (); }); }
void image_profile_changed (GObject *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.invalidate (); }); }
void selection_invalidated (GimpImage *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { impl.native_context.selection_changed (); }); }
void image_disconnected (GimpObject *, gpointer data)
{ visit (data, [] (FilterImpl& impl) { gimp_painter_binding_close (G_OBJECT (impl.owner), nullptr); }); }
struct FilterUndoImpl
{
  ObjectRef<GObject> buffer;
  bool cache_complete = false;
  void close () noexcept { raw.reset (); opaque_arguments.reset (); args.reset (); buffer.reset (); }
  std::string procedure;
  BytesRef raw, opaque_arguments;
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
          snapshot.opaque_arguments = BytesRef (impl.opaque_arguments ? g_bytes_ref (impl.opaque_arguments.get ()) : nullptr);
          GeglBuffer *buffer = gimp_drawable_get_buffer (GIMP_DRAWABLE (impl.owner));
          if (!buffer) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter has no committed cache");
          snapshot.buffer = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_dup (buffer)));
          snapshot.cache_complete = impl.scheduler.snapshot ().cache_complete;
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
        auto current = ObjectRef<GObject>::adopt (G_OBJECT (gegl_buffer_dup (gimp_drawable_get_buffer (GIMP_DRAWABLE (impl.owner)))));
        impl.definition_installed ();
        auto restored = std::move (snapshot.buffer);
        const bool complete = snapshot.cache_complete;
        snapshot.buffer = std::move (current);
        snapshot.cache_complete = impl.scheduler.snapshot ().cache_complete;
        impl.procedure.swap (snapshot.procedure);
        impl.raw.swap (snapshot.raw);
        impl.args.swap (snapshot.args);
        impl.opaque_arguments.swap (snapshot.opaque_arguments);
        impl.publish_definition (GEGL_BUFFER (restored.get ()), complete);
      });
    });
  });
}
gint64 undo_memsize (GimpObject *object, gint64 *gui)
{
  const auto retained = boundary<gint64> (nullptr, 0, [&] {
    auto *store = BindingStore::find (G_OBJECT (object));
    if (!store || store->state () != BindingStore::State::active) return gint64 (0);
    return store->read<FilterUndoSlot> ([] (const FilterUndoImpl& snapshot) {
      const std::uint64_t limit = G_MAXINT64;
      std::uint64_t size = sizeof (snapshot);
      const auto add = [&] (std::uint64_t bytes) { size = bytes > limit - size ? limit : size + bytes; };
      add (snapshot.procedure.capacity ());
      if (snapshot.raw) add (g_bytes_get_size (snapshot.raw.get ()));
      if (snapshot.opaque_arguments) add (g_bytes_get_size (snapshot.opaque_arguments.get ()));
      if (snapshot.buffer)
        {
          auto *buffer = GEGL_BUFFER (snapshot.buffer.get ());
          std::uint64_t area = gegl_buffer_get_width (buffer);
          const std::uint64_t height = gegl_buffer_get_height (buffer), bpp = babl_format_get_bytes_per_pixel (gegl_buffer_get_format (buffer));
          area = height && area > limit / height ? limit : area * height;
          add (bpp && area > limit / bpp ? limit : area * bpp);
        }
      return gint64 (size);
    });
  });
  const auto base = GIMP_OBJECT_CLASS (gimp_filter_layer_undo_parent_class)->get_memsize (object, gui);
  return retained > G_MAXINT64 - base ? G_MAXINT64 : retained + base;
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
    case 5: g_value_take_boxed (value, gimp_filter_layer_ref_opaque_arguments (layer)); break;
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
          std::string name = impl.procedure;
          BytesRef raw (impl.raw ? g_bytes_ref (impl.raw.get ()) : nullptr);
          BytesRef opaque (impl.opaque_arguments ? g_bytes_ref (impl.opaque_arguments.get ()) : nullptr);
          auto arguments = impl.args;
          const auto token = BindingStore::require (G_OBJECT (copy)).generation ();
          const auto cache = impl.scheduler.snapshot ();
          duplicate_impl.definition_installed ();
          const auto revision = duplicate_impl.definition_revision;
          /* Publish all fields before retired payload deleters can reenter. */
          duplicate_impl.procedure.swap (name);
          duplicate_impl.raw.swap (raw);
          duplicate_impl.opaque_arguments.swap (opaque);
          duplicate_impl.args.swap (arguments);
          duplicate_impl.attach ();
          if (!duplicate_impl.definition_current (token, revision))
            throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter duplicate changed during attachment");
          duplicate_impl.configure ();
          if (!duplicate_impl.definition_current (token, revision))
            throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter duplicate changed during configuration");
          duplicate_impl.scheduler.restore_cache (cache);
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
  GIMP_OBJECT_CLASS (klass)->get_memsize = undo_memsize;
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
  g_object_class_install_property (object, 5,
    g_param_spec_boxed ("filter-opaque-arguments", "Opaque arguments", "Unavailable converted argument model, retained uninterpreted",
                        G_TYPE_BYTES, G_PARAM_READABLE));
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
                                const std::shared_ptr<const FilterArguments> *imported = nullptr,
                                GBytes *opaque_arguments = nullptr)
{
  return boundary<gboolean> (error, FALSE, [&] () -> gboolean {
    if (!GIMP_IS_FILTER_LAYER (layer)) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected FilterLayer");
    auto owner_pin = ObjectRef<GObject>::retain (G_OBJECT (layer));
    auto image_pin = ObjectRef<GObject>::retain (G_OBJECT (gimp_item_get_image (GIMP_ITEM (layer))));
    std::string name = procedure ? procedure : "";
    BytesRef bytes (raw ? g_bytes_ref (raw) : nullptr);
    BytesRef opaque (opaque_arguments ? g_bytes_ref (opaque_arguments) : nullptr);
    std::shared_ptr<const FilterArguments> arguments = imported ? *imported :
                                                       args ? std::make_shared<FilterArguments> (args) : nullptr;
    BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([&] (FilterImpl& impl) {
      const auto binding_token = BindingStore::require (G_OBJECT (layer)).generation ();
      const auto revision = impl.definition_revision;
      if (impl.definition_revision == std::numeric_limits<std::uint64_t>::max ())
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter definition revision exhausted");
      if (push_undo && gimp_item_is_attached (GIMP_ITEM (layer)))
        {
          auto *undo = gimp_image_undo_push (gimp_item_get_image (GIMP_ITEM (layer)),
                                            gimp_filter_layer_undo_get_type (), GIMP_UNDO_FILTER_LAYER_DEFINITION,
                                            "Filter layer definition", GimpDirtyMask (GIMP_DIRTY_ITEM | GIMP_DIRTY_ITEM_META | GIMP_DIRTY_DRAWABLE),
                                            "item", layer, nullptr);
          if (undo && reinterpret_cast<GimpFilterLayerUndo *> (undo)->binding_failed)
            throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter definition Undo construction failed");
        }
      if (!impl.definition_current (binding_token, revision))
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter definition target changed during Undo creation");
      impl.definition_installed ();
      const auto installed_revision = impl.definition_revision;
      /* Byte payloads can have caller-supplied destruction callbacks. Keep
       * the retired definition alive until the whole replacement is published. */
      impl.procedure.swap (name); impl.raw.swap (bytes);
      impl.args.swap (arguments); impl.opaque_arguments.swap (opaque);
      impl.attach ();
      if (!impl.definition_current (binding_token, installed_revision) || !impl.publish_definition ())
        throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "Filter definition target changed during notification");
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
gboolean gimp_filter_layer_set_definition_with_opaque_arguments (GimpFilterLayer *layer, const gchar *procedure,
                                                                GBytes *raw, GBytes *opaque, GError **error)
{ return set_definition (layer, procedure, raw, nullptr, false, error, nullptr, opaque); }
#define FILTER_READ(type, fallback, expression) \
  return boundary<type> (nullptr, fallback, [&] { \
    if (!GIMP_IS_FILTER_LAYER (layer)) return fallback; \
    return BindingStore::require (G_OBJECT (layer)).read<FilterSlot> ([&] (const FilterImpl& impl) -> type { return expression; }); \
  })
gchar *gimp_filter_layer_dup_procedure (GimpFilterLayer *layer)
{ FILTER_READ (gchar *, static_cast<gchar *> (nullptr), g_strdup (impl.procedure.c_str ())); }
GBytes *gimp_filter_layer_ref_definition (GimpFilterLayer *layer)
{ FILTER_READ (GBytes *, static_cast<GBytes *> (nullptr), impl.raw ? g_bytes_ref (impl.raw.get ()) : nullptr); }
GBytes *gimp_filter_layer_ref_opaque_arguments (GimpFilterLayer *layer)
{ FILTER_READ (GBytes *, static_cast<GBytes *> (nullptr), impl.opaque_arguments ? g_bytes_ref (impl.opaque_arguments.get ()) : nullptr); }
GimpValueArray *gimp_filter_layer_dup_args (GimpFilterLayer *layer)
{ FILTER_READ (GimpValueArray *, static_cast<GimpValueArray *> (nullptr), impl.args ? impl.args->copy_values () : nullptr); }
GimpFilterLayerState gimp_filter_layer_get_state (GimpFilterLayer *layer)
{ FILTER_READ (GimpFilterLayerState, GIMP_FILTER_LAYER_CLOSED, static_cast<GimpFilterLayerState> (impl.scheduler.state ())); }
gchar *gimp_filter_layer_dup_error (GimpFilterLayer *layer)
{ FILTER_READ (gchar *, static_cast<gchar *> (nullptr), g_strdup (impl.scheduler.error ().c_str ())); }
guint64 gimp_filter_layer_get_definition_revision (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.definition_revision); }
guint64 gimp_filter_layer_get_generation (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.scheduler.generation ()); }
guint64 gimp_filter_layer_get_cache_generation (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.scheduler.cache_generation ()); }
guint64 gimp_filter_layer_get_run_count (GimpFilterLayer *layer)
{ FILTER_READ (guint64, guint64 (0), impl.scheduler.starts ()); }
gint64 gimp_filter_layer_get_max_quantum_us (GimpFilterLayer *layer)
{ FILTER_READ (gint64, gint64 (0), impl.maximum_quantum_us); }
gint64 gimp_filter_layer_get_max_graph_quantum_us (GimpFilterLayer *layer)
{ FILTER_READ (gint64, gint64 (0), impl.maximum_graph_us); }
gint64 gimp_filter_layer_get_max_read_quantum_us (GimpFilterLayer *layer)
{ FILTER_READ (gint64, gint64 (0), impl.maximum_read_us); }
#undef FILTER_READ
void gimp_filter_layer_mark_as_loaded (GimpFilterLayer *layer)
{ boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([] (FilterImpl& impl) {
    impl.native_context.reset (); impl.scheduler.mark_loaded (); impl.staged.reset (); }); }); }
void gimp_filter_layer_invalidate (GimpFilterLayer *layer)
{ boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([] (FilterImpl& impl) { impl.invalidate (); }); }); }
void gimp_filter_layer_cancel (GimpFilterLayer *layer)
{
  boundary_void (nullptr, [&] {
    if (!GIMP_IS_FILTER_LAYER (layer)) return;
    BindingStore::require (G_OBJECT (layer)).with<FilterSlot> ([] (FilterImpl& impl) {
      /* Detach before callbacks can replace or close this generation. The
       * completed drawable buffer is independent from the staged import. */
      auto retired = std::move (impl.staged);
      impl.native_context.reset ();
      impl.scheduler.reject ("Filter execution cancelled");
      impl.schedule ();
      g_signal_emit_by_name (impl.owner, "filter-state-changed");
      /* No further Impl access: signal or retired-buffer destruction may
       * synchronously close the owner or install a replacement definition. */
    });
  });
}

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
      impl.native_context.reset ();
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
const GValue *gimp_filter_arguments_snapshot_peek_value (const GimpFilterArgumentsSnapshot *snapshot, guint argument)
{
  return boundary<const GValue *> (nullptr, nullptr, [&] () -> const GValue * {
    if (!snapshot || argument >= snapshot->arguments->size () ||
        !snapshot->arguments->scalar (argument)) return nullptr;
    return snapshot->arguments->at (argument);
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
